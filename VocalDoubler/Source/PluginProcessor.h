#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/Doubler.h"

//==============================================================================
// VocalDoubler — an iZotope-style vocal doubler. A bank of modulated short
// delay lines generates several "extra takes" of the voice that are panned
// across the stereo field, controlled by Separation (width), Variation
// (modulation depth / randomness) and Amount (wet level), plus an Effect Only
// solo of the doubled signal.
//==============================================================================
class VocalDoublerProcessor : public juce::AudioProcessor
{
public:
    VocalDoublerProcessor();
    ~VocalDoublerProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 0.06; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Soft bypass driven by the editor's Bypass button (not an automatable
    // parameter): when engaged the dry signal passes through untouched.
    std::atomic<bool> bypassed { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void applyProgram (int index);

    // The per-voice LFO rates in Doubler are hand-tuned around this characteristic
    // base rate (Hz). The "modRate" parameter divided by this reference gives the
    // scale applied to every voice, so modRate == kReferenceModRate leaves the
    // original motion unchanged.
    static constexpr float kReferenceModRate = 0.2f;

    Doubler engine;
    std::atomic<float>* separationPtr = nullptr;
    std::atomic<float>* variationPtr  = nullptr;
    std::atomic<float>* amountPtr     = nullptr;
    std::atomic<float>* effectOnlyPtr = nullptr;
    std::atomic<float>* modRatePtr    = nullptr;
    std::atomic<float>* modSyncPtr    = nullptr;
    std::atomic<float>* modDivPtr     = nullptr;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalDoublerProcessor)
};
