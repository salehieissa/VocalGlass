#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "../../common/Licensing/LicenseManager.h"

//==============================================================================
// VocalGate — a downward noise gate for vocals.
//
// A sidechain copy of the input runs through a high-pass filter (so rumble and
// plosives don't hold the gate open), is mono-summed and fed to a hybrid
// peak/RMS detector. A per-sample state machine (closed -> open -> hold ->
// release) drives an attack/release-smoothed gain that ducks the signal down
// to -range dB between phrases. Hysteresis keeps the gate from chattering
// around the threshold; SC Listen monitors the filtered detector signal.
//==============================================================================
class VocalGateProcessor : public juce::AudioProcessor
{
public:
    VocalGateProcessor();
    ~VocalGateProcessor() override = default;

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
    std::atomic<float> inDb  { -100.0f }; // pre-gate input peak
    std::atomic<float> grDb  { 0.0f };    // current attenuation (>= 0)
    std::atomic<float> outDb { -100.0f }; // post-gate output peak

    // License engine (hard lock — DSP is bypassed until activated). The editor
    // reads this to show/hide its activation overlay.
    LicenseManager license { "VocalGate" };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateSidechainFilter (float freq);
    void applyProgram (int index);
    int  currentProgram = 0;

    using Dup = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                               juce::dsp::IIR::Coefficients<float>>;

    Dup scFilter;                          // sidechain high-pass (detection only)
    juce::AudioBuffer<float> scBuffer;     // filtered detection copy

    // Hybrid detector state.
    float peakEnv = 0.0f;                  // fast peak follower
    float rmsSq   = 0.0f;                  // one-pole smoothed squared signal
    float peakAtkCoeff = 0.0f, peakRelCoeff = 0.0f, rmsCoeff = 0.0f;

    // Gate state machine + smoothed gain.
    enum class GateState { Closed, Open, Hold };
    GateState gateState = GateState::Closed;
    int   holdRemaining = 0;
    float gain = 1.0f;

    double currentSampleRate = 44100.0;
    float  lastScFreq = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalGateProcessor)
};
