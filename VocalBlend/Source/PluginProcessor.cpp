#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
VocalBlendProcessor::VocalBlendProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    // Load any cached activation and validate online in the background.
    license.loadCachedAndValidate();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout VocalBlendProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "mode", 1 }, "Mode",
        StringArray { "Vocal", "Even", "Beat" }, 1));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "blend", 1 }, "Blend",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "glue", 1 }, "Glue",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "warmth", 1 }, "Warmth",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "air", 1 }, "Air",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "width", 1 }, "Width",
        NormalisableRange<float> (0.0f, 150.0f, 0.1f), 100.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "output", 1 }, "Output",
        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "limit", 1 }, "Limit", true));

    return layout;
}

//==============================================================================
void VocalBlendProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    engine.prepare (spec);
}

bool VocalBlendProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void VocalBlendProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    // License gate: until activated, pass audio through clean (no processing).
    if (! license.isActivated())
        return;

    BlendEngine::Params p;
    p.mode   = (int) apvts.getRawParameterValue ("mode")->load();
    p.blend  = apvts.getRawParameterValue ("blend")->load() * 0.01f;
    p.glue   = apvts.getRawParameterValue ("glue")->load() * 0.01f;
    p.warmth = apvts.getRawParameterValue ("warmth")->load() * 0.01f;
    p.air    = apvts.getRawParameterValue ("air")->load() * 0.01f;
    p.width  = apvts.getRawParameterValue ("width")->load() * 0.01f;
    p.outDb  = apvts.getRawParameterValue ("output")->load();
    p.limit  = apvts.getRawParameterValue ("limit")->load() > 0.5f;

    engine.setParams (p);
    engine.process (buffer);
}

//==============================================================================
juce::AudioProcessorEditor* VocalBlendProcessor::createEditor()
{
    return new VocalBlendEditor (*this);
}

//==============================================================================
// Presets.
//==============================================================================
namespace
{
    struct PresetDef
    {
        juce::String name;
        int   mode;      // 0 Vocal, 1 Even, 2 Beat
        float blend, glue, warmth, air, width, output;
        bool  limit;
    };

    const std::vector<PresetDef>& getPresets()
    {
        static const std::vector<PresetDef> presets = {
            //                      mode blend glue warm air  wdth  out   limit
            { "Default",             1,  50,   30,  25,  25,  100,  0.0f, true },
            { "Vocal Up",            0,  65,   35,  20,  35,  100,  0.0f, true },
            { "Beat Knock",          2,  40,   45,  40,  15,  105,  0.5f, true },
            { "Radio Ready",         1,  60,   50,  30,  40,  110,  1.0f, true },
            { "Warm Tape Glue",      1,  35,   55,  60,  10,  95,   0.0f, true },
            { "Wide & Airy",         1,  45,   25,  15,  55,  135,  0.0f, true },
            { "Signature",           0,  55,   40,  30,  35,  110,  0.5f, true },
        };
        return presets;
    }
}

int VocalBlendProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String VocalBlendProcessor::getProgramName (int index)
{
    const auto& p = getPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? p[(size_t) index].name : juce::String();
}

void VocalBlendProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    applyProgram (index);
}

void VocalBlendProcessor::applyProgram (int index)
{
    const auto& presets = getPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    const auto& p = presets[(size_t) index];

    auto setFloat = [this] (const char* id, float v)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (v));
    };

    setFloat ("mode",   (float) p.mode);
    setFloat ("blend",  p.blend);
    setFloat ("glue",   p.glue);
    setFloat ("warmth", p.warmth);
    setFloat ("air",    p.air);
    setFloat ("width",  p.width);
    setFloat ("output", p.output);
    if (auto* param = apvts.getParameter ("limit"))
        param->setValueNotifyingHost (p.limit ? 1.0f : 0.0f);
}

//==============================================================================
void VocalBlendProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalBlendProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalBlendProcessor();
}
