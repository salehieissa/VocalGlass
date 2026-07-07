#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

//==============================================================================
// ClipEngine — an oversampled soft clipper.
//
// Signal path: input drive -> 4x oversampled waveshaper (soft / warm / hard
// curve, scaled to the ceiling) -> equal-gain dry/wet mix -> output trim.
// The shaper is normalised so a full-scale sine at 0 dB drive passes through
// the SOFT curve nearly untouched; drive pushes material into the curve.
//
// A visual tap peak-holds the pre-clip and post-clip waveforms into small
// slot rings so the editor can draw in/out waves and the live clip amount.
//==============================================================================
class ClipEngine
{
public:
    enum Shape { Soft = 0, Warm, Hard };

    struct Params
    {
        float driveDb   = 6.0f;    // 0..24
        int   shape     = Soft;
        float ceilingDb = -0.3f;   // -12..0
        float mix       = 1.0f;    // 0..1
        float outDb     = 0.0f;    // -12..+12
        bool  hq        = true;    // 4x oversampling
    };

    void prepare (double sampleRate, int maxBlock, int numChannels)
    {
        sr = sampleRate;
        channels = juce::jmax (1, numChannels);
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) channels, 2 /* 2^2 = 4x */,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        oversampler->initProcessing ((size_t) maxBlock);
        dryBuffer.setSize (channels, maxBlock);
        reset();
    }

    void reset()
    {
        if (oversampler) oversampler->reset();
        visWrite = 0;
        visIn.fill (0.0f);
        visOut.fill (0.0f);
    }

    int latencySamples (bool hq) const
    {
        return (hq && oversampler) ? (int) oversampler->getLatencyInSamples() : 0;
    }

    void setParams (const Params& p) { params = p; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = juce::jmin (channels, buffer.getNumChannels());
        const int n     = buffer.getNumSamples();
        if (n == 0) return;

        const float drive   = juce::Decibels::decibelsToGain (params.driveDb);
        const float ceiling = juce::Decibels::decibelsToGain (params.ceilingDb);
        const float outGain = juce::Decibels::decibelsToGain (params.outDb);
        const float wet     = params.mix;

        // keep the dry signal for the mix
        for (int ch = 0; ch < numCh; ++ch)
            dryBuffer.copyFrom (ch, 0, buffer, ch, 0, n);

        float preClipPeak = 0.0f, postClipPeak = 0.0f;

        // visual tap density: one slot per ~256 samples of input
        const int slotLen = juce::jmax (64, n / 8);

        auto shapeSample = [this, ceiling] (float x)
        {
            // normalise into ceiling units so the curve knee tracks the ceiling
            const float u = x / ceiling;
            float y;
            switch (params.shape)
            {
                case Warm:  y = std::tanh (u); break;
                case Hard:
                {
                    // hard clamp with a small quadratic knee so the edge isn't
                    // a razor (continuous value + slope at both knee ends)
                    constexpr float knee = 0.1f;
                    const float a = std::abs (u);
                    const float s = u >= 0.0f ? 1.0f : -1.0f;
                    if (a <= 1.0f - knee)      y = u;
                    else if (a >= 1.0f + knee) y = s;
                    else
                    {
                        const float d2 = a - (1.0f - knee);
                        y = s * (a - d2 * d2 / (4.0f * knee));
                    }
                    break;
                }
                case Soft:
                default:
                {
                    // classic cubic: linear-ish below, rounds into the ceiling
                    const float a = juce::jlimit (-1.5f, 1.5f, u);
                    y = a - (a * a * a) / 6.75f;   // hits +-1.0 at |u| = 1.5
                    y = juce::jlimit (-1.0f, 1.0f, y);
                    break;
                }
            }
            return y * ceiling;
        };

        if (params.hq && oversampler)
        {
            // drive into the oversampled domain
            buffer.applyGain (0, n, drive);
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float* d = buffer.getReadPointer (ch);
                for (int i = 0; i < n; ++i) preClipPeak = juce::jmax (preClipPeak, std::abs (d[i]));
            }

            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(),
                                                (size_t) numCh, (size_t) n);
            auto osBlock = oversampler->processSamplesUp (block);
            for (size_t ch = 0; ch < osBlock.getNumChannels(); ++ch)
            {
                float* d = osBlock.getChannelPointer (ch);
                for (size_t i = 0; i < osBlock.getNumSamples(); ++i)
                    d[i] = shapeSample (d[i]);
            }
            oversampler->processSamplesDown (block);
        }
        else
        {
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = buffer.getWritePointer (ch);
                for (int i = 0; i < n; ++i)
                {
                    const float x = d[i] * drive;
                    preClipPeak = juce::jmax (preClipPeak, std::abs (x));
                    d[i] = shapeSample (x);
                }
            }
        }

        // mix + output trim, and the visual tap on the final wet signal
        int slotFill = visFill;
        for (int i = 0; i < n; ++i)
        {
            float inPk = 0.0f, outPk = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = buffer.getWritePointer (ch);
                const float dry = dryBuffer.getSample (ch, i);
                const float mixed = (dry + (d[i] - dry) * wet) * outGain;
                d[i] = mixed;
                inPk  = juce::jmax (inPk,  std::abs (dry * drive));
                outPk = juce::jmax (outPk, std::abs (mixed));
                postClipPeak = juce::jmax (postClipPeak, std::abs (mixed));
            }

            slotIn  = juce::jmax (slotIn,  inPk);
            slotOut = juce::jmax (slotOut, outPk);
            if (++slotFill >= slotLen)
            {
                const int w = visWrite.load (std::memory_order_relaxed);
                visIn [(size_t) w] = slotIn;
                visOut[(size_t) w] = slotOut;
                visWrite.store ((w + 1) % kVisSlots, std::memory_order_release);
                slotIn = slotOut = 0.0f;
                slotFill = 0;
            }
        }
        visFill = slotFill;

        // live "how hard am I clipping" readout (dB shaved off the peaks)
        const float pre  = juce::Decibels::gainToDecibels (preClipPeak + 1.0e-9f);
        const float post = juce::Decibels::gainToDecibels (
            juce::jmin (preClipPeak, ceiling) + 1.0e-9f);
        clipDb.store (juce::jmax (0.0f, pre - post));
        inDb.store  (juce::Decibels::gainToDecibels (preClipPeak  + 1.0e-9f));
        outDb.store (juce::Decibels::gainToDecibels (postClipPeak + 1.0e-9f));
    }

    // ---- visual tap (editor side) ----
    static constexpr int kVisSlots = 256;
    void copyVis (float* inDest, float* outDest, int count) const
    {
        const int w = visWrite.load (std::memory_order_acquire);
        for (int i = 0; i < count; ++i)
        {
            const int idx = (w + i * kVisSlots / count) % kVisSlots;
            inDest[i]  = visIn [(size_t) idx];
            outDest[i] = visOut[(size_t) idx];
        }
    }
    int visHead() const { return visWrite.load (std::memory_order_acquire); }

    std::atomic<float> inDb { -100.0f }, outDb { -100.0f }, clipDb { 0.0f };

private:
    Params params;
    double sr = 44100.0;
    int channels = 2;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> dryBuffer;

    std::array<float, kVisSlots> visIn {}, visOut {};
    std::atomic<int> visWrite { 0 };
    int visFill = 0;
    float slotIn = 0.0f, slotOut = 0.0f;
};
