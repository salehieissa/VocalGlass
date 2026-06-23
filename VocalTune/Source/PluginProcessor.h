#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/PitchCorrector.h"
#include <array>

//==============================================================================
// VocalTune — real-time pitch correction (autotune). YIN detection + a
// time-domain granular pitch shifter snap sung notes onto an editable scale.
//==============================================================================
class VocalTuneProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener
{
public:
    VocalTuneProcessor();
    ~VocalTuneProcessor() override;

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

    // Live readouts for the editor.
    int   getDetectedNote()  const { return corrector.getDetectedNote(); }
    float getDetectedCents() const { return corrector.getDetectedCents(); }
    bool  hasPitch()         const { return corrector.hasPitch(); }

    // Re-apply the scale mask onto the 12 note params (the "reset" action).
    void applyScaleToNotes();

    juce::AudioProcessorValueTreeState apvts;
    juce::UndoManager undo;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void parameterChanged (const juce::String& id, float newValue) override;
    void applyProgram (int index);

    PitchCorrector corrector;

    std::atomic<float>* retunePtr   = nullptr;
    std::atomic<float>* humanizePtr  = nullptr;
    std::atomic<float>* flexPtr      = nullptr;
    std::atomic<float>* modernPtr    = nullptr;
    std::atomic<float>* hqPtr         = nullptr;
    std::atomic<float>* detunePtr     = nullptr;
    std::atomic<float>* rangePtr      = nullptr;
    std::atomic<float>* powerPtr      = nullptr;
    std::array<std::atomic<float>*, 12> notePtr { };

    // Cached so the rescale path (which can run on the audio thread) never has
    // to build juce::Strings or hash-lookup parameters there.
    std::atomic<float>* keyScalePtr = nullptr;
    std::atomic<float>* keyPtr      = nullptr;
    std::array<juce::RangedAudioParameter*, 12> noteParam { };

    std::atomic<bool> rescaleFlag { false };
    int currentProgram = 0;
    int reportedLatency = -1;   // last value sent to the host (re-sent only on change)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalTuneProcessor)
};
