#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
VocalAirProcessor::VocalAirProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "PARAMS", createParameterLayout())
{
    midAirPtr  = apvts.getRawParameterValue ("midAir");
    highAirPtr = apvts.getRawParameterValue ("highAir");
    linkPtr    = apvts.getRawParameterValue ("link");
    powerPtr   = apvts.getRawParameterValue ("power");
    trimPtr    = apvts.getRawParameterValue ("trim");
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout VocalAirProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "midAir", 1 }, "Mid Air",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 24.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "highAir", 1 }, "High Air",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 31.0f));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "link", 1 }, "Link", false));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "power", 1 }, "Power", true));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "trim", 1 }, "Trim",
        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    return layout;
}

//==============================================================================
void VocalAirProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto chans = (juce::uint32) juce::jmax (1, getTotalNumInputChannels());

    // 4x oversampling so the high-frequency tanh excitement (6.5k / 13.5k) makes
    // its harmonics above Nyquist instead of folding back down as fizz.
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        chans, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampling->initProcessing ((size_t) samplesPerBlock);

    juce::dsp::ProcessSpec spec { sampleRate * 4.0, (juce::uint32) (samplesPerBlock * 4), chans };
    engine.prepare (spec);

    setLatencySamples ((int) oversampling->getLatencyInSamples());
}

bool VocalAirProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void VocalAirProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // License gate: silence output until this plugin is activated.
    if (! license.isLicensed())
    {
        buffer.clear();
        return;
    }
    const int n = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    engine.setParams (midAirPtr->load() * 0.01f,
                      highAirPtr->load() * 0.01f,
                      trimPtr->load(),
                      powerPtr->load() > 0.5f);

    // Oversample -> excite at 4x -> downsample.
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
juce::AudioProcessorEditor* VocalAirProcessor::createEditor()
{
    return new VocalAirEditor (*this);
}

//==============================================================================
// Presets — each is a mid/high air + trim combination.
//==============================================================================
namespace
{
    struct PresetDef { juce::String name; float midAir; float highAir; float trim; };

    const std::vector<PresetDef>& getPresets()
    {
        // VocalAir is a high-frequency exciter, so the tinny risk is highest
        // here. The shelves reach +12 dB at 100%, so air amounts are kept
        // CONSERVATIVE and biased toward "mid air" (6.5 kHz presence) over
        // "high air" (13.5 kHz) to add sheen without thinning the vocal. Trim
        // is pulled down slightly on brighter presets so switching programs
        // stays roughly loudness-matched against the default.
        static const std::vector<PresetDef> presets = {
            { "Natural",      24.0f, 31.0f,  0.0f },  // matches param defaults
            { "Silk",         16.0f, 22.0f,  0.0f },  // subtle sheen
            { "Presence",     40.0f, 16.0f, -0.5f },  // upper-mid lift, low air
            { "De-Dull",      36.0f, 28.0f, -0.5f },  // restores dull vocals
            { "Pop Sheen",    30.0f, 36.0f, -1.0f },  // modern pop top
            { "Crisp Top",    28.0f, 40.0f, -1.5f },  // crisp but body-safe
            { "Breathy",      22.0f, 44.0f, -1.5f },  // airy, intimate
            { "Max Air",      58.0f, 60.0f, -3.0f },  // aggressive, still tasteful
        };
        return presets;
    }
}

int VocalAirProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String VocalAirProcessor::getProgramName (int index)
{
    const auto& p = getPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? p[(size_t) index].name : juce::String();
}

void VocalAirProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    applyProgram (index);
}

void VocalAirProcessor::applyProgram (int index)
{
    const auto& presets = getPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    const auto& p = presets[(size_t) index];

    auto set = [this] (const char* id, float v)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (v));
    };

    set ("midAir",  p.midAir);
    set ("highAir", p.highAir);
    set ("trim",    p.trim);
}

//==============================================================================
void VocalAirProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalAirProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));

        // Re-broadcast discrete params at their snapped value so the host's
        // normalized cache reports an exact step (state-restoration correctness).
        for (auto* p : getParameters())
            if (const int steps = p->getNumSteps(); p->isDiscrete() && steps > 1)
                p->setValueNotifyingHost ((float) juce::roundToInt (p->getValue() * (float) (steps - 1))
                                          / (float) (steps - 1));
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalAirProcessor();
}
