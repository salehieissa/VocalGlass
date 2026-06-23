#pragma once

#include <juce_dsp/juce_dsp.h>

//==============================================================================
// Reverb — a thin wrapper around JUCE's built-in Freeverb-style reverb.
// Good, cheap, and easy to drive from a few knobs.
//==============================================================================
class ReverbModule
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        reverb.prepare (spec);
        reverb.reset();
    }

    // size/damp/mix all 0..1
    void setParams (float size, float damp, float mix) noexcept
    {
        params.roomSize  = size;
        params.damping   = damp;
        params.width     = 1.0f;
        params.wetLevel  = mix;
        params.dryLevel  = 1.0f - mix;
        params.freezeMode = 0.0f;
        reverb.setParameters (params);
    }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        reverb.process (ctx);
    }

private:
    juce::dsp::Reverb reverb;
    juce::dsp::Reverb::Parameters params;
};
