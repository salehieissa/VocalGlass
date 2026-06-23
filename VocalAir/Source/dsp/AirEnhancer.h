#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

//==============================================================================
// VocalAir engine — a two-band dynamic high-frequency exciter / enhancer.
//
//   * "mid air"  lifts presence around 6.5 kHz
//   * "high air"  lifts air around 13.5 kHz
//
// Each band combines two things:
//   1. A dynamic high-shelf boost (0 .. ~+12 dB). The shelf gain breathes a
//      touch with an envelope follower so quieter passages get slightly more
//      lift — keeps it feeling alive rather than static.
//   2. Gentle harmonic excitement: the band is high-passed, run through a soft
//      tanh saturator to manufacture upper harmonics, and mixed back in.
//
// "power" is a true bypass, "trim" is a clean output gain. The post-processing
// output level is published through an atomic for the meter to read.
//==============================================================================
class AirEnhancer
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sr           = spec.sampleRate;
        numChannels  = (int) juce::jmin<juce::uint32> (spec.numChannels, 2);
        maxBlock     = (int) spec.maximumBlockSize;

        midShelf.prepare (spec);
        highShelf.prepare (spec);
        midExcite.prepare (spec);
        highExcite.prepare (spec);

        midScratch.setSize (2, maxBlock);
        highScratch.setSize (2, maxBlock);

        // Fixed high-pass corners feeding the excitement saturators.
        *midExcite.state  = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, kMidFreq);
        *highExcite.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, kHighFreq);

        // Invalidate the shelf cache so the rebuild below always runs for the
        // (possibly new) sample rate.
        lastMidDb  = std::numeric_limits<float>::quiet_NaN();
        lastHighDb = std::numeric_limits<float>::quiet_NaN();
        updateShelves (0.0f, 0.0f, 0.0f, 0.0f);
        reset();
    }

    void reset()
    {
        midShelf.reset();
        highShelf.reset();
        midExcite.reset();
        highExcite.reset();
        inEnv     = 0.0f;
        meterEnv  = 0.0f;
    }

    void setParams (float midAir01, float highAir01, float trimDb, bool powerOn)
    {
        midAir  = juce::jlimit (0.0f, 1.0f, midAir01);
        highAir = juce::jlimit (0.0f, 1.0f, highAir01);
        trimGain = juce::Decibels::decibelsToGain (juce::jlimit (-12.0f, 12.0f, trimDb));
        power = powerOn;
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int n  = buffer.getNumSamples();
        const int nc = juce::jmin (buffer.getNumChannels(), 2);
        if (n == 0 || nc == 0) return;

        if (power)
        {
            // ----- envelope follower on the input for the dynamic shelf -----
            float mag = 0.0f;
            for (int ch = 0; ch < nc; ++ch)
                mag = juce::jmax (mag, buffer.getMagnitude (ch, 0, n));

            const float aCoef = blockCoef (5.0f,   n);   // fast attack
            const float rCoef = blockCoef (220.0f, n);   // slow release
            inEnv = mag > inEnv ? aCoef * (inEnv - mag) + mag
                                : rCoef * (inEnv - mag) + mag;

            // Quieter passages (lower env) get a little more boost.
            float dyn = 0.0f;
            if (inEnv > 0.001f)   // above ~-60 dB
            {
                const float db = juce::Decibels::gainToDecibels (inEnv);
                dyn = juce::jlimit (0.0f, 1.0f, (-12.0f - db) / 24.0f); // -12 dB -> 0, -36 dB -> 1
            }
            const float extra = dyn * 3.0f; // up to +3 dB extra on quiet material

            updateShelves (midAir, highAir, extra * (midAir  > 0.001f ? 1.0f : 0.0f),
                                            extra * (highAir > 0.001f ? 1.0f : 0.0f));

            // ----- dynamic high-shelves -----
            juce::dsp::AudioBlock<float> block (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            midShelf.process (ctx);
            highShelf.process (ctx);

            // ----- harmonic excitement -----
            applyExcite (buffer, midExcite,  midScratch,  midAir  * 0.30f, 3.0f);
            applyExcite (buffer, highExcite, highScratch, highAir * 0.35f, 4.0f);

            // ----- output trim -----
            buffer.applyGain (trimGain);
        }

        // ----- output meter: peak with fast attack / slow release -----
        float out = 0.0f;
        for (int ch = 0; ch < nc; ++ch)
            out = juce::jmax (out, buffer.getMagnitude (ch, 0, n));

        const float ma = blockCoef (3.0f,   n);
        const float mr = blockCoef (300.0f, n);
        meterEnv = out > meterEnv ? ma * (meterEnv - out) + out
                                  : mr * (meterEnv - out) + out;
        outputLevel.store (meterEnv);
    }

    float getOutputLevel() const noexcept { return outputLevel.load(); }

private:
    using Duplicator = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                      juce::dsp::IIR::Coefficients<float>>;

    void updateShelves (float midA, float highA, float midExtraDb, float highExtraDb)
    {
        const float midDb  = midA  * 12.0f + midExtraDb;
        const float highDb = highA * 12.0f + highExtraDb;

        // makeHighShelf is deterministic in (sr, freq, Q, dB); sr/freq/Q are
        // fixed, so the coefficients only change when the computed shelf dB
        // changes. Skipping the rebuild when dB is unchanged is bit-identical
        // for a fixed parameter + dynamic state, but still tracks the envelope-
        // driven "extra" term whenever it actually moves.
        // Bit-exact comparison (the value is recomputed deterministically each
        // block); avoids -Wfloat-equal and treats the NaN sentinel correctly.
        if (! bitsEqual (midDb, lastMidDb))
        {
            *midShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                sr, kMidFreq, 0.7f, juce::Decibels::decibelsToGain (midDb));
            lastMidDb = midDb;
        }

        if (! bitsEqual (highDb, lastHighDb))
        {
            *highShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                sr, kHighFreq, 0.7f, juce::Decibels::decibelsToGain (highDb));
            lastHighDb = highDb;
        }
    }

    static bool bitsEqual (float a, float b) noexcept
    {
        return std::memcmp (&a, &b, sizeof (float)) == 0;
    }

    static float softSat (float x, float drive)
    {
        return std::tanh (x * drive) / std::tanh (drive);
    }

    void applyExcite (juce::AudioBuffer<float>& buffer, Duplicator& hp,
                      juce::AudioBuffer<float>& scratch, float amount, float drive)
    {
        if (amount < 0.0001f) return;

        const int n  = buffer.getNumSamples();
        const int nc = juce::jmin (buffer.getNumChannels(), 2);

        for (int ch = 0; ch < nc; ++ch)
            scratch.copyFrom (ch, 0, buffer, ch, 0, n);

        juce::dsp::AudioBlock<float> block (scratch.getArrayOfWritePointers(),
                                            (size_t) nc, 0, (size_t) n);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        hp.process (ctx);

        for (int ch = 0; ch < nc; ++ch)
        {
            auto* dst = buffer.getWritePointer (ch);
            auto* src = scratch.getReadPointer (ch);
            for (int i = 0; i < n; ++i)
                dst[i] += amount * softSat (src[i], drive);
        }
    }

    float blockCoef (float ms, int n) const
    {
        // Per-block one-pole coefficient given a time constant in ms.
        const float blocksPerSec = (float) sr / juce::jmax (1, n);
        return std::exp (-1.0f / (juce::jmax (0.05f, ms) * 0.001f * blocksPerSec));
    }

    double sr = 44100.0;
    int numChannels = 2;
    int maxBlock = 512;

    float midAir = 0.0f, highAir = 0.0f;
    float trimGain = 1.0f;
    bool  power = true;

    float inEnv = 0.0f;
    float meterEnv = 0.0f;
    std::atomic<float> outputLevel { 0.0f };

    // Last shelf dB actually applied; NaN forces the first prepare() update.
    float lastMidDb  = std::numeric_limits<float>::quiet_NaN();
    float lastHighDb = std::numeric_limits<float>::quiet_NaN();

    Duplicator midShelf, highShelf, midExcite, highExcite;
    juce::AudioBuffer<float> midScratch, highScratch;

    static constexpr float kMidFreq  = 6500.0f;
    static constexpr float kHighFreq = 13500.0f;
};
