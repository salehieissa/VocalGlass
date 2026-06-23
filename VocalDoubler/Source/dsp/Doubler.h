#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

//==============================================================================
// VocalDoubler engine — an iZotope-style vocal doubler. The input is summed to
// a mono source and fed through several (kNumVoices) short, independently
// modulated fractional delay lines (~8..40 ms). Each voice's delay time is
// modulated by a pair of slow LFOs at slightly different rates plus a smoothed
// random walk, which produces both the timing scatter and the tiny pitch drift
// of a real multi-take double. The voices are panned across the stereo field.
//
//   separation (0..1) — stereo spread / pan width of the doubled voices.
//   variation  (0..1) — depth + randomness of the time/pitch modulation.
//   amount     (0..1) — wet level of the doubled signal blended with the dry.
//   effectOnly (bool) — solo the doubled (wet) signal, muting the dry path.
//
// The dry path is a straight copy (no added latency).
//==============================================================================
class Doubler
{
public:
    static constexpr int kNumVoices = 6;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sr = spec.sampleRate;
        maxDelaySamples = (int) (0.060 * sr) + 8;

        juce::dsp::ProcessSpec monoSpec { sr, spec.maximumBlockSize, 1 };

        for (int v = 0; v < kNumVoices; ++v)
        {
            auto& voice = voices[(size_t) v];
            voice.line.prepare (monoSpec);
            voice.line.setMaximumDelayInSamples (maxDelaySamples);
            voice.line.reset();

            // Base delays spread across ~9..38 ms so the voices never stack.
            const float t = (float) v / (float) (kNumVoices - 1);
            voice.baseDelayMs = 9.0f + t * 29.0f;

            // Alternating L/R pan positions, widest pair first.
            static const float pans[kNumVoices] = { -1.0f, 1.0f, -0.62f, 0.62f, -0.30f, 0.30f };
            voice.basePan = pans[v];

            // Distinct slow LFO rates for organic, never-repeating motion.
            voice.lfoRateA = 0.08f + 0.05f * (float) v;          // 0.08..0.33 Hz
            voice.lfoRateB = 0.17f + 0.037f * (float) (kNumVoices - v);
            voice.phaseA   = juce::MathConstants<float>::twoPi * (0.13f * (float) v);
            voice.phaseB   = juce::MathConstants<float>::twoPi * (0.37f * (float) v);

            voice.randCurrent = 0.0f;
            voice.randTarget  = (rng.nextFloat() * 2.0f - 1.0f);
            voice.randCounter = 0;
        }

        sepSm.reset (sr, 0.05);
        varSm.reset (sr, 0.05);
        amtSm.reset (sr, 0.03);
        dryGainSm.reset (sr, 0.03);

        modScaleSm.reset (sr, 0.05);
        modScaleSm.setCurrentAndTargetValue (1.0f);
    }

    void reset()
    {
        for (auto& v : voices) v.line.reset();
    }

    void setParams (float sep01, float var01, float amt01, bool effectOnly)
    {
        sepSm.setTargetValue (juce::jlimit (0.0f, 1.0f, sep01));
        varSm.setTargetValue (juce::jlimit (0.0f, 1.0f, var01));
        amtSm.setTargetValue (juce::jlimit (0.0f, 1.0f, amt01));
        dryGainSm.setTargetValue (effectOnly ? 0.0f : 1.0f);
    }

    // Master modulation-rate multiplier applied uniformly to every voice's LFO
    // rates, preserving their relative spread/detuning. 1.0 == the original,
    // hand-tuned per-voice rates (so the default feel is unchanged).
    void setModRateScale (float scale)
    {
        modScaleSm.setTargetValue (juce::jlimit (0.01f, 100.0f, scale));
    }

    void process (juce::AudioBuffer<float>& buf)
    {
        const int n  = buf.getNumSamples();
        const int nc = buf.getNumChannels();
        if (n == 0 || nc == 0) return;

        auto* left  = buf.getWritePointer (0);
        auto* right = nc > 1 ? buf.getWritePointer (1) : nullptr;

        const float invNorm = 1.0f / std::sqrt ((float) kNumVoices);

        // How fast the smoothed random target is refreshed (faster with variation).
        for (int i = 0; i < n; ++i)
        {
            const float sep = sepSm.getNextValue();
            const float var = varSm.getNextValue();
            const float amt = amtSm.getNextValue();
            const float dryGain = dryGainSm.getNextValue();
            const float modScale = modScaleSm.getNextValue();

            const float inL = left[i];
            const float inR = right ? right[i] : inL;
            const float mono = 0.5f * (inL + inR);

            // Modulation depth (ms) and randomness scale with variation.
            const float depthMs   = 0.6f + var * 6.5f;
            const float randAmt    = 0.15f + var * 0.85f;
            const int   randPeriod = juce::jmax (1, (int) (sr * (0.18f - var * 0.13f)));

            float wetL = 0.0f, wetR = 0.0f;

            for (auto& voice : voices)
            {
                // --- delay-time modulation: two LFOs + smoothed random walk ---
                voice.phaseA += juce::MathConstants<float>::twoPi * voice.lfoRateA * modScale / (float) sr;
                voice.phaseB += juce::MathConstants<float>::twoPi * voice.lfoRateB * modScale / (float) sr;
                if (voice.phaseA > juce::MathConstants<float>::twoPi) voice.phaseA -= juce::MathConstants<float>::twoPi;
                if (voice.phaseB > juce::MathConstants<float>::twoPi) voice.phaseB -= juce::MathConstants<float>::twoPi;

                if (--voice.randCounter <= 0)
                {
                    voice.randTarget  = rng.nextFloat() * 2.0f - 1.0f;
                    voice.randCounter = randPeriod;
                }
                voice.randCurrent += (voice.randTarget - voice.randCurrent) * 0.0012f;

                const float lfo = 0.6f * std::sin (voice.phaseA) + 0.4f * std::sin (voice.phaseB);
                const float modMs = depthMs * (lfo * (1.0f - randAmt) + voice.randCurrent * randAmt);

                float delaySamples = (voice.baseDelayMs + modMs) * 0.001f * (float) sr;
                delaySamples = juce::jlimit (1.0f, (float) maxDelaySamples - 1.0f, delaySamples);

                const float delayed = voice.line.popSample (0, delaySamples, true);
                voice.line.pushSample (0, mono);

                // --- pan (equal power), width scaled by separation ---
                const float pan = voice.basePan * sep;
                const float ang = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
                wetL += delayed * std::cos (ang);
                wetR += delayed * std::sin (ang);
            }

            wetL *= invNorm;
            wetR *= invNorm;

            const float oL = dryGain * inL + amt * wetL;
            const float oR = dryGain * inR + amt * wetR;

            left[i] = oL;
            if (right) right[i] = oR;
        }
    }

private:
    struct Voice
    {
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> line { 1 << 16 };
        float baseDelayMs = 12.0f;
        float basePan     = 0.0f;
        float lfoRateA = 0.1f, lfoRateB = 0.2f;
        float phaseA = 0.0f, phaseB = 0.0f;
        float randCurrent = 0.0f, randTarget = 0.0f;
        int   randCounter = 0;
    };

    double sr = 44100.0;
    int maxDelaySamples = 2048;

    std::array<Voice, (size_t) kNumVoices> voices;
    juce::Random rng;

    juce::SmoothedValue<float> sepSm, varSm, amtSm, dryGainSm, modScaleSm;
};
