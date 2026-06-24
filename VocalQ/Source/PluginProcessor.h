#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/EQBand.h"
#include "dsp/SpectrumAnalyzer.h"

#include "../../common/Licensing/LicenseManager.h"

//==============================================================================
// VocalQ — an 8-band dynamic vocal EQ.
//
// Each band is a parametric filter (bell / shelf / cut / notch) whose gain can
// react to the signal (threshold / attack / release / range), processed on
// Stereo, Mid or Side. A live response curve and output meters feed the UI.
//==============================================================================
class VocalQProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kNumBands = 8;

    VocalQProcessor();
    ~VocalQProcessor() override = default;

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

    // Bands (UI reads their published shape for the curve).
    std::array<EQBand, kNumBands> bands;

    // UI state shared with the editor.
    std::atomic<int>   soloBand { -1 };
    std::atomic<float> outLDb { -100.0f }, outRDb { -100.0f };

    // License engine (hard lock — DSP is bypassed until activated). The editor
    // reads this to show/hide its activation overlay.
    LicenseManager license { "VocalQ" };

    // Live input spectrum for the response display.
    SpectrumAnalyzer analyzer;

    // The rate the band coefficients are designed at (= host rate * oversampling
    // factor). The curve display uses this so the drawn response matches the
    // oversampled audio exactly (no top-octave "cramping" mismatch).
    double getSampleRateHz() const { return eqDesignRate; }

    static juce::String bandParamId (int band, const char* suffix);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void cacheParamPointers();
    void applyProgram (int index);

    struct BandPtrs
    {
        std::atomic<float>* on; std::atomic<float>* type; std::atomic<float>* freq;
        std::atomic<float>* q;  std::atomic<float>* gain; std::atomic<float>* range;
        std::atomic<float>* thresh; std::atomic<float>* atk; std::atomic<float>* rel;
        std::atomic<float>* chan;
    };
    std::array<BandPtrs, kNumBands> bp;
    std::atomic<float>* outPtr = nullptr;

    // 2x oversampling around the band processing. Running the biquads at twice
    // the rate pushes the audio Nyquist well above 20 kHz, so bell/shelf bands no
    // longer "cramp" (lose accuracy) in the top octave — the response matches the
    // drawn curve up to the edge of hearing, the way Pro-Q does.
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    static constexpr int kOsLog2 = 1; // 1 => 2x

    // Solo audition filters: two cascaded biquads per channel so the audition is
    // a tight, well-defined listening window (bandpass / lowpass / highpass
    // depending on band type).
    std::array<juce::dsp::IIR::Filter<float>, 2> soloFilt;
    std::array<juce::dsp::IIR::Filter<float>, 2> soloFiltB;
    // Cache of the last inputs used to build the solo coefficients, so we only
    // rebuild when the soloed band / type / freq / q actually changes.
    int    soloCoeffBand = -1, soloCoeffType = -1;
    double soloCoeffFreq = 0.0, soloCoeffQ = 0.0;
    juce::AudioBuffer<float> dryBuffer;
    double currentSampleRate = 44100.0;
    double eqDesignRate = 44100.0;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalQProcessor)
};
