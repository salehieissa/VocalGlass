#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

//==============================================================================
// VocalDelay engine — a real stereo delay built on juce::dsp.
//
//   * Lagrange-interpolated stereo delay line (per-channel fractional reads)
//   * Four routing modes: Left only / Ping-Pong / Dual / Right only
//   * Feedback path: high-pass + low-pass filtering, analog saturation & "wow"
//   * Chorus-style modulation of the delay time (depth / rate)
//   * Optional Lo-Fi (sample-rate + bit reduction) on the wet signal
//   * Dry/wet mix and output gain, with L/R peak metering
//==============================================================================
class DelayEngine
{
public:
    enum Mode { LeftOnly = 0, PingPong, Dual, RightOnly };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        const int maxSamples = (int) (sampleRate * 2.2) + 256; // 2 s max + modulation headroom
        delay.setMaximumDelayInSamples (maxSamples);
        delay.prepare (spec);
        delay.reset();

        hp.prepare (spec);
        hp.setType (juce::dsp::StateVariableTPTFilterType::highpass);
        lp.prepare (spec);
        lp.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

        smoothL.reset (sampleRate, 0.05);
        smoothR.reset (sampleRate, 0.05);
        wetSmooth.reset (sampleRate, 0.02);
        drySmooth.reset (sampleRate, 0.02);
        gainSmooth.reset (sampleRate, 0.02);
        fbSmooth.reset (sampleRate, 0.02);

        modPhase = 0.0f;
        wowPhase = 0.0f;
        holdL = holdR = 0.0f;
        holdCounter = 0;

        // Force the filter coefficients to be (re)computed on the next setParams.
        lastHpHz = -1.0f;
        lastLpHz = -1.0f;

        peakL.store (0.0f);
        peakR.store (0.0f);
    }

    void reset()
    {
        delay.reset();
        hp.reset();
        lp.reset();
        holdL = holdR = 0.0f;
        holdCounter = 0;
    }

    struct Params
    {
        float delaySamplesL = 4800.0f;
        float delaySamplesR = 4800.0f;
        int   mode          = Dual;
        float feedback      = 0.4f;   // 0..1.1
        float depth         = 0.0f;   // 0..1
        float rate          = 0.2f;   // Hz
        float hpHz          = 120.0f;
        float lpHz          = 18000.0f;
        float dryWet        = 0.5f;   // 0..1
        float outputGain    = 1.0f;   // linear
        float analog        = 0.2f;   // 0..1
        bool  lofi          = false;
    };

    void setParams (const Params& p)
    {
        params = p;
        smoothL.setTargetValue (p.delaySamplesL);
        smoothR.setTargetValue (p.delaySamplesR);
        wetSmooth.setTargetValue (p.dryWet);
        drySmooth.setTargetValue (1.0f - p.dryWet);
        gainSmooth.setTargetValue (p.outputGain);
        fbSmooth.setTargetValue (p.feedback);

        // Recompute (relatively expensive) TPT filter coefficients only when the
        // requested cutoff actually changes. The result is bit-identical to calling
        // setCutoffFrequency every block, but avoids the per-block coefficient maths
        // when the user isn't moving the filter knobs.
        if (p.hpHz != lastHpHz)
        {
            hp.setCutoffFrequency (juce::jlimit (20.0f, 2000.0f, p.hpHz));
            lastHpHz = p.hpHz;
        }
        if (p.lpHz != lastLpHz)
        {
            lp.setCutoffFrequency (juce::jlimit (1000.0f, 20000.0f, p.lpHz));
            lastLpHz = p.lpHz;
        }
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh = buffer.getNumChannels();
        if (numCh < 1) return;

        auto* left  = buffer.getWritePointer (0);
        auto* right = numCh > 1 ? buffer.getWritePointer (1) : left;

        const float modRateInc = juce::MathConstants<float>::twoPi * params.rate / (float) sampleRate;
        const float wowInc      = juce::MathConstants<float>::twoPi * 0.13f / (float) sampleRate;
        const float modDepthSamp = params.depth * 0.004f * (float) sampleRate; // up to ~4 ms
        const float wowDepthSamp = params.analog * 0.0012f * (float) sampleRate; // subtle drift
        const float drive = 1.0f + params.analog * 4.0f;

        float localPeakL = 0.0f, localPeakR = 0.0f;

        // Lo-fi parameters
        const int   holdSamples = juce::jmax (1, (int) (sampleRate / 8000.0)); // ~8 kHz S&H
        const float bitStep = 1.0f / 64.0f; // ~6-bit quantisation

        for (int n = 0; n < numSamples; ++n)
        {
            const float inL = left[n];
            const float inR = right[n];

            const float fb = fbSmooth.getNextValue();

            // modulation offsets
            const float modL = std::sin (modPhase) * modDepthSamp + std::sin (wowPhase) * wowDepthSamp;
            const float modR = std::sin (modPhase + 0.5f) * modDepthSamp + std::cos (wowPhase) * wowDepthSamp;
            modPhase += modRateInc; if (modPhase > juce::MathConstants<float>::twoPi) modPhase -= juce::MathConstants<float>::twoPi;
            wowPhase += wowInc;     if (wowPhase > juce::MathConstants<float>::twoPi) wowPhase -= juce::MathConstants<float>::twoPi;

            const float dSampL = juce::jlimit (1.0f, (float) delay.getMaximumDelayInSamples() - 2.0f,
                                               smoothL.getNextValue() + modL);
            const float dSampR = juce::jlimit (1.0f, (float) delay.getMaximumDelayInSamples() - 2.0f,
                                               smoothR.getNextValue() + modR);

            // read delayed samples
            float dL = delay.popSample (0, dSampL, true);
            float dR = delay.popSample (1, dSampR, true);

            // Tone-shape the delayed signal with the high-pass + low-pass. This is
            // applied to BOTH the audible wet taps and the recirculating feedback so
            // the low-cut / high-cut knobs are clearly heard on every repeat — not
            // just buried deep in the feedback tail.
            float fL = lp.processSample (0, hp.processSample (0, dL));
            float fR = lp.processSample (1, hp.processSample (1, dR));

            // feedback signal: filtered + analog saturation
            float fbL = fL;
            float fbR = fR;

            if (params.analog > 0.001f)
            {
                fbL = std::tanh (fbL * drive) / drive;
                fbR = std::tanh (fbR * drive) / drive;
            }

            // Flush denormals on the recirculating feedback signal. On silent tails
            // the values decaying through the loop can become subnormal, which causes
            // large CPU spikes. Snapping sub -300 dB values to zero is inaudible
            // (well below the 24-bit noise floor) but keeps the loop cheap.
            fbL = flushDenorm (fbL);
            fbR = flushDenorm (fbR);

            // write into the lines according to the routing mode
            float wrL = inL, wrR = inR;
            switch (params.mode)
            {
                case PingPong:
                    wrL = inL + fbR * fb;
                    wrR = inR + fbL * fb;
                    break;
                case Dual:
                    wrL = inL + fbL * fb;
                    wrR = inR + fbR * fb;
                    break;
                case LeftOnly:
                    wrL = inL + fbL * fb;
                    wrR = 0.0f;
                    break;
                case RightOnly:
                    wrL = 0.0f;
                    wrR = inR + fbR * fb;
                    break;
                default: break;
            }

            delay.pushSample (0, wrL);
            delay.pushSample (1, wrR);

            // wet taps (filtered, so the cut knobs shape what you actually hear)
            float wetL = fL;
            float wetR = fR;

            if (params.mode == LeftOnly)  wetR = 0.0f;
            if (params.mode == RightOnly) wetL = 0.0f;

            // Lo-fi on the wet signal
            if (params.lofi)
            {
                if (holdCounter <= 0)
                {
                    holdL = std::floor (wetL / bitStep + 0.5f) * bitStep;
                    holdR = std::floor (wetR / bitStep + 0.5f) * bitStep;
                    holdCounter = holdSamples;
                }
                --holdCounter;
                wetL = holdL;
                wetR = holdR;
            }

            const float wetMix = wetSmooth.getNextValue();
            const float dryMix = drySmooth.getNextValue();
            const float g = gainSmooth.getNextValue();

            const float outL = (inL * dryMix + wetL * wetMix) * g;
            const float outR = (inR * dryMix + wetR * wetMix) * g;

            left[n]  = outL;
            right[n] = outR;

            localPeakL = juce::jmax (localPeakL, std::abs (outL));
            localPeakR = juce::jmax (localPeakR, std::abs (outR));
        }

        peakL.store (localPeakL);
        peakR.store (localPeakR);
    }

    std::atomic<float> peakL { 0.0f };
    std::atomic<float> peakR { 0.0f };

private:
    static inline float flushDenorm (float x) noexcept
    {
        return std::abs (x) < 1.0e-15f ? 0.0f : x;
    }

    double sampleRate = 44100.0;

    // Cached cutoffs so filter coefficients are only recomputed on change.
    float lastHpHz = -1.0f;
    float lastLpHz = -1.0f;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delay { 96000 * 3 };
    juce::dsp::StateVariableTPTFilter<float> hp, lp;

    juce::SmoothedValue<float> smoothL, smoothR, wetSmooth, drySmooth, gainSmooth, fbSmooth;

    float modPhase = 0.0f, wowPhase = 0.0f;
    float holdL = 0.0f, holdR = 0.0f;
    int   holdCounter = 0;

    Params params;
};
