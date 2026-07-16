#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <map>
#include "dsp/GeekEngine.h"

#include "../../common/Licensing/LicenseManager.h"

//==============================================================================
// VocalGeek — the handheld dose console. Five cartridges (lean / smoke / acid /
// snow / geeked), a DOSE macro, output trim, and a performance pad: HIT A
// stutter, HIT B tape stop, PRINT freeze, TAP division cycle, D-pad texture and
// space nudges.
//==============================================================================
class VocalGeekProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener,
                           private juce::AsyncUpdater
{
public:
    VocalGeekProcessor();
    ~VocalGeekProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    GeekEngine engine;

    // License engine (hard lock — DSP is bypassed until activated). The editor
    // reads this to show/hide its activation overlay.
    LicenseManager license { "VocalGeek" };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void applyProgram (int index);

    // Per-cartridge memory: every cartridge keeps its own knob settings, so
    // swapping snow -> lean -> snow brings snow back exactly as you left it.
    void parameterChanged (const juce::String& id, float newValue) override;
    void handleAsyncUpdate() override;              // applies the snapshot on the message thread
    void snapshotTheme (int theme);
    void recallTheme (int theme);

    static constexpr int numThemes = 6;
    static const char* const* snapshotIds();        // params captured per cartridge
    std::array<std::map<juce::String, float>, numThemes> themeMemory;
    std::atomic<int> pendingRecallTheme { -1 };
    int lastTheme = 0;
    bool recalling = false;

    double currentSampleRate = 44100.0;
    int currentProgram = 0;
    bool midiHeldA = false, midiHeldB = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalGeekProcessor)
};
