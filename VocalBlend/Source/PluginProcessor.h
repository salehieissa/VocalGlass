#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/BlendEngine.h"

#include "../../common/Licensing/LicenseManager.h"

//==============================================================================
// VocalBlend — master-bus glue for vocal-forward mixes. Seats the vocal into
// the beat with complementary mid/side presence shaping, adds warmth, air,
// width and a slow glue compressor, and caps the output with a soft-clip
// safety. Three tilt modes bias the seating: VOCAL / EVEN / BEAT.
//==============================================================================
class VocalBlendProcessor : public juce::AudioProcessor
{
public:
    VocalBlendProcessor();
    ~VocalBlendProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Editor readout: glue gain reduction in dB (>= 0).
    float getGainReductionDb() const { return engine.getGainReductionDb(); }

    juce::AudioProcessorValueTreeState apvts;
    BlendEngine engine;

    // License engine (hard lock — DSP is bypassed until activated). The editor
    // reads this to show/hide its activation overlay.
    LicenseManager license { "VocalBlend" };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void applyProgram (int index);

    double currentSampleRate = 44100.0;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalBlendProcessor)
};
