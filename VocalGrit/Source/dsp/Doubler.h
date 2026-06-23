#pragma once

#include <juce_dsp/juce_dsp.h>

//==============================================================================
// Doubler — artificial double-tracking (ADT).
//
// Makes one vocal sound like two by adding short, slowly-modulated delayed
// copies panned left/right. The tiny pitch wobble (from modulating the delay
// time) makes each copy feel like a separate take.
//==============================================================================
class Doubler
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        const int maxDelay = (int) (sampleRate * 0.05) + 8; // up to 50 ms

        for (auto& dl : delays)
        {
            dl.setMaximumDelayInSamples (maxDelay);
            dl.prepare (spec);
            dl.reset();
        }

        phase[0] = 0.0;
        phase[1] = 0.5; // start the two voices out of phase
    }

    // detune: 0..1 wobble depth,  width: 0..1 stereo spread,  mix: 0..1 amount
    void setParams (float detune, float width, float mix) noexcept
    {
        depthSamples = detune * (float) (sampleRate * 0.004); // up to ~4 ms swing
        widthAmt = width;
        mixAmt   = mix;
    }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int numCh = buffer.getNumChannels();
        if (numCh < 1) return;

        auto* left  = buffer.getWritePointer (0);
        auto* right = numCh > 1 ? buffer.getWritePointer (1) : left;
        const int n = buffer.getNumSamples();

        const float baseL = (float) (sampleRate * 0.021); // 21 ms
        const float baseR = (float) (sampleRate * 0.027); // 27 ms

        const double incL = rateL / sampleRate;
        const double incR = rateR / sampleRate;

        // width: same-side vs cross-feed gains
        const float a = 0.5f + 0.5f * widthAmt;
        const float b = 0.5f - 0.5f * widthAmt;

        for (int i = 0; i < n; ++i)
        {
            const float dryL = left[i];
            const float dryR = right[i];
            const float mono = 0.5f * (dryL + dryR);

            const float lfoL = (float) std::sin (juce::MathConstants<double>::twoPi * phase[0]);
            const float lfoR = (float) std::sin (juce::MathConstants<double>::twoPi * phase[1]);

            delays[0].pushSample (0, mono);
            delays[1].pushSample (0, mono);

            const float voiceL = delays[0].popSample (0, baseL + depthSamples * lfoL);
            const float voiceR = delays[1].popSample (0, baseR + depthSamples * lfoR);

            left[i]  = dryL + mixAmt * (voiceL * a + voiceR * b);
            right[i] = dryR + mixAmt * (voiceR * a + voiceL * b);

            phase[0] += incL; if (phase[0] >= 1.0) phase[0] -= 1.0;
            phase[1] += incR; if (phase[1] >= 1.0) phase[1] -= 1.0;
        }
    }

private:
    using DL = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>;

    double sampleRate = 44100.0;
    std::array<DL, 2> delays;

    double phase[2] { 0.0, 0.5 };
    const double rateL = 0.31; // Hz — two slightly different wobble rates
    const double rateR = 0.43;

    float depthSamples = 0.0f;
    float widthAmt = 1.0f;
    float mixAmt   = 0.0f;
};
