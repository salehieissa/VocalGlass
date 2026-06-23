#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/Compressor.h"

//==============================================================================
// VocalComp — a feed-forward vocal compressor with three voicings (ARC / Opto /
// Warm), parallel mix, makeup gain and output trim, plus live input / output /
// gain-reduction metering for the editor.
//==============================================================================
class VocalCompProcessor : public juce::AudioProcessor
{
public:
    VocalCompProcessor();
    ~VocalCompProcessor() override = default;

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
    Compressor engine;

private:
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling; // 4x, anti-alias Warm saturation
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void applyProgram (int index);

    std::atomic<float>* threshPtr = nullptr;
    std::atomic<float>* ratioPtr  = nullptr;
    std::atomic<float>* attackPtr = nullptr;
    std::atomic<float>* releasePtr = nullptr;
    std::atomic<float>* makeupPtr = nullptr;
    std::atomic<float>* mixPtr    = nullptr;
    std::atomic<float>* trimPtr   = nullptr;
    std::atomic<float>* modePtr   = nullptr;
    std::atomic<float>* gatePtr   = nullptr;

    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalCompProcessor)
};
