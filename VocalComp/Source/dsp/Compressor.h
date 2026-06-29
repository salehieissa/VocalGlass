#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <cmath>

//==============================================================================
// VocalComp engine — a feed-forward stereo-linked vocal compressor.
//
// Detector  : peak / RMS blend (mode dependent), stereo-linked.
// Curve     : soft-knee static curve with threshold + ratio.
// Ballistics: smoothed gain reduction in the dB domain (attack / release).
// Output    : makeup gain -> optional saturation (Warm) -> parallel dry/wet
//             Mix -> output Trim.
//
// Three voicings:
//   ARC  — clean, fast peak detection, moderate modern knee.
//   Opto — slower, program-dependent RMS detection, soft optical knee.
//   Warm — soft knee plus gentle tanh saturation (FET-ish colour).
//
// Live metering atomics (all in dB) are exposed for the editor's timer:
//   input L/R, output L/R, and current gain reduction.
//==============================================================================
class Compressor
{
public:
    enum Mode { Arc = 0, Opto, Warm };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sr = spec.sampleRate;
        reset();
    }

    void reset()
    {
        envDb  = 0.0f;
        rmsSq  = 0.0f;
        gateEnv   = 0.0f;
        gateGain  = 1.0f;
        gateHold  = 0;
        meterInEnv  = { 0.0f, 0.0f };
        meterOutEnv = { 0.0f, 0.0f };
        grMeterEnv  = 0.0f;
        publishMeters();
    }

    void setParams (float thresholdDb_, float ratio_, float attackMs_, float releaseMs_,
                    float makeupDb_, float mix01_, float trimDb_, int mode_, float gateThreshDb_)
    {
        thresholdDb = thresholdDb_;
        ratio       = juce::jmax (1.0f, ratio_);
        attackMs    = attackMs_;
        releaseMs   = releaseMs_;
        makeupDb    = makeupDb_;
        mix         = juce::jlimit (0.0f, 1.0f, mix01_);
        trimDb      = trimDb_;
        mode        = juce::jlimit (0, 2, mode_);
        gateThreshDb = gateThreshDb_;
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int n  = buffer.getNumSamples();
        const int nc = juce::jmin (buffer.getNumChannels(), 2);
        if (n == 0 || nc == 0) return;

        // --- mode-dependent character ---
        // The attack/release knobs stay the dominant control of the timing — mode
        // only nudges it slightly. The real voicing difference lives in the knee
        // width and the peak/RMS detector blend, not in a big hidden multiply on
        // the user's time values (which made the knobs feel like they did nothing).
        float atkScale = 1.0f, relScale = 1.0f, kneeDb = 6.0f, rmsBlend = 0.25f;
        switch (mode)
        {
            case Arc:  atkScale = 1.0f;  relScale = 1.0f;  kneeDb = 6.0f;  rmsBlend = 0.20f; break;
            case Opto: atkScale = 1.4f;  relScale = 1.6f;  kneeDb = 12.0f; rmsBlend = 0.75f; break;
            case Warm: atkScale = 1.15f; relScale = 1.25f; kneeDb = 14.0f; rmsBlend = 0.50f; break;
            default: break;
        }

        const float aCoef   = coef (attackMs  * atkScale);
        const float rCoef   = coef (releaseMs * relScale);
        const float rmsCoef = coef (10.0f);                 // ~10 ms RMS window
        const float meterAtk = coef (5.0f);                 // meter ballistics
        const float meterRel = coef (180.0f);

        const float makeupGain = juce::Decibels::decibelsToGain (makeupDb);
        const float trimGain   = juce::Decibels::decibelsToGain (trimDb);

        // ---- noise gate (runs before compression) ----
        // Disabled at the bottom of the knob's travel so it's fully transparent
        // until dialled in. Hysteresis + a hold time stop it chattering on the
        // tails of words; fast open, slow close keeps consonants intact.
        const bool  gateActive = gateThreshDb > -79.5f;
        const float gateOpenDb  = gateThreshDb;
        const float gateCloseDb = gateThreshDb - 4.0f;       // hysteresis
        const float gateAtk = coef (1.5f);                   // fast open
        const float gateRel = coef (120.0f);                 // smooth close
        const float gateDet = coef (3.0f);                   // detector smoothing
        const int   gateHoldSamples = (int) (0.060 * sr);    // 60 ms hold

        auto* L = buffer.getWritePointer (0);
        auto* R = nc > 1 ? buffer.getWritePointer (1) : nullptr;

        std::array<float, 2> inEnv  = meterInEnv;
        std::array<float, 2> outEnv = meterOutEnv;
        float grEnv = grMeterEnv;

        for (int i = 0; i < n; ++i)
        {
            float xL = L[i];
            float xR = R ? R[i] : xL;

            // ---- input metering (peak with ballistics) — on the raw input ----
            trackMeter (inEnv[0], std::abs (xL), meterAtk, meterRel);
            trackMeter (inEnv[1], std::abs (xR), meterAtk, meterRel);

            // ---- noise gate (gain applied before compression) ----
            if (gateActive)
            {
                const float gPeak = juce::jmax (std::abs (xL), std::abs (xR));
                trackMeter (gateEnv, gPeak, gateDet, gateDet);   // smoothed detector level
                const float gDb = juce::Decibels::gainToDecibels (gateEnv + 1.0e-9f);

                if (gDb >= gateOpenDb)            gateHold = gateHoldSamples;   // above open: (re)arm hold
                else if (gDb < gateCloseDb && gateHold > 0) --gateHold;          // below close: run down hold

                // Open while above the close threshold OR still within the hold window.
                const float gateTarget = (gDb >= gateCloseDb || gateHold > 0) ? 1.0f : 0.0f;
                const float gc = gateTarget > gateGain ? gateAtk : gateRel;     // fast open / slow close
                gateGain = gateTarget + gc * (gateGain - gateTarget);

                xL *= gateGain;
                xR *= gateGain;
            }
            else
            {
                gateGain = 1.0f;
                gateHold = 0;
            }

            // ---- detector (stereo-linked peak / RMS blend) ----
            const float peak = juce::jmax (std::abs (xL), std::abs (xR));
            const float sq   = 0.5f * (xL * xL + xR * xR);
            rmsSq = sq + rmsCoef * (rmsSq - sq);
            const float rms  = std::sqrt (juce::jmax (0.0f, rmsSq));
            const float det  = peak * (1.0f - rmsBlend) + rms * rmsBlend;
            const float inDb = juce::Decibels::gainToDecibels (det + 1.0e-9f);

            // ---- soft-knee static curve -> target gain reduction (<= 0 dB) ----
            const float slope = (1.0f / ratio) - 1.0f;
            const float over  = inDb - thresholdDb;
            float targetGrDb;
            if (over <= -kneeDb * 0.5f)
                targetGrDb = 0.0f;
            else if (over >= kneeDb * 0.5f)
                targetGrDb = slope * over;
            else
            {
                const float k = over + kneeDb * 0.5f;       // 0..kneeDb
                targetGrDb = slope * (k * k) / (2.0f * kneeDb);
            }

            // ---- ballistics in dB domain ----
            if (targetGrDb < envDb)                          // need more reduction => attack
                envDb = targetGrDb + aCoef * (envDb - targetGrDb);
            else                                             // recovering => release
                envDb = targetGrDb + rCoef * (envDb - targetGrDb);

            const float gain = juce::Decibels::decibelsToGain (envDb);

            // ---- apply gain + makeup ----
            float yL = xL * gain * makeupGain;
            float yR = xR * gain * makeupGain;

            // ---- Warm saturation (gentle tanh) ----
            if (mode == Warm)
            {
                constexpr float drive = 1.6f;
                const float norm = std::tanh (drive);
                yL = std::tanh (yL * drive) / norm;
                yR = std::tanh (yR * drive) / norm;
            }

            // ---- parallel Mix (dry/wet) then Trim ----
            yL = (xL * (1.0f - mix) + yL * mix) * trimGain;
            yR = (xR * (1.0f - mix) + yR * mix) * trimGain;

            L[i] = yL;
            if (R) R[i] = yR;

            // ---- output metering + GR metering ----
            trackMeter (outEnv[0], std::abs (yL), meterAtk, meterRel);
            trackMeter (outEnv[1], std::abs (yR), meterAtk, meterRel);

            const float grNow = -envDb;                      // positive dB of reduction
            grEnv = grNow > grEnv ? grNow : grNow + meterRel * (grEnv - grNow);
        }

        meterInEnv  = inEnv;
        meterOutEnv = outEnv;
        grMeterEnv  = grEnv;
        publishMeters();
    }

    // Meter atomics (dB). GR is positive dB of reduction.
    std::atomic<float> meterInL  { -100.0f };
    std::atomic<float> meterInR  { -100.0f };
    std::atomic<float> meterOutL { -100.0f };
    std::atomic<float> meterOutR { -100.0f };
    std::atomic<float> meterGR   {    0.0f };

private:
    float coef (float ms) const
    {
        return std::exp (-1.0f / (juce::jmax (0.05f, ms) * 0.001f * (float) sr));
    }

    static void trackMeter (float& env, float x, float atk, float rel)
    {
        env = x > env ? atk * (env - x) + x : rel * (env - x) + x;
    }

    void publishMeters()
    {
        meterInL .store (juce::Decibels::gainToDecibels (meterInEnv[0]  + 1.0e-9f));
        meterInR .store (juce::Decibels::gainToDecibels (meterInEnv[1]  + 1.0e-9f));
        meterOutL.store (juce::Decibels::gainToDecibels (meterOutEnv[0] + 1.0e-9f));
        meterOutR.store (juce::Decibels::gainToDecibels (meterOutEnv[1] + 1.0e-9f));
        meterGR  .store (grMeterEnv);
    }

    double sr = 44100.0;

    float thresholdDb = -24.0f;
    float ratio       = 2.0f;
    float attackMs    = 20.0f;
    float releaseMs   = 200.0f;
    float makeupDb    = 0.0f;
    float mix         = 1.0f;
    float trimDb      = 0.0f;
    int   mode        = Arc;
    float gateThreshDb = -80.0f;   // -80 == gate off

    float envDb = 0.0f;
    float rmsSq = 0.0f;

    // gate state
    float gateEnv  = 0.0f;
    float gateGain = 1.0f;
    int   gateHold = 0;

    std::array<float, 2> meterInEnv  { 0.0f, 0.0f };
    std::array<float, 2> meterOutEnv { 0.0f, 0.0f };
    float grMeterEnv = 0.0f;
};
