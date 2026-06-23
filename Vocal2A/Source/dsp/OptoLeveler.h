#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>

//==============================================================================
// OptoLeveler — an LA-2A-style optical leveling amplifier.
//
// Signal flow (per block):
//   dry copy  ->  detection sidechain (hi-freq emphasis high-shelf)  ->
//   program-dependent envelope (fast attack, two-stage opto release)  ->
//   gain computer (soft knee, ratio = compress 3:1 / limit 10:1)  ->
//   apply gain reduction  ->  manual makeup (gain knob) + auto makeup  ->
//   parallel mix (dry/wet)  ->  trim  ->  subtle analog hum + noise.
//
// Optical cells release slowly and program-dependently: the more (and longer)
// the cell has been illuminated, the slower it lets go. We model that with a
// two-time-constant release whose blend tracks the current reduction.
//==============================================================================
class OptoLeveler
{
public:
    enum Mode { Compress = 0, Limit = 1 };
    enum Analog { Hum50 = 0, Hum60 = 1, AnalogOff = 2 };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = (int) spec.numChannels;

        dry.setSize (numChannels, (int) spec.maximumBlockSize, false, false, true);

        // detection high-shelf (emphasises highs in the mono sidechain only)
        juce::dsp::ProcessSpec monoSpec { sampleRate, spec.maximumBlockSize, 1 };
        scFilter.prepare (monoSpec);
        scFilter.reset();
        updateSidechainFilter();

        // attack / release coefficients
        attackCoeff   = std::exp (-1.0 / (0.010 * sampleRate));   // ~10 ms
        relFastCoeff  = std::exp (-1.0 / (0.080 * sampleRate));   // ~80 ms initial
        relSlowCoeff  = std::exp (-1.0 / (1.800 * sampleRate));   // ~1.8 s tail
        makeupSmoothCoeff = std::exp (-1.0 / (0.250 * sampleRate));

        env = 0.0;
        autoMakeupDb = 0.0;
        humPhase = 0.0;
        noiseLp = 0.0f;
        meterIn = meterOut = meterGr = 0.0f;

        random.setSeedRandomly();
    }

    //==========================================================================
    void setParams (float gain0to100, float peakReduction0to100, int mode,
                    bool autoMakeupOn, int analogMode, float hiFreq0to100,
                    float mix0to100, float trimDb)
    {
        // gain knob = manual makeup / drive (dB). 50 -> ~+6 dB.
        manualMakeupLin = (float) juce::Decibels::decibelsToGain (
            juce::jmap (gain0to100, 0.0f, 100.0f, -12.0f, 24.0f));

        // peak reduction lowers the threshold (more leveling).
        thresholdDb = juce::jmap (peakReduction0to100, 0.0f, 100.0f, 0.0f, -42.0f);

        ratio = (mode == Limit) ? 10.0f : 3.0f;
        autoMakeup = autoMakeupOn;
        analog = analogMode;

        // hi freq: shelf gain flat (0 dB) .. bright (+12 dB) in the sidechain.
        const float newShelfDb = juce::jmap (hiFreq0to100, 0.0f, 100.0f, 0.0f, 12.0f);
        if (std::abs (newShelfDb - shelfGainDb) > 0.01f)
        {
            shelfGainDb = newShelfDb;
            updateSidechainFilter();
        }

        mix = juce::jlimit (0.0f, 1.0f, mix0to100 * 0.01f);
        trimLin = (float) juce::Decibels::decibelsToGain (trimDb);
    }

    //==========================================================================
    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = juce::jmin (numChannels, buffer.getNumChannels());
        const int n = buffer.getNumSamples();
        if (numCh <= 0 || n <= 0) return;

        // keep an unprocessed dry copy for the parallel mix
        for (int ch = 0; ch < numCh; ++ch)
            dry.copyFrom (ch, 0, buffer, ch, 0, n);

        const float kneeDb = 6.0f;       // soft knee width
        const float eps = 1.0e-6f;
        float inPeak = 0.0f, outPeak = 0.0f, grPeak = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            // --- build mono detection signal (stereo-linked, max of channels) ---
            float detIn = 0.0f, inAbs = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float s = buffer.getSample (ch, i);
                detIn = juce::jmax (detIn, std::abs (s));
                inAbs = juce::jmax (inAbs, std::abs (s));
            }
            inPeak = juce::jmax (inPeak, inAbs);

            // hi-freq emphasis in the sidechain only
            const float detFiltered = scFilter.processSample (detIn);
            const float detAbs = std::abs (detFiltered);

            const float levelDb = juce::Decibels::gainToDecibels (detAbs + eps);

            // --- gain computer: soft-knee static curve -> target reduction (dB, +ve) ---
            const float over = levelDb - thresholdDb;
            float targetGr;
            if (over <= -kneeDb * 0.5f)
                targetGr = 0.0f;
            else if (over >= kneeDb * 0.5f)
                targetGr = over * (1.0f - 1.0f / ratio);
            else
            {
                const float x = over + kneeDb * 0.5f;       // 0..knee
                targetGr = (1.0f - 1.0f / ratio) * (x * x) / (2.0f * kneeDb);
            }

            // --- opto envelope: fast attack, program-dependent two-stage release ---
            if ((double) targetGr > env)
            {
                env = targetGr + (env - targetGr) * attackCoeff;
            }
            else
            {
                // the deeper the reduction, the more the slow cell dominates
                const double depth = juce::jlimit (0.0, 1.0, env / 12.0);
                const double relCoeff = relFastCoeff + (relSlowCoeff - relFastCoeff) * depth;
                env = targetGr + (env - targetGr) * relCoeff;
            }

            const float grDb = (float) env;
            grPeak = juce::jmax (grPeak, grDb);

            const float grLin = (float) juce::Decibels::decibelsToGain (-grDb);

            // --- auto makeup tracks a slow average of the gain reduction ---
            autoMakeupDb = grDb + (autoMakeupDb - grDb) * makeupSmoothCoeff;
            const float autoLin = autoMakeup
                ? (float) juce::Decibels::decibelsToGain (autoMakeupDb * 0.9)
                : 1.0f;

            const float makeup = manualMakeupLin * autoLin;

            // --- analog colouration (subtle hum + DARK noise) ---
            // The noise is heavily low-passed so it reads as a soft "warmth"
            // floor rather than bright white hiss in the top octave.
            float hum = 0.0f, noise = 0.0f;
            if (analog != AnalogOff)
            {
                const double humHz = (analog == Hum50) ? 50.0 : 60.0;
                hum = 0.0050f * std::sin ((float) humPhase);                 // ~ -46 dB
                humPhase += juce::MathConstants<double>::twoPi * humHz / sampleRate;
                if (humPhase > juce::MathConstants<double>::twoPi)
                    humPhase -= juce::MathConstants<double>::twoPi;

                const float white = random.nextFloat() * 2.0f - 1.0f;
                noiseLp += 0.035f * (white - noiseLp);   // ~1-stage LP, very dark
                noise = noiseLp * 0.0042f;               // dark, ~ -58 dB but no hiss
            }

            // --- per-channel: compress, makeup, parallel mix, trim, analog ---
            float outAbs = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float wet = buffer.getSample (ch, i) * grLin * makeup;
                const float dryS = dry.getSample (ch, i);
                float out = wet * mix + dryS * (1.0f - mix);
                out *= trimLin;
                out += hum + noise;
                buffer.setSample (ch, i, out);
                outAbs = juce::jmax (outAbs, std::abs (out));
            }
            outPeak = juce::jmax (outPeak, outAbs);
        }

        // --- update meters (ballistics smoothed toward block peaks) ---
        const float ballistic = 0.4f;
        meterIn  += (juce::Decibels::gainToDecibels (inPeak  + eps) - meterIn)  * ballistic;
        meterOut += (juce::Decibels::gainToDecibels (outPeak + eps) - meterOut) * ballistic;
        meterGr  += (grPeak - meterGr) * ballistic;

        inputDb.store (meterIn);
        outputDb.store (meterOut);
        grReductionDb.store (juce::jmax (0.0f, meterGr));
    }

    //==========================================================================
    // Meter taps for the VU needle (read by the editor).
    std::atomic<float> inputDb       { -60.0f };
    std::atomic<float> outputDb      { -60.0f };
    std::atomic<float> grReductionDb {   0.0f };

private:
    void updateSidechainFilter()
    {
        const double fs = sampleRate > 0.0 ? sampleRate : 44100.0;
        *scFilter.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            fs, 5000.0, 0.707f,
            (float) juce::Decibels::decibelsToGain (shelfGainDb));
    }

    double sampleRate = 44100.0;
    int numChannels = 2;

    juce::AudioBuffer<float> dry;

    juce::dsp::IIR::Filter<float> scFilter;
    float shelfGainDb = 0.0f;

    double attackCoeff = 0.0, relFastCoeff = 0.0, relSlowCoeff = 0.0;
    double makeupSmoothCoeff = 0.0;
    double env = 0.0;
    double autoMakeupDb = 0.0;

    float thresholdDb = 0.0f;
    float ratio = 3.0f;
    float manualMakeupLin = 1.0f;
    float trimLin = 1.0f;
    float mix = 1.0f;
    bool  autoMakeup = false;
    int   analog = AnalogOff;

    double humPhase = 0.0;
    juce::Random random;
    float  noiseLp = 0.0f;   // low-pass state that darkens the analog noise

    float meterIn = -60.0f, meterOut = -60.0f, meterGr = 0.0f;
};
