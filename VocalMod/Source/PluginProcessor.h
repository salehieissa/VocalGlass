#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/ModEngine.h"

#include "../../common/Licensing/LicenseManager.h"

//==============================================================================
// VocalMod — one modulation plugin with three voices: Chorus / Flanger /
// Phaser. Free-running or host-synced LFO, stereo width via LFO phase offset,
// wet-path tone control and an equal-power mix.
//==============================================================================
class VocalModProcessor : public juce::AudioProcessor
{
public:
    VocalModProcessor();
    ~VocalModProcessor() override = default;

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

    // Helpers for the editor.
    juce::String getDivisionText() const;               // e.g. "1/4T"
    float getActiveRateHz() const { return displayRate.load(); }

    juce::AudioProcessorValueTreeState apvts;
    ModEngine engine;

    // License engine (hard lock — DSP is bypassed until activated). The editor
    // reads this to show/hide its activation overlay.
    LicenseManager license { "VocalMod" };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void applyProgram (int index);

    double currentSampleRate = 44100.0;
    int currentProgram = 0;

    std::atomic<float> displayRate { 0.5f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalModProcessor)
};
