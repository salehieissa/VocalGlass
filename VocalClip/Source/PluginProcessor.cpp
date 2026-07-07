#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
VocalClipProcessor::VocalClipProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    license.loadCachedAndValidate();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
VocalClipProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "drive", 1 }, "Drive",
        NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f));

    layout.add (std::make_unique<AudioParameterChoice>(
        ParameterID { "shape", 1 }, "Shape",
        StringArray { "Soft", "Warm", "Hard" }, 0));

    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "ceiling", 1 }, "Ceiling",
        NormalisableRange<float> (-12.0f, 0.0f, 0.1f), -0.3f));

    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f));

    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "output", 1 }, "Output",
        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { "hq", 1 }, "HQ", true));

    return layout;
}

//==============================================================================
void VocalClipProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock, 2);
    const bool hq = apvts.getRawParameterValue ("hq")->load() > 0.5f;
    setLatencySamples (engine.latencySamples (hq));
}

bool VocalClipProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

//==============================================================================
void VocalClipProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // License gate: until activated, pass audio through clean (no processing).
    if (! license.isActivated())
        return;

    ClipEngine::Params p;
    p.driveDb   = apvts.getRawParameterValue ("drive")->load();
    p.shape     = (int) apvts.getRawParameterValue ("shape")->load();
    p.ceilingDb = apvts.getRawParameterValue ("ceiling")->load();
    p.mix       = apvts.getRawParameterValue ("mix")->load() * 0.01f;
    p.outDb     = apvts.getRawParameterValue ("output")->load();
    p.hq        = apvts.getRawParameterValue ("hq")->load() > 0.5f;
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
            { "Default",        { {"drive",6},  {"shape",1}, {"ceiling",-0.3f}, {"mix",100}, {"output",0}, {"hq",1} } },
            // The loudness stage of a modern rap/R&B vocal chain: warm curve,
            // moderate drive, ceiling just under full scale.
            { "Drake Vocal",    { {"drive",6},  {"shape",1}, {"ceiling",-0.3f}, {"mix",100}, {"output",0}, {"hq",1} } },
            { "Gentle Glue",    { {"drive",3},  {"shape",0}, {"ceiling",-0.5f}, {"mix",100}, {"output",0}, {"hq",1} } },
            { "Slammed",        { {"drive",12}, {"shape",2}, {"ceiling",-0.1f}, {"mix",100}, {"output",0}, {"hq",1} } },
            { "Parallel Crush", { {"drive",15}, {"shape",1}, {"ceiling",-1.0f}, {"mix",40},  {"output",0}, {"hq",1} } },
            { "Signature",      { {"drive",8},  {"shape",1}, {"ceiling",-0.3f}, {"mix",100}, {"output",0}, {"hq",1} } },
        };
        return presets;
    }
}

int VocalClipProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String VocalClipProcessor::getProgramName (int index)
{
    const auto& presets = getPresets();
    if (juce::isPositiveAndBelow (index, (int) presets.size()))
        return presets[(size_t) index].name;
    return {};
}

void VocalClipProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;
    currentProgram = index;
    applyProgram (index);
}

void VocalClipProcessor::applyProgram (int index)
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
juce::AudioProcessorEditor* VocalClipProcessor::createEditor()
{
    return new VocalClipEditor (*this);
}

void VocalClipProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void VocalClipProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalClipProcessor();
}
