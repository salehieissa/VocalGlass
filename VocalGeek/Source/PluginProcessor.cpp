#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
VocalGeekProcessor::VocalGeekProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    license.loadCachedAndValidate();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
VocalGeekProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice>(
        ParameterID { "theme", 1 }, "Cartridge",
        StringArray { "Lean", "Smoke", "Acid", "Snow", "Geeked" }, 0));

    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "dose", 1 }, "Dose",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f));

    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "texture", 1 }, "Texture",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f));

    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "space", 1 }, "Space",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f));

    layout.add (std::make_unique<AudioParameterChoice>(
        ParameterID { "rate", 1 }, "Rate",
        StringArray { "1/4", "1/8", "1/16", "1/32" }, 1));

    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { "hita", 1 }, "Hit A", false));

    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { "hitb", 1 }, "Hit B", false));

    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { "print", 1 }, "Print", false));

    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "output", 1 }, "Output",
        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    return layout;
}

//==============================================================================
void VocalGeekProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock, 2);
}

bool VocalGeekProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

//==============================================================================
void VocalGeekProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // License gate: until activated, pass audio through clean (no processing).
    if (! license.isActivated())
        return;

    GeekEngine::Params p;
    p.theme   = (int) apvts.getRawParameterValue ("theme")->load();
    p.dose    = apvts.getRawParameterValue ("dose")->load() * 0.01f;
    p.texture = apvts.getRawParameterValue ("texture")->load() * 0.01f;
    p.space   = apvts.getRawParameterValue ("space")->load() * 0.01f;
    p.rate    = (int) apvts.getRawParameterValue ("rate")->load();
    p.hitA    = apvts.getRawParameterValue ("hita")->load() > 0.5f;
    p.hitB    = apvts.getRawParameterValue ("hitb")->load() > 0.5f;
    p.freeze  = apvts.getRawParameterValue ("print")->load() > 0.5f;
    p.outDb   = apvts.getRawParameterValue ("output")->load();

    if (auto* playhead = getPlayHead())
        if (auto pos = playhead->getPosition())
            if (auto bpm = pos->getBpm())
                p.bpm = *bpm;

    engine.setParams (p);
    engine.process (buffer);
}

//==============================================================================
// Presets (exposed to the host's program/preset menu).
namespace
{
    struct PresetDef
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values;
    };

    const std::vector<PresetDef>& getPresets()
    {
        static const std::vector<PresetDef> presets =
        {
            { "Default",       { {"theme",0}, {"dose",50},  {"texture",50}, {"space",50}, {"rate",1}, {"output",0} } },
            { "Purple Drank",  { {"theme",0}, {"dose",75},  {"texture",35}, {"space",70}, {"rate",1}, {"output",0} } },
            { "Hotbox",        { {"theme",1}, {"dose",60},  {"texture",55}, {"space",55}, {"rate",1}, {"output",0} } },
            { "Third Eye",     { {"theme",2}, {"dose",70},  {"texture",60}, {"space",65}, {"rate",2}, {"output",0} } },
            { "Whiteout",      { {"theme",3}, {"dose",65},  {"texture",45}, {"space",40}, {"rate",2}, {"output",0} } },
            { "Zombieland",    { {"theme",4}, {"dose",85},  {"texture",70}, {"space",30}, {"rate",3}, {"output",-1} } },
        };
        return presets;
    }
}

int VocalGeekProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String VocalGeekProcessor::getProgramName (int index)
{
    const auto& presets = getPresets();
    if (juce::isPositiveAndBelow (index, (int) presets.size()))
        return presets[(size_t) index].name;
    return {};
}

void VocalGeekProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;
    currentProgram = index;
    applyProgram (index);
}

void VocalGeekProcessor::applyProgram (int index)
{
    const auto& presets = getPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    for (auto* ap : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& [id, value] : presets[(size_t) index].values)
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
}

//==============================================================================
juce::AudioProcessorEditor* VocalGeekProcessor::createEditor()
{
    return new VocalGeekEditor (*this);
}

void VocalGeekProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void VocalGeekProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalGeekProcessor();
}
