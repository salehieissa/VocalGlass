#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>

//==============================================================================
// VocalChop engine — a host-synced beat repeater ("stutter") for vocals.
//
// A circular buffer continuously records the dry input. On every REFRESH
// boundary (a musical division of the host tempo) the engine captures a new
// slice start; the wet path then loops the most recent CHOP division of audio
// from that point until the next refresh. FREEZE stops refreshing so the
// current slice loops forever.
//
//   * CHOP    — loop length (1/4 .. 1/64, incl. dotted & triplet)
//   * REFRESH — how often a fresh slice is captured (1/1 .. 1/8)
//   * GATE    — per-repeat gate: shortens each repeat's sustain, hard chop feel
//   * FADE    — per-repeat decay: successive repeats get quieter (tape-stop-ish)
//   * MIX     — equal-power dry/wet
//
// Repeats are crossfaded at the loop seam (~4 ms) so no clicks. When the host
// gives no tempo we free-run at 120 BPM. Silence in, silence out — the wet
// path only replays captured input.
//==============================================================================
class ChopEngine
{
public:
    struct Params
    {
        double bpm        = 120.0;
        double chopBeats  = 0.5;    // loop length in quarter-note beats
        double refreshBeats = 1.0;  // capture interval in beats
        double hostPpq    = -1.0;   // host position in quarter notes (<0 = none)
        bool  hostPlaying = false;
        bool  freeze      = false;
        float gate        = 0.0f;   // 0..1
        float fade        = 0.0f;   // 0..1
        float mix         = 1.0f;   // 0..1
        float outDb       = 0.0f;   // -12..+12
    };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        // longest slice: 4 beats at 40 BPM = 6 s; buffer 8 s for headroom
        bufLen = juce::nextPowerOfTwo ((int) (sampleRate * 8.0));
        buffer.setSize (2, bufLen);
        buffer.clear();

        mixSmooth.reset (sampleRate, 0.02);
        outSmooth.reset (sampleRate, 0.03);

        writePos = 0;
        sliceStart = 0;
        phaseSamples = 0;
        repeatIndex = 0;
        hasSlice = false;
        xfadeLen = juce::jmax (16, (int) (sampleRate * 0.004));
    }

    void reset()
    {
        buffer.clear();
        writePos = 0;
        sliceStart = 0;
        phaseSamples = 0;
        repeatIndex = 0;
        hasSlice = false;
    }

    void setParams (const Params& p)
    {
        params = p;
        mixSmooth.setTargetValue (p.mix);
        outSmooth.setTargetValue (juce::Decibels::decibelsToGain (p.outDb));

        const double spb = 60.0 / juce::jlimit (40.0, 300.0, p.bpm) * sampleRate;
        chopLen    = juce::jlimit (64, bufLen / 2, (int) std::round (spb * p.chopBeats));
        refreshLen = juce::jlimit (chopLen, bufLen / 2, (int) std::round (spb * p.refreshBeats));

        // Phase-lock to the host beat grid while the transport runs, so the
        // refresh cycle starts on the division boundary, not on plugin load.
        // A small tolerance avoids re-seeking on ordinary block jitter.
        if (p.hostPlaying && p.hostPpq >= 0.0)
        {
            const double beatsIn = std::fmod (p.hostPpq, p.refreshBeats);
            const int target = juce::jlimit (0, refreshLen - 1,
                                             (int) (beatsIn / p.refreshBeats * refreshLen));
            if (std::abs (target - phaseSamples) > juce::jmax (64, refreshLen / 32))
            {
                if (target < phaseSamples)   // we jumped back across a boundary
                    forceCapture = true;
                phaseSamples = target;
            }
        }
    }

    // Current repeat index (for the editor's activity display).
    int getRepeatIndex() const { return repeatIndex.load(); }

    //==========================================================================
    // Visual tap for the editor's chop display. The refresh cycle is split
    // into kVisSlots bins; each bin stores the peak of the WET signal (after
    // gate/fade shaping, before mix) so the display shows exactly what the
    // chopper is doing. Bins are overwritten progressively as the playhead
    // sweeps, so the previous cycle stays visible ahead of the playhead.
    static constexpr int kVisSlots = 256;

    float getVisPhase() const   { return visPhase.load(); }    // 0..1 in cycle
    float getVisChopFrac() const { return visChopFrac.load(); } // chop/refresh

    void copyVis (float* dest, int n) const
    {
        for (int i = 0; i < n && i < kVisSlots; ++i)
            dest[i] = visWet[(size_t) i].load (std::memory_order_relaxed);
    }

    void process (juce::AudioBuffer<float>& audio)
    {
        const int numSamples = audio.getNumSamples();
        const int numCh = juce::jmin (2, audio.getNumChannels());
        if (numCh < 1 || numSamples == 0) return;

        visPhase.store ((float) phaseSamples / (float) refreshLen);
        visChopFrac.store ((float) chopLen / (float) refreshLen);

        auto* wr0 = buffer.getWritePointer (0);
        auto* wr1 = buffer.getWritePointer (1);

        for (int n = 0; n < numSamples; ++n)
        {
            const float inL = audio.getSample (0, n);
            const float inR = numCh > 1 ? audio.getSample (1, n) : inL;

            // record dry input
            wr0[writePos] = inL;
            wr1[writePos] = inR;

            // refresh boundary: capture a fresh slice (unless frozen)
            if (phaseSamples == 0 || forceCapture)
            {
                if (! (params.freeze && hasSlice))
                {
                    sliceStart = writePos;
                    hasSlice = true;
                    repeatIndex.store (0);
                }
                forceCapture = false;
            }

            // wet: loop the captured slice in CHOP-length repeats
            float wetL = 0.0f, wetR = 0.0f;
            if (hasSlice)
            {
                const int rep = phaseSamples / chopLen;
                const int pos = phaseSamples % chopLen;
                repeatIndex.store (rep);

                const int idx = (sliceStart + pos) & (bufLen - 1);
                wetL = wr0[idx];
                wetR = wr1[idx];

                // seam crossfade: blend loop end into loop start
                if (pos >= chopLen - xfadeLen && chopLen > xfadeLen * 2)
                {
                    const float t = (float) (pos - (chopLen - xfadeLen)) / (float) xfadeLen;
                    const int idx0 = (sliceStart + (pos - chopLen + xfadeLen) % chopLen) & (bufLen - 1);
                    wetL = wetL * (1.0f - t) + wr0[idx0] * t;
                    wetR = wetR * (1.0f - t) + wr1[idx0] * t;
                }

                // GATE: sustain shortens with the amount; short raised-cosine tail
                if (params.gate > 1.0e-3f)
                {
                    const float sustain = 1.0f - 0.85f * params.gate;   // fraction of repeat
                    const float posF = (float) pos / (float) chopLen;
                    if (posF > sustain)
                    {
                        const float relLen = juce::jmax (0.02f, 0.15f * (1.0f - params.gate));
                        const float rel = (posF - sustain) / relLen;
                        const float env = rel >= 1.0f ? 0.0f
                                        : 0.5f + 0.5f * std::cos (rel * juce::MathConstants<float>::pi);
                        wetL *= env; wetR *= env;
                    }
                }

                // FADE: each successive repeat decays
                if (params.fade > 1.0e-3f && rep > 0)
                {
                    const float g = std::pow (1.0f - 0.55f * params.fade, (float) rep);
                    wetL *= g; wetR *= g;
                }
            }

            // equal-power dry/wet + output trim
            const float mix = mixSmooth.getNextValue();
            const float wetGain = std::sin (mix * juce::MathConstants<float>::halfPi);
            const float dryGain = std::cos (mix * juce::MathConstants<float>::halfPi);
            const float out = outSmooth.getNextValue();

            audio.setSample (0, n, (inL * dryGain + wetL * wetGain) * out);
            if (numCh > 1)
                audio.setSample (1, n, (inR * dryGain + wetR * wetGain) * out);

            // visual tap: peak-hold the shaped wet signal into the slot bin
            {
                const int slot = juce::jlimit (0, kVisSlots - 1,
                                               (int) ((juce::int64) phaseSamples * kVisSlots / refreshLen));
                if (slot != visSlot)
                {
                    visSlot = slot;
                    visAccum = 0.0f;
                }
                visAccum = juce::jmax (visAccum, std::abs (wetL), std::abs (wetR));
                visWet[(size_t) slot].store (visAccum, std::memory_order_relaxed);
            }

            writePos = (writePos + 1) & (bufLen - 1);
            if (++phaseSamples >= refreshLen)
                phaseSamples = 0;
        }
    }

private:
    double sampleRate = 44100.0;
    Params params;

    juce::AudioBuffer<float> buffer;
    int bufLen = 0;
    int writePos = 0;
    int sliceStart = 0;
    int phaseSamples = 0;
    int chopLen = 4096, refreshLen = 8192, xfadeLen = 256;
    bool hasSlice = false;
    bool forceCapture = false;
    std::atomic<int> repeatIndex { 0 };

    // visual tap state
    std::array<std::atomic<float>, kVisSlots> visWet {};
    std::atomic<float> visPhase { 0.0f }, visChopFrac { 0.25f };
    int visSlot = -1;
    float visAccum = 0.0f;

    juce::SmoothedValue<float> mixSmooth, outSmooth;
};
