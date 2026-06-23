#pragma once

#include <juce_dsp/juce_dsp.h>

//==============================================================================
// Delay — a feedback echo with darkening repeats.
//
// Each repeat is low-passed in the feedback path, so echoes get progressively
// darker (an "analog tape"-ish feel) instead of harsh digital copies.
//==============================================================================
class Delay
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        const int maxDelay = (int) (sampleRate * 2.0) + 8; // up to 2 s

        for (auto& dl : delays)
        {
            dl.setMaximumDelayInSamples (maxDelay);
            dl.prepare (spec);
            dl.reset();
        }

        // ~4.5 kHz one-pole low-pass for the feedback path.
        dampCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi
                                     * 4500.0f / (float) sampleRate);
        fbState.fill (0.0f);
    }

    // timeMs: echo time, feedback: 0..~0.95, mix: 0..1 dry/wet
    void setParams (float timeMs, float feedback, float mix) noexcept
    {
        delaySamples = juce::jlimit (1.0f, (float) (sampleRate * 2.0),
                                     (float) (sampleRate * (timeMs / 1000.0)));
        fbAmt  = juce::jlimit (0.0f, 0.95f, feedback);
        mixAmt = mix;
    }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int numCh = buffer.getNumChannels();
        const int n     = buffer.getNumSamples();

        for (int ch = 0; ch < numCh && ch < 2; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            const auto c = (size_t) ch;
            for (int i = 0; i < n; ++i)
            {
                const float in = data[i];
                const float delayed = delays[c].popSample (0, delaySamples);

                // low-pass the signal that feeds back, for darker repeats
                fbState[c] += dampCoeff * (delayed - fbState[c]);
                delays[c].pushSample (0, in + fbState[c] * fbAmt);

                data[i] = in * (1.0f - mixAmt) + delayed * mixAmt;
            }
        }
    }

private:
    using DL = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>;

    double sampleRate = 44100.0;
    std::array<DL, 2> delays;
    std::array<float, 2> fbState { 0.0f, 0.0f };

    float delaySamples = 11025.0f;
    float fbAmt = 0.0f;
    float mixAmt = 0.0f;
    float dampCoeff = 0.5f;
};
