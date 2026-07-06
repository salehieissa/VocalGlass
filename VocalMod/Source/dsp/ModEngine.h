#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

//==============================================================================
// VocalMod engine — one modulation processor with three swappable voices:
//
//   * Chorus  — two modulated taps per channel (12 ms & 18 ms base, ±6 ms max)
//               on a Lagrange-interpolated delay line, light feedback allowed
//   * Flanger — single modulated tap (1.5 ms base, ±4 ms), feedback loop with
//               the tone low-pass inside the loop
//   * Phaser  — 6-stage first-order allpass cascade per channel, cutoff swept
//               200 Hz..4 kHz (log) by the LFO, feedback around the cascade
//
// Mode changes crossfade over ~30 ms so switching never clicks. Stereo width
// offsets the right channel's LFO phase by up to 90°. The wet path runs
// through a one-pole tone low-pass and is combined with an equal-power mix.
//==============================================================================
class ModEngine
{
public:
    enum Mode { Chorus = 0, Flanger, Phaser };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        // Longest tap is 18 ms + 6 ms modulation; leave generous headroom.
        const int maxSamples = (int) (sampleRate * 0.030) + 256;
        chorusLine.setMaximumDelayInSamples (maxSamples);
        chorusLine.prepare (spec);
        flangerLine.setMaximumDelayInSamples (maxSamples);
        flangerLine.prepare (spec);

        depthSmooth.reset (sampleRate, 0.05);
        fbSmooth.reset (sampleRate, 0.05);
        mixSmooth.reset (sampleRate, 0.02);
        widthSmooth.reset (sampleRate, 0.05);

        fadeLen = juce::jmax (1, (int) (sampleRate * 0.030)); // ~30 ms mode crossfade
        fadeLeft = 0;

        reset();
    }

    void reset()
    {
        chorusLine.reset();
        flangerLine.reset();
        for (int ch = 0; ch < 2; ++ch)
        {
            flangerFb[ch] = 0.0f;
            flangerLoopLP[ch] = 0.0f;
            phaserFb[ch] = 0.0f;
            toneState[ch] = 0.0f;
            for (int s = 0; s < kPhaserStages; ++s)
                apState[ch][s] = 0.0f;
        }
        lfoPhase = 0.0f;
    }

    struct Params
    {
        int   mode     = Chorus;
        float rateHz   = 0.5f;     // LFO rate
        float depth    = 0.5f;     // 0..1
        float feedback = 0.0f;     // -0.95..+0.95
        float mix      = 0.5f;     // 0..1
        float width    = 1.0f;     // 0..1 → right LFO offset up to 90°
        float toneHz   = 12000.0f; // wet-path low-pass
    };

    void setParams (const Params& p)
    {
        if (p.mode != curMode)
        {
            // Start a crossfade from the previous voice into the new one, and
            // clear the incoming voice's state so it starts from silence.
            prevMode = curMode;
            curMode  = p.mode;
            fadeLeft = fadeLen;
            resetModeState (curMode);
        }

        params = p;
        depthSmooth.setTargetValue (p.depth);
        fbSmooth.setTargetValue (p.feedback);
        mixSmooth.setTargetValue (p.mix);
        widthSmooth.setTargetValue (p.width);

        toneCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi
                                    * juce::jlimit (1000.0f, 20000.0f, p.toneHz)
                                    / (float) sampleRate);
        phaseInc = juce::MathConstants<float>::twoPi * p.rateHz / (float) sampleRate;
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh = juce::jmin (2, buffer.getNumChannels());
        if (numCh < 1) return;

        for (int n = 0; n < numSamples; ++n)
        {
            const float depth = depthSmooth.getNextValue();
            const float fb    = fbSmooth.getNextValue();
            const float mix   = mixSmooth.getNextValue();
            const float width = widthSmooth.getNextValue();

            // equal-power dry/wet gains
            const float wetGain = std::sin (mix * juce::MathConstants<float>::halfPi);
            const float dryGain = std::cos (mix * juce::MathConstants<float>::halfPi);

            const float fadeT = fadeLeft > 0
                                    ? 1.0f - (float) fadeLeft / (float) fadeLen
                                    : 1.0f;

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* data = buffer.getWritePointer (ch);
                const float in = data[n];

                // stereo width: right channel LFO leads by up to 90°
                const float phase = lfoPhase
                                  + (ch == 1 ? width * juce::MathConstants<float>::halfPi : 0.0f);

                float wet = renderVoice (curMode, ch, in, phase, depth, fb);
                if (fadeLeft > 0)
                {
                    const float old = renderVoice (prevMode, ch, in, phase, depth, fb);
                    wet = old * (1.0f - fadeT) + wet * fadeT;
                }

                // one-pole tone low-pass on the wet path only
                toneState[ch] += toneCoef * (wet - toneState[ch]);
                toneState[ch] = flushDenorm (toneState[ch]);

                data[n] = in * dryGain + toneState[ch] * wetGain;
            }

            if (fadeLeft > 0) --fadeLeft;

            lfoPhase += phaseInc;
            if (lfoPhase > juce::MathConstants<float>::twoPi)
                lfoPhase -= juce::MathConstants<float>::twoPi;
        }
    }

private:
    static constexpr int kPhaserStages = 6;

    //==========================================================================
    float renderVoice (int mode, int ch, float in, float phase, float depth, float fb)
    {
        switch (mode)
        {
            case Chorus:  return renderChorus  (ch, in, phase, depth, fb);
            case Flanger: return renderFlanger (ch, in, phase, depth, fb);
            case Phaser:  return renderPhaser  (ch, in, phase, depth, fb);
            default:      return 0.0f;
        }
    }

    // Two taps per channel: 12 ms & 18 ms base, modulated ±6 ms max, the second
    // tap's LFO in quadrature so the taps never collapse onto each other.
    float renderChorus (int ch, float in, float phase, float depth, float fb)
    {
        const float sr = (float) sampleRate;
        const float depthSamp = depth * 0.006f * sr;
        const float maxD = (float) chorusLine.getMaximumDelayInSamples() - 4.0f;

        const float d1 = juce::jlimit (4.0f, maxD, 0.012f * sr + std::sin (phase) * depthSamp);
        const float d2 = juce::jlimit (4.0f, maxD, 0.018f * sr
                             + std::sin (phase + juce::MathConstants<float>::halfPi) * depthSamp);

        const float t1 = chorusLine.popSample (ch, d1, false);
        const float t2 = chorusLine.popSample (ch, d2, true);
        const float wet = (t1 + t2) * 0.5f;

        // light feedback only — half strength keeps the chorus from ringing
        chorusLine.pushSample (ch, in + flushDenorm (wet * fb * 0.5f));
        return wet;
    }

    // Single tap, 1.5 ms base ± 4 ms, feedback loop with the tone LP inside it.
    float renderFlanger (int ch, float in, float phase, float depth, float fb)
    {
        const float sr = (float) sampleRate;
        const float maxD = (float) flangerLine.getMaximumDelayInSamples() - 4.0f;
        const float d = juce::jlimit (2.0f, maxD,
                                      0.0015f * sr + std::sin (phase) * depth * 0.004f * sr);

        const float t = flangerLine.popSample (ch, d, true);

        // tone low-pass inside the feedback loop so repeats darken naturally
        flangerLoopLP[ch] += toneCoef * (t - flangerLoopLP[ch]);
        flangerLoopLP[ch] = flushDenorm (flangerLoopLP[ch]);

        flangerLine.pushSample (ch, in + flushDenorm (flangerLoopLP[ch] * fb));
        return t;
    }

    // 6 first-order allpass stages, cutoff swept 200 Hz..4 kHz on a log curve,
    // feedback taken around the whole cascade.
    float renderPhaser (int ch, float in, float phase, float depth, float fb)
    {
        const float sweep = 0.5f + 0.5f * std::sin (phase) * depth;          // 0..1
        const float fc = 200.0f * std::pow (20.0f, sweep);                   // 200..4000 log
        const float g = std::tan (juce::MathConstants<float>::pi * fc / (float) sampleRate);
        const float a = (g - 1.0f) / (g + 1.0f);

        float x = in + phaserFb[ch] * fb;
        for (int s = 0; s < kPhaserStages; ++s)
        {
            const float y = a * x + apState[ch][s];
            apState[ch][s] = flushDenorm (x - a * y);
            x = y;
        }

        phaserFb[ch] = flushDenorm (x);
        return x;
    }

    //==========================================================================
    void resetModeState (int mode)
    {
        switch (mode)
        {
            case Chorus:
                chorusLine.reset();
                break;
            case Flanger:
                flangerLine.reset();
                flangerFb[0] = flangerFb[1] = 0.0f;
                flangerLoopLP[0] = flangerLoopLP[1] = 0.0f;
                break;
            case Phaser:
                phaserFb[0] = phaserFb[1] = 0.0f;
                for (int ch = 0; ch < 2; ++ch)
                    for (int s = 0; s < kPhaserStages; ++s)
                        apState[ch][s] = 0.0f;
                break;
            default: break;
        }
    }

    static inline float flushDenorm (float x) noexcept
    {
        return std::abs (x) < 1.0e-15f ? 0.0f : x;
    }

    //==========================================================================
    double sampleRate = 44100.0;
    Params params;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> chorusLine  { 4096 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> flangerLine { 4096 };

    float flangerFb[2] {}, flangerLoopLP[2] {};
    float phaserFb[2] {};
    float apState[2][kPhaserStages] {};
    float toneState[2] {};

    juce::SmoothedValue<float> depthSmooth, fbSmooth, mixSmooth, widthSmooth;

    float lfoPhase = 0.0f, phaseInc = 0.0f, toneCoef = 1.0f;

    int curMode = Chorus, prevMode = Chorus;
    int fadeLen = 1, fadeLeft = 0;
};
