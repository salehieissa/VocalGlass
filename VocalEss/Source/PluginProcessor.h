#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
// VocalEss — a de-esser / dynamic sibilance controller.
//
// A sidechain filter listens to the high band, an envelope follower measures
// how far it pushes past the threshold, and the resulting gain reduction is
// applied either to just the high band (Split) or the whole signal (Wideband).
// You can monitor the audio or solo the sidechain to find the right frequency.
//==============================================================================
class VocalEssProcessor : public juce::AudioProcessor
{
public:
    VocalEssProcessor();
    ~VocalEssProcessor() override = default;

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

    // Live metering (all in dB). Read by the editor.
    std::atomic<float> scLevelDb { -100.0f }; // sidechain detector level
    std::atomic<float> attenDb   { 0.0f };    // gain reduction (>= 0)
    std::atomic<float> outLDb    { -100.0f };
    std::atomic<float> outRDb    { -100.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateSidechainFilter (int type, float freq);
    void applyProgram (int index);
    int  currentProgram = 0;

    using Dup = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                               juce::dsp::IIR::Coefficients<float>>;

    juce::dsp::LinkwitzRileyFilter<float> crossover;   // split-band 2-way
    Dup scFilter;                                      // sidechain detection shape
    juce::AudioBuffer<float> scBuffer;                 // filtered detection copy

    // Envelope follower / gain-reduction state.
    float envelope = 0.0f;
    float atkCoeff = 0.0f, relCoeff = 0.0f;
    double currentSampleRate = 44100.0;

    int   lastScType = -1;
    float lastScFreq = -1.0f;
    float lastCrossoverFreq = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalEssProcessor)
};
