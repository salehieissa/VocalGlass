#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Scales.h"

//==============================================================================
VocalTuneProcessor::VocalTuneProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "PARAMS", createParameterLayout())
{
    retunePtr   = apvts.getRawParameterValue ("retuneSpeed");
    humanizePtr = apvts.getRawParameterValue ("humanize");
    flexPtr     = apvts.getRawParameterValue ("flexTune");
    modernPtr   = apvts.getRawParameterValue ("modern");
    hqPtr       = apvts.getRawParameterValue ("hq");
    detunePtr   = apvts.getRawParameterValue ("detuneHz");
    rangePtr    = apvts.getRawParameterValue ("vocalRange");
    powerPtr    = apvts.getRawParameterValue ("power");
    keyScalePtr = apvts.getRawParameterValue ("keyScale");
    keyPtr      = apvts.getRawParameterValue ("key");
    for (int i = 0; i < 12; ++i)
    {
        const auto id = "note" + juce::String (i);
        notePtr[(size_t) i]   = apvts.getRawParameterValue (id);
        noteParam[(size_t) i] = apvts.getParameter (id);
    }

    // Recompute the note mask whenever key / scale changes.
    apvts.addParameterListener ("keyScale", this);
    apvts.addParameterListener ("key", this);
}

VocalTuneProcessor::~VocalTuneProcessor()
{
    apvts.removeParameterListener ("keyScale", this);
    apvts.removeParameterListener ("key", this);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout VocalTuneProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "vocalRange", 1 }, "Vocal Range",
        music::vocalRangeNames(), 1)); // default Alto/Tenor

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "keyScale", 1 }, "Key & Scale",
        music::scaleNames(), 0)); // default Chromatic

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "key", 1 }, "Key",
        music::keyNames(), 0)); // default C

    // The 12 note enables — default to chromatic (all on).
    for (int i = 0; i < 12; ++i)
        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { "note" + String (i), 1 },
            "Note " + music::noteNames()[i], true));

    // Defaults are biased toward strong, obvious tuning out of the box:
    // fast retune, no vibrato bleed, full snap (flex 0 = hardest).
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "retuneSpeed", 1 }, "Retune Speed",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 16.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "humanize", 1 }, "Humanize",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "flexTune", 1 }, "Flex Tune",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "modern", 1 }, "Modern", true));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "hq", 1 }, "HQ", false));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "detuneHz", 1 }, "Detune",
        NormalisableRange<float> (430.0f, 450.0f, 0.1f), 440.0f));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "power", 1 }, "Power", true));

    return layout;
}

//==============================================================================
void VocalTuneProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    corrector.prepare (sampleRate, samplesPerBlock, getTotalNumInputChannels());
    // Latency depends on the Low Latency / HQ mode. Report the current value here;
    // processBlock re-notifies the host only on a genuine change (never every
    // block, which makes some hosts glitch / reset their PDC).
    reportedLatency = corrector.getLatencySamples();
    setLatencySamples (reportedLatency);
}

bool VocalTuneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void VocalTuneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    if (rescaleFlag.exchange (false))
        applyScaleToNotes();

    std::array<bool, 12> enabled { };
    for (int i = 0; i < 12; ++i)
        enabled[(size_t) i] = notePtr[(size_t) i]->load() > 0.5f;

    const int rangeIdx = juce::jlimit (0, (int) music::vocalRanges().size() - 1,
                                       (int) rangePtr->load());
    const auto& r = music::vocalRanges()[(size_t) rangeIdx];

    corrector.setParams (enabled,
                         retunePtr->load() * 0.01f,
                         humanizePtr->load() * 0.01f,
                         flexPtr->load() * 0.01f,
                         modernPtr->load() > 0.5f,
                         hqPtr->load() > 0.5f,
                         detunePtr->load(),
                         r.minHz, r.maxHz,
                         powerPtr->load() > 0.5f);

    corrector.process (buffer);

    // The Low Latency / HQ switch changes the engine's delay. Tell the host only
    // when it actually flips, so plugin-delay compensation stays correct.
    const int lat = corrector.getLatencySamples();
    if (lat != reportedLatency)
    {
        reportedLatency = lat;
        setLatencySamples (lat);
    }
}

//==============================================================================
void VocalTuneProcessor::parameterChanged (const juce::String& id, float)
{
    if (id == "keyScale" || id == "key")
        rescaleFlag.store (true); // applied on the audio thread, no UI-thread param writes here
}

void VocalTuneProcessor::applyScaleToNotes()
{
    const int scaleIdx = (int) keyScalePtr->load();
    const int key      = (int) keyPtr->load();
    const auto mask = music::maskFor (scaleIdx, key);

    for (int i = 0; i < 12; ++i)
        if (auto* p = noteParam[(size_t) i])
            p->setValueNotifyingHost (mask[(size_t) i] ? 1.0f : 0.0f);
}

//==============================================================================
juce::AudioProcessorEditor* VocalTuneProcessor::createEditor()
{
    return new VocalTuneEditor (*this);
}

//==============================================================================
namespace
{
    struct PresetDef
    {
        juce::String name;
        float retune, humanize, flex;
        bool  modern, hq;
        int   scaleIndex;  // -1 = leave the user's current scale/key untouched
        int   key;         // -1 = leave key untouched
    };

    const std::vector<PresetDef>& getPresets()
    {
        // scaleIndex matches music::scales(): 0 Chromatic, 1 Major, 2 Minor...
        // flex tune: 0 = snap everything fully (hard), 100 = natural/loose.
        static const std::vector<PresetDef> presets = {
            // name           retune humanize flex  modern  hq     scale key
            { "Default",        16.0f,   0.0f,  0.0f, true,  false,  -1,  -1 },
            { "Hard Tune",       0.0f,   0.0f,  0.0f, false, false,  -1,  -1 },
            { "Pop Tight",      10.0f,   5.0f, 12.0f, true,  false,  -1,  -1 },
            { "Natural",        38.0f,  40.0f, 60.0f, true,  false,  -1,  -1 },
            { "Subtle",         55.0f,  60.0f, 80.0f, true,  false,  -1,  -1 },
            { "Robot",           0.0f,   0.0f,  0.0f, false, false,   0,  -1 },
        };
        return presets;
    }
}

int VocalTuneProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String VocalTuneProcessor::getProgramName (int index)
{
    const auto& p = getPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? p[(size_t) index].name : juce::String();
}

void VocalTuneProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    applyProgram (index);
}

void VocalTuneProcessor::applyProgram (int index)
{
    const auto& presets = getPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    const auto& p = presets[(size_t) index];

    auto setF = [this] (const char* id, float v)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (v));
    };
    setF ("retuneSpeed", p.retune);
    setF ("humanize",    p.humanize);
    setF ("flexTune",    p.flex);
    setF ("modern",      p.modern ? 1.0f : 0.0f);
    setF ("hq",          p.hq     ? 1.0f : 0.0f);

    // Optionally pin a scale/key. Writing these choice params trips the
    // parameterChanged listener, which re-derives the 12 note toggles.
    if (p.key >= 0)
        if (auto* param = apvts.getParameter ("key"))
            param->setValueNotifyingHost (param->convertTo0to1 ((float) p.key));
    if (p.scaleIndex >= 0)
        if (auto* param = apvts.getParameter ("keyScale"))
            param->setValueNotifyingHost (param->convertTo0to1 ((float) p.scaleIndex));
}

//==============================================================================
void VocalTuneProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalTuneProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    return new VocalTuneProcessor();
}
