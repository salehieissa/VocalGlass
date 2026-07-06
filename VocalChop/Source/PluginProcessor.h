#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/ChopEngine.h"

#include "../../common/Licensing/LicenseManager.h"

//==============================================================================
// VocalChop — host-synced beat repeater for vocal chops. Two hero divisions
// (CHOP loop length, REFRESH capture rate), FREEZE to hold the current slice,
// per-repeat GATE and FADE shaping, and an equal-power MIX.
//==============================================================================
class VocalChopProcessor : public juce::AudioProcessor
{
public:
    VocalChopProcessor();
    ~VocalChopProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Editor helpers.
    juce::String getChopText() const;
    juce::String getRefreshText() const;
    int getRepeatIndex() const { return engine.getRepeatIndex(); }

    juce::AudioProcessorValueTreeState apvts;
    ChopEngine engine;

    // License engine (hard lock — DSP is bypassed until activated). The editor
    // reads this to show/hide its activation overlay.
    LicenseManager license { "VocalChop" };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void applyProgram (int index);

    double currentSampleRate = 44100.0;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalChopProcessor)
};
