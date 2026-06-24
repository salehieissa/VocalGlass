#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/Formant.h"
#include "dsp/Doubler.h"
#include "dsp/Delay.h"
#include "dsp/ReverbModule.h"
#include "dsp/Glitch.h"
#include "licensing/LicenseManager.h"

//==============================================================================
// VocalGrit — a small vocal effects chain.
//
// Signal flow (per block):
//   GRIT   : pre high-pass -> drive -> oversampled waveshaper -> DC block
//            -> tone low-pass -> dry/wet mix
//   DOUBLER: ADT-style modulated stereo doubling
//   DELAY  : feedback echo with darkening repeats
//   REVERB : Freeverb-style space
//   OUTPUT : global trim gain
// Each module has its own on/off toggle.
//==============================================================================
class VocalGritProcessor : public juce::AudioProcessor
{
public:
    VocalGritProcessor();
    ~VocalGritProcessor() override = default;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==========================================================================
    // Presets (drive the header browser in the UI).
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // The value tree holding all parameter state. The editor attaches to this.
    juce::AudioProcessorValueTreeState apvts;

    // Live metering (linear peak, 0..1+). Read by the editor's meters.
    std::atomic<float> inputLevel  { 0.0f };
    std::atomic<float> outputLevel { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // The nonlinear curve applied to every (oversampled) sample.
    static float shape (float x, int mode, float bias);

    // 4x oversampling wraps the nonlinearity to suppress aliasing.
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    using Shelf = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                 juce::dsp::IIR::Coefficients<float>>;

    juce::dsp::StateVariableTPTFilter<float> preHighPass;   // keep lows clean
    juce::dsp::StateVariableTPTFilter<float> toneLowPass;   // tone control
    Shelf ampShelf;                                         // "amp" low-mid warmth
    Shelf presenceShelf;                                    // "presence" air boost
    juce::dsp::StateVariableTPTFilter<float> speakerHP;     // "speaker" cab low end
    juce::dsp::StateVariableTPTFilter<float> speakerLP;     // "speaker" cab top end

    // Formant-dependent tone compensation:
    //   down-shift (deep voice)  -> high-pass up to 140 Hz to clear the mud
    //   up-shift   (bright voice) -> low shelf to put body/weight back
    juce::dsp::StateVariableTPTFilter<float> formantHP;
    Shelf formantLowShelf;
    float lastFormantUp = -1.0f;

    // Cached cutoffs so we only recompute filter coefficients (a std::tan per
    // call) when the controlling value actually changes. Same input -> same
    // coefficients, so this is bit-identical to recomputing every block.
    float lastLowCut     = -1.0f;
    float lastTone       = -1.0f;
    float lastFormantHPc = -1.0f;

    // One-pole DC blockers (per channel) to remove offset from biasing.
    std::array<float, 2> dcX1 { 0.0f, 0.0f };
    std::array<float, 2> dcY1 { 0.0f, 0.0f };

    // Downstream effect modules.
    Formant      formantProc;
    Doubler      doubler;
    Delay        delay;
    ReverbModule reverb;
    Glitch       glitch;

    juce::AudioBuffer<float> dryBuffer;
    double currentSampleRate = 44100.0;

    int currentProgram = 0;
    void applyProgram (int index);

    // Output is muted until this plugin is activated (audio-thread-safe read).
    licensing::ProductLicense license { "VOCALGRIT" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalGritProcessor)
};
