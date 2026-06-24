#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/AirEnhancer.h"

#include "../../common/Licensing/LicenseManager.h"

//==============================================================================
// VocalAir — an airy high-frequency exciter / enhancer. Two dynamic bands
// ("mid air" presence + "high air" sheen) feed a harmonic exciter, with a link
// toggle, true-bypass power, and an output trim.
//==============================================================================
class VocalAirProcessor : public juce::AudioProcessor
{
public:
    VocalAirProcessor();
    ~VocalAirProcessor() override = default;

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

    float getOutputLevel() const noexcept { return engine.getOutputLevel(); }

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;

    // License engine (hard lock — DSP is bypassed until activated). The editor
    // reads this to show/hide its activation overlay.
    LicenseManager license { "VocalAir" };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void applyProgram (int index);

    AirEnhancer engine;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling; // 4x, anti-alias the exciter
    std::atomic<float>* midAirPtr  = nullptr;
    std::atomic<float>* highAirPtr = nullptr;
    std::atomic<float>* linkPtr    = nullptr;
    std::atomic<float>* powerPtr   = nullptr;
    std::atomic<float>* trimPtr    = nullptr;

    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalAirProcessor)
};
