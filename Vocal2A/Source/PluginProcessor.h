#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/OptoLeveler.h"

//==============================================================================
// Vocal2A — an LA-2A-style optical vocal leveler. One detection-driven opto
// cell with program-dependent release, compress/limit ratios, auto makeup,
// sidechain hi-freq emphasis, parallel mix and analog hum.
//==============================================================================
class Vocal2AProcessor : public juce::AudioProcessor
{
public:
    Vocal2AProcessor();
    ~Vocal2AProcessor() override = default;

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

    juce::AudioProcessorValueTreeState apvts;
    OptoLeveler engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void applyProgram (int index);

    std::atomic<float>* gainPtr      = nullptr;
    std::atomic<float>* peakPtr      = nullptr;
    std::atomic<float>* modePtr      = nullptr;
    std::atomic<float>* autoPtr      = nullptr;
    std::atomic<float>* analogPtr    = nullptr;
    std::atomic<float>* hiFreqPtr    = nullptr;
    std::atomic<float>* mixPtr       = nullptr;
    std::atomic<float>* trimPtr      = nullptr;

    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Vocal2AProcessor)
};
