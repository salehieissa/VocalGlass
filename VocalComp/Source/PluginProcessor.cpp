#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
VocalCompProcessor::VocalCompProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    threshPtr  = apvts.getRawParameterValue ("threshold");
    ratioPtr   = apvts.getRawParameterValue ("ratio");
    attackPtr  = apvts.getRawParameterValue ("attack");
    releasePtr = apvts.getRawParameterValue ("release");
    makeupPtr  = apvts.getRawParameterValue ("makeup");
    mixPtr     = apvts.getRawParameterValue ("mix");
    trimPtr    = apvts.getRawParameterValue ("trim");
    modePtr    = apvts.getRawParameterValue ("mode");
    gatePtr    = apvts.getRawParameterValue ("gate");

    // Load any cached activation and validate online in the background.
    license.loadCachedAndValidate();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout VocalCompProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "threshold", 1 }, "Threshold",
        NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -28.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "ratio", 1 }, "Ratio",
        NormalisableRange<float> (1.0f, 20.0f, 0.01f, 0.5f), 2.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "attack", 1 }, "Attack",
        NormalisableRange<float> (0.1f, 300.0f, 0.1f, 0.4f), 20.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "release", 1 }, "Release",
        NormalisableRange<float> (5.0f, 2000.0f, 1.0f, 0.4f), 200.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "makeup", 1 }, "Gain",
        NormalisableRange<float> (-12.0f, 24.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "trim", 1 }, "Trim",
        NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "mode", 1 }, "Mode",
        StringArray { "ARC", "Opto", "Warm" }, Compressor::Arc));

    // Gate threshold; the bottom of the range (-80) means the gate is off.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "gate", 1 }, "Gate",
        NormalisableRange<float> (-80.0f, 0.0f, 0.1f), -80.0f));

    return layout;
}

//==============================================================================
void VocalCompProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto chans = (juce::uint32) juce::jmax (1, getTotalNumInputChannels());

    // 4x oversampling so Warm-mode tanh saturation doesn't alias on transients.
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        chans, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampling->initProcessing ((size_t) samplesPerBlock);

    juce::dsp::ProcessSpec spec { sampleRate * 4.0, (juce::uint32) (samplesPerBlock * 4), chans };
    engine.prepare (spec);

    setLatencySamples ((int) oversampling->getLatencyInSamples());
}

bool VocalCompProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void VocalCompProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    // License gate: until activated, pass audio through clean (no processing).
    if (! license.isActivated())
        return;

    engine.setParams (threshPtr->load(),
                      ratioPtr->load(),
                      attackPtr->load(),
                      releasePtr->load(),
                      makeupPtr->load(),
                      mixPtr->load() * 0.01f,
                      trimPtr->load(),
                      (int) modePtr->load(),
                      gatePtr->load());

    // Oversample -> compress at 4x -> downsample.
    juce::dsp::AudioBlock<float> base (buffer);
    auto os = oversampling->processSamplesUp (base);

    const int osCh = (int) os.getNumChannels();
    const int osN  = (int) os.getNumSamples();
    float* ptrs[2] = { nullptr, nullptr };
    for (int ch = 0; ch < osCh && ch < 2; ++ch) ptrs[ch] = os.getChannelPointer ((size_t) ch);
    juce::AudioBuffer<float> osBuf (ptrs, juce::jmin (osCh, 2), osN);
    engine.process (osBuf);

    oversampling->processSamplesDown (base);
}

//==============================================================================
juce::AudioProcessorEditor* VocalCompProcessor::createEditor()
{
    return new VocalCompEditor (*this);
}

//==============================================================================
// Presets — threshold, ratio, attack, release, makeup, mix, trim, mode.
//==============================================================================
namespace
{
    struct PresetDef
    {
        juce::String name;
        float threshold, ratio, attack, release, makeup, mix, trim;
        int   mode;
        float gate;
    };

    const std::vector<PresetDef>& getPresets()
    {
        // Makeup gains roughly compensate the typical gain reduction so every
        // preset stays full and consistent in loudness (never thin / tinny).
        // Note: Opto internally slows attack x2.2 and release x3.0; Warm adds
        // gentle tanh saturation after makeup, so its makeup is kept modest.
        static const std::vector<PresetDef> presets = {
            //  name              thr     ratio  atk    rel     mk    mix    trim  mode             gate
            // [0] matches the plugin's default parameter state exactly.
            { "Init",          -28.0f,  2.0f,  20.0f, 200.0f,  0.0f, 100.0f, 0.0f, Compressor::Arc,  -80.0f },
            { "Vocal Glue",    -24.0f,  2.0f,  30.0f, 250.0f,  3.0f, 100.0f, 0.0f, Compressor::Arc,  -80.0f },
            { "Lead Vocal",    -26.0f,  3.0f,  12.0f, 160.0f,  4.5f, 100.0f, 0.0f, Compressor::Arc,  -80.0f },
            { "Punch",         -28.0f,  4.0f,   6.0f,  80.0f,  5.0f, 100.0f, 0.0f, Compressor::Arc,  -80.0f },
            { "Smooth Opto",   -28.0f,  3.0f,  15.0f, 220.0f,  4.0f, 100.0f, 0.0f, Compressor::Opto, -80.0f },
            { "Parallel Warm", -34.0f,  5.0f,   8.0f, 140.0f,  5.0f,  45.0f, 1.0f, Compressor::Warm, -80.0f },
            { "Aggressive",    -34.0f,  8.0f,   4.0f, 110.0f,  6.0f, 100.0f, 0.0f, Compressor::Arc,  -80.0f },
            { "Gated Tight",   -24.0f,  3.0f,  10.0f, 140.0f,  3.5f, 100.0f, 0.0f, Compressor::Arc,  -55.0f },
        };
        return presets;
    }
}

int VocalCompProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String VocalCompProcessor::getProgramName (int index)
{
    const auto& p = getPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? p[(size_t) index].name : juce::String();
}

void VocalCompProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    applyProgram (index);
}

void VocalCompProcessor::applyProgram (int index)
{
    const auto& presets = getPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    const auto& p = presets[(size_t) index];

    auto set = [this] (const char* id, float value)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (value));
    };

    set ("threshold", p.threshold);
    set ("ratio",     p.ratio);
    set ("attack",    p.attack);
    set ("release",   p.release);
    set ("makeup",    p.makeup);
    set ("mix",       p.mix);
    set ("trim",      p.trim);
    set ("mode",      (float) p.mode);
    set ("gate",      p.gate);
}

//==============================================================================
void VocalCompProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalCompProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalCompProcessor();
}
