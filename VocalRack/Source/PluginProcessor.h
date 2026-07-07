#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "../../common/Licensing/LicenseManager.h"
#include "dsp/Compressor.h"
#include "dsp/AirEnhancer.h"
#include "dsp/DelayEngine.h"
#include "dsp/PlateReverb.h"
#include "dsp/ClipEngine.h"

//==============================================================================
// VocalRack — a full vocal channel strip ("studio rack") in one window.
//
// Seven modules processed in series, each with its own power switch, plus two
// parallel send busses (DELAY, REVERB) tapped post-AIR whose 100%-wet returns
// are summed back in before the clipper:
//
//   GATE → DE-ESS → EQ → COMP → HEAT → AIR ─┬────────────────┬→ CLIP → OUTPUT
//                                           ├→ DELAY send  ──┤
//                                           └→ REVERB send ──┘
//
// Each module is the corresponding suite plugin's engine, driven by the rack's
// simplified macro parameters, so the modules sound identical to the siblings:
//
//   GATE    VocalGate's hybrid detector + state machine (fixed timing
//           internals; only threshold + release are exposed).
//   DE-ESS  VocalEss's split-band ducker (Linkwitz-Riley split at essFreq,
//           the high band alone is ducked; amount maps to threshold depth).
//   EQ      HPF 12 dB/oct + mud dip (350 Hz) + presence (3.5 kHz) + air shelf
//           (12 kHz), juce::dsp IIR per channel.
//   COMP    VocalComp's Compressor engine (ARC voicing): amount drives the
//           threshold -10..-40 dB at fixed 3:1 / 5 ms / 80 ms with auto makeup.
//   HEAT    VocalGrit's GRIT stage (Warm curve): drive into a 4x-oversampled
//           tanh(x + bias) shaper with DC blocker, level-compensated, plus a
//           one-pole tilt tone around 1.2 kHz.
//   AIR     VocalAir's AirEnhancer high band (13.5 kHz dynamic shelf +
//           harmonic excitement); amount maps 0..+6 dB of shelf.
//   DELAY   send bus on VocalDelay's DelayEngine (Dual mode, no modulation,
//           analog feedback colour): repeats high-passed at 300 Hz and
//           low-passed at 6 kHz so they sit behind the vocal.
//   REVERB  send bus on VocalVerb's Dattorro plate (PlateReverb): input
//           high-passed at 250 Hz, wet high-cut at 9 kHz; size maps to tank
//           size + decay time.
//   CLIP    VocalClip's ClipEngine, fixed SOFT curve at a -0.3 dBFS ceiling,
//           4x oversampled; amount maps to 0..12 dB drive. No makeup after.
//==============================================================================
class VocalRackProcessor : public juce::AudioProcessor
{
public:
    VocalRackProcessor();
    ~VocalRackProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 4.0; }   // delay/reverb tails

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Live metering (read by the editor's timer).
    std::atomic<float> inDb      { -100.0f }; // input peak, pre-chain
    std::atomic<float> outDb     { -100.0f }; // output peak, post-chain
    std::atomic<float> gateGrDb  { 0.0f };    // gate attenuation this block (>= 0)
    std::atomic<float> essGrDb   { 0.0f };    // de-ess reduction this block (>= 0)
    std::atomic<float> compGrDb  { 0.0f };    // comp reduction this block (>= 0)
    std::atomic<float> clipDb    { 0.0f };    // dB clipped off this block (>= 0)

    // License engine (hard lock — DSP is bypassed until activated). The editor
    // reads this to show/hide its activation overlay.
    LicenseManager license { "VocalRack" };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void applyProgram (int index);
    int  currentProgram = 0;

    using Dup = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                               juce::dsp::IIR::Coefficients<float>>;

    // ---- GATE (fixed internals: atk 0.5 ms, hold 25 ms, range -25 dB,
    //      hysteresis 4 dB, sidechain HPF 100 Hz) ----
    Dup gateScFilter;
    juce::AudioBuffer<float> gateScBuffer;
    float gatePeakEnv = 0.0f, gateRmsSq = 0.0f;
    float gatePeakAtkCoeff = 0.0f, gatePeakRelCoeff = 0.0f, gateRmsCoeff = 0.0f;
    enum class GateState { Closed, Open, Hold };
    GateState gateState = GateState::Closed;
    int   gateHoldRemaining = 0;
    float gateGain = 1.0f;

    // ---- DE-ESS ----
    juce::dsp::LinkwitzRileyFilter<float> essCrossover;
    Dup essScFilter;
    juce::AudioBuffer<float> essScBuffer;
    float essEnv = 0.0f, essAtkCoeff = 0.0f, essRelCoeff = 0.0f;
    float lastEssFreq = -1.0f;

    // ---- EQ ----
    Dup eqHpfFilter, eqMudFilter, eqPresFilter, eqAirFilter;
    float lastEqHpf = -1.0f, lastEqMud = -999.0f, lastEqPres = -999.0f, lastEqAir = -999.0f;

    // ---- COMP (VocalComp engine) ----
    Compressor comp;

    // ---- HEAT (VocalGrit's GRIT stage, Warm curve, 4x oversampled) ----
    std::unique_ptr<juce::dsp::Oversampling<float>> heatOversampling;
    float heatDcX1[2] = { 0.0f, 0.0f }, heatDcY1[2] = { 0.0f, 0.0f };
    float heatTiltLp[2] = { 0.0f, 0.0f };

    // ---- AIR (VocalAir engine, high band only) ----
    AirEnhancer air;

    // ---- DELAY send (VocalDelay engine; tap post-AIR) ----
    DelayEngine delayEngine;
    juce::AudioBuffer<float> dlyBuf;
    bool wasDlyOn = false;

    // ---- REVERB send (VocalVerb plate; tap post-AIR, input HPF 250 Hz) ----
    PlateReverb verbEngine;
    Dup verbHpfFilter;
    juce::AudioBuffer<float> verbBuf;
    bool wasVerbOn = false;

    // ---- CLIP (VocalClip engine, fixed SOFT shape) ----
    ClipEngine clipEngine;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalRackProcessor)
};
