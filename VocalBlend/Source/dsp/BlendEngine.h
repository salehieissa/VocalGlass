#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

//==============================================================================
// VocalBlend engine — a master-bus "seat the vocal in the beat" processor.
//
// Signal flow (stereo in / stereo out):
//   1. L/R -> M/S. The vocal lives in the MID channel around 1.5-4 kHz; the
//      beat's competing energy sits in the SIDES.
//   2. BLEND (hero): a presence bell (+0..3.5 dB @ 2.6 kHz) on the MID and a
//      matching carve (0..-4 dB) on the SIDES — the classic "make room"
//      complement. The three tilt modes bias this seating:
//        VOCAL — full mid lift, deeper side carve, slight side shelf down
//        EVEN  — balanced lift/carve
//        BEAT  — minimal carve, sides kept full, mid lift halved
//   3. WARMTH: low shelf (+0..3 dB @ 180 Hz) plus a gentle tanh drive on the
//      summed signal for harmonic fusion.
//   4. AIR: high shelf (+0..4 dB @ 12 kHz) on both channels.
//   5. WIDTH: side gain 0..150%, with the sides high-passed at 120 Hz so the
//      low end stays mono-safe at any width.
//   6. GLUE: slow stereo-linked bus compressor (2:1, 30 ms attack, ~250 ms
//      auto release). Threshold and auto-makeup scale with the amount.
//   7. OUTPUT trim, then an optional soft clipper at -0.3 dBFS (LIMIT pill).
//
// All gains are smoothed; filter coefficients update per block. Silence in,
// silence out — nothing here self-oscillates or generates noise.
//==============================================================================
class BlendEngine
{
public:
    struct Params
    {
        int   mode    = 1;      // 0 Vocal, 1 Even, 2 Beat
        float blend   = 0.5f;   // 0..1  presence seating amount
        float glue    = 0.3f;   // 0..1  bus compression amount
        float warmth  = 0.25f;  // 0..1  low shelf + drive
        float air     = 0.25f;  // 0..1  high shelf
        float width   = 1.0f;   // 0..1.5 side gain
        float outDb   = 0.0f;   // -12..+12 output trim
        bool  limit   = true;   // soft-clip safety at -0.3 dBFS
    };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        for (auto* f : { &midBell, &sideBell, &lowShelfM, &lowShelfS,
                         &airShelfM, &airShelfS, &sideHpf })
            f->reset();

        widthSmooth.reset (sampleRate, 0.05);
        outSmooth.reset (sampleRate, 0.03);
        makeupSmooth.reset (sampleRate, 0.10);

        envDb = 0.0f;
        grDb.store (0.0f);
        updateFilters (params, true);
    }

    void reset()
    {
        for (auto* f : { &midBell, &sideBell, &lowShelfM, &lowShelfS,
                         &airShelfM, &airShelfS, &sideHpf })
            f->reset();
        envDb = 0.0f;
    }

    void setParams (const Params& p)
    {
        const bool filtersDirty =
            p.mode != params.mode
            || std::abs (p.blend - params.blend) > 1.0e-4f
            || std::abs (p.warmth - params.warmth) > 1.0e-4f
            || std::abs (p.air - params.air) > 1.0e-4f;

        params = p;
        if (filtersDirty)
            updateFilters (p, false);

        widthSmooth.setTargetValue (p.width);
        outSmooth.setTargetValue (juce::Decibels::decibelsToGain (p.outDb));

        // Glue: threshold walks down and makeup walks up with the amount.
        glueThreshDb = -8.0f - 10.0f * p.glue;                   // -8 .. -18
        makeupSmooth.setTargetValue (juce::Decibels::decibelsToGain (p.glue * 2.5f));

        attCoef = 1.0f - std::exp (-1.0f / (0.030f * (float) sampleRate));
        relCoef = 1.0f - std::exp (-1.0f / (0.250f * (float) sampleRate));
    }

    // Smoothed gain-reduction readout (dB >= 0) for the editor.
    float getGainReductionDb() const { return grDb.load(); }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh = buffer.getNumChannels();
        if (numCh < 1 || numSamples == 0) return;

        auto* left  = buffer.getWritePointer (0);
        auto* right = numCh > 1 ? buffer.getWritePointer (1) : left;

        float blockGrPeak = 0.0f;

        for (int n = 0; n < numSamples; ++n)
        {
            const float inL = left[n], inR = right[n];

            // ---- M/S seating + tone
            float mid  = (inL + inR) * 0.5f;
            float side = (inL - inR) * 0.5f;

            mid  = midBell.processSample (mid);
            mid  = lowShelfM.processSample (mid);
            mid  = airShelfM.processSample (mid);

            side = sideBell.processSample (side);
            side = lowShelfS.processSample (side);
            side = airShelfS.processSample (side);

            // mono-safe width: high-passed sides scale, low sides stay put
            const float width = widthSmooth.getNextValue();
            const float sideHi = sideHpf.processSample (side);
            const float sideLo = side - sideHi;
            side = sideLo + sideHi * width;

            float l = mid + side;
            float r = mid - side;

            // ---- warmth drive (gentle, level-compensated tanh)
            if (driveAmt > 1.0e-4f)
            {
                const float g = 1.0f + driveAmt * 1.5f;
                const float inv = 1.0f / std::tanh (g);
                l = std::tanh (l * g) * inv;
                r = std::tanh (r * g) * inv;
            }

            // ---- glue: stereo-linked feed-forward comp, 2:1 over threshold
            {
                const float peak = juce::jmax (std::abs (l), std::abs (r));
                const float lvlDb = juce::Decibels::gainToDecibels (peak, -80.0f);
                const float overDb = juce::jmax (0.0f, lvlDb - glueThreshDb);
                const float targetGr = overDb * 0.5f * params.glue;   // ratio scales in

                envDb += (targetGr > envDb ? attCoef : relCoef) * (targetGr - envDb);
                envDb = envDb < 1.0e-6f ? 0.0f : envDb;

                const float grGain = juce::Decibels::decibelsToGain (-envDb);
                const float makeup = makeupSmooth.getNextValue();
                l *= grGain * makeup;
                r *= grGain * makeup;
                blockGrPeak = juce::jmax (blockGrPeak, envDb);
            }

            // ---- output trim + soft clip safety
            const float out = outSmooth.getNextValue();
            l *= out;
            r *= out;

            if (params.limit)
            {
                l = softClip (l);
                r = softClip (r);
            }

            left[n] = l;
            if (numCh > 1) right[n] = r;
        }

        // display follows the block peak with a slow fall
        const float shown = grDb.load();
        grDb.store (blockGrPeak > shown ? blockGrPeak
                                        : shown + (blockGrPeak - shown) * 0.25f);
    }

private:
    //==========================================================================
    // Soft clip: transparent below the knee, saturating up to the ceiling.
    static inline float softClip (float x) noexcept
    {
        constexpr float ceiling = 0.966f;   // -0.3 dBFS
        constexpr float knee = 0.80f;
        const float ax = std::abs (x);
        if (ax <= knee) return x;
        const float over = (ax - knee) / (1.0f - knee);
        const float shaped = knee + (ceiling - knee) * std::tanh (over);
        return x > 0.0f ? shaped : -shaped;
    }

    void updateFilters (const Params& p, bool force)
    {
        juce::ignoreUnused (force);
        using Coeffs = juce::dsp::IIR::Coefficients<float>;

        // tilt-mode weights: {mid lift, side carve, side shelf trim}
        float liftW = 1.0f, carveW = 1.0f, sideTrimDb = 0.0f;
        switch (p.mode)
        {
            case 0: liftW = 1.15f; carveW = 1.25f; sideTrimDb = -0.8f * p.blend; break; // VOCAL
            case 2: liftW = 0.50f; carveW = 0.35f; sideTrimDb = 0.0f;            break; // BEAT
            default: break;                                                             // EVEN
        }

        const float liftDb  = 3.5f * p.blend * liftW;
        const float carveDb = -4.0f * p.blend * carveW;

        *midBell.coefficients  = *Coeffs::makePeakFilter (sampleRate, 2600.0, 0.9f,
                                     juce::Decibels::decibelsToGain (liftDb));
        *sideBell.coefficients = *Coeffs::makePeakFilter (sampleRate, 2600.0, 0.9f,
                                     juce::Decibels::decibelsToGain (carveDb + sideTrimDb));

        const float warmDb = 3.0f * p.warmth;
        *lowShelfM.coefficients = *Coeffs::makeLowShelf (sampleRate, 180.0, 0.8f,
                                     juce::Decibels::decibelsToGain (warmDb));
        *lowShelfS.coefficients = *Coeffs::makeLowShelf (sampleRate, 180.0, 0.8f,
                                     juce::Decibels::decibelsToGain (warmDb * 0.5f));

        const float airDb = 4.0f * p.air;
        *airShelfM.coefficients = *Coeffs::makeHighShelf (sampleRate, 12000.0, 0.7f,
                                     juce::Decibels::decibelsToGain (airDb));
        *airShelfS.coefficients = *Coeffs::makeHighShelf (sampleRate, 12000.0, 0.7f,
                                     juce::Decibels::decibelsToGain (airDb));

        *sideHpf.coefficients = *Coeffs::makeHighPass (sampleRate, 120.0);

        driveAmt = p.warmth * 0.35f;
    }

    //==========================================================================
    double sampleRate = 44100.0;
    Params params;

    juce::dsp::IIR::Filter<float> midBell, sideBell, lowShelfM, lowShelfS,
                                  airShelfM, airShelfS, sideHpf;

    juce::SmoothedValue<float> widthSmooth, outSmooth, makeupSmooth;

    float glueThreshDb = -10.0f, attCoef = 0.01f, relCoef = 0.001f;
    float envDb = 0.0f, driveAmt = 0.0f;
    std::atomic<float> grDb { 0.0f };
};
