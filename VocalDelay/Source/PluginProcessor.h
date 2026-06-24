#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "licensing/LicenseManager.h"
#include <juce_dsp/juce_dsp.h>
#include "dsp/DelayEngine.h"

//==============================================================================
// VocalDelay — a tempo-syncable stereo delay for vocals. Sync to host, an
// internal/tap BPM, or free milliseconds; four routing modes; feedback-path
// filtering, modulation, analog colour and a lo-fi crush.
//==============================================================================
class VocalDelayProcessor : public juce::AudioProcessor
{
public:
    VocalDelayProcessor();
    ~VocalDelayProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Helpers for the editor.
    juce::String getDivisionText() const;     // e.g. "1/8D"
    float getActiveBpm() const { return displayBpm.load(); }
    float getActiveDelayMs() const { return displayMs.load(); }

    juce::AudioProcessorValueTreeState apvts;
    DelayEngine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void applyProgram (int index);

    double currentSampleRate = 44100.0;
    int currentProgram = 0;

    std::atomic<float> displayBpm { 120.0f };
    std::atomic<float> displayMs  { 350.0f };

    // Output is muted until this plugin is activated (audio-thread-safe read).
    licensing::ProductLicense license { "VOCALDELAY" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalDelayProcessor)
};
