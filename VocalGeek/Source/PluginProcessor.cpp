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
    lastTheme = (int) apvts.getRawParameterValue ("theme")->load();
    apvts.addParameterListener ("theme", this);
}

VocalGeekProcessor::~VocalGeekProcessor()
{
    apvts.removeParameterListener ("theme", this);
    cancelPendingUpdate();
}

//==============================================================================
// Per-cartridge memory. Swapping cartridges stashes the outgoing cartridge's
// settings and restores whatever the incoming one was last set to, so every
// drug keeps its own dose/texture/space/rate/auto/output. First visit to a
// cartridge inherits the current settings (nothing to recall yet).
const char* const* VocalGeekProcessor::snapshotIds()
{
    static const char* const ids[] = { "dose", "texture", "space",
                                       "rate", "auto", "output", nullptr };
    return ids;
}

void VocalGeekProcessor::parameterChanged (const juce::String&, float newValue)
{
    if (recalling)
        return;
    pendingRecallTheme.store ((int) std::round (newValue));
    triggerAsyncUpdate();
}

void VocalGeekProcessor::handleAsyncUpdate()
{
    const int newTheme = pendingRecallTheme.exchange (-1);
    if (newTheme < 0 || newTheme >= numThemes || newTheme == lastTheme)
        return;

    snapshotTheme (lastTheme);
    recallTheme (newTheme);
    lastTheme = newTheme;
}

void VocalGeekProcessor::snapshotTheme (int theme)
{
    if (! juce::isPositiveAndBelow (theme, numThemes))
        return;
    auto& slot = themeMemory[(size_t) theme];
    for (auto id = snapshotIds(); *id != nullptr; ++id)
        slot[*id] = apvts.getRawParameterValue (*id)->load();
}

void VocalGeekProcessor::recallTheme (int theme)
{
    if (! juce::isPositiveAndBelow (theme, numThemes))
        return;
    const auto& slot = themeMemory[(size_t) theme];
    if (slot.empty())
        return;                       // never visited: keep current settings

    recalling = true;
    for (const auto& [id, value] : slot)
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    recalling = false;
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
VocalGeekProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice>(
        ParameterID { "theme", 1 }, "Cartridge",
        StringArray { "Lean", "Smoke", "Acid", "Snow", "Geeked", "Overdose" }, 0));

    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "dose", 1 }, "Dose",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 35.0f));

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

    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { "auto", 1 }, "Auto", false));

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
void VocalGeekProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // MIDI performance: C1 (36) holds HIT A, D1 (38) holds HIT B, E1 (40)
    // taps the rate — so the pads are playable from a keyboard while tracking.
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            if (m.getNoteNumber() == 36) midiHeldA = true;
            if (m.getNoteNumber() == 38) midiHeldB = true;
            if (m.getNoteNumber() == 40)
                if (auto* par = apvts.getParameter ("rate"))
                    par->setValueNotifyingHost (par->convertTo0to1 (
                        (float) (((int) apvts.getRawParameterValue ("rate")->load() + 1) % 4)));
        }
        else if (m.isNoteOff() || m.isAllNotesOff())
        {
            if (m.getNoteNumber() == 36 || m.isAllNotesOff()) midiHeldA = false;
            if (m.getNoteNumber() == 38 || m.isAllNotesOff()) midiHeldB = false;
        }
    }

    // License gate: until activated, pass audio through clean (no processing).
    if (! license.isActivated())
        return;

    GeekEngine::Params p;
    p.theme    = (int) apvts.getRawParameterValue ("theme")->load();
    p.dose     = apvts.getRawParameterValue ("dose")->load() * 0.01f;
    p.texture  = apvts.getRawParameterValue ("texture")->load() * 0.01f;
    p.space    = apvts.getRawParameterValue ("space")->load() * 0.01f;
    p.rate     = (int) apvts.getRawParameterValue ("rate")->load();
    p.hitA     = midiHeldA || apvts.getRawParameterValue ("hita")->load() > 0.5f;
    p.hitB     = midiHeldB || apvts.getRawParameterValue ("hitb")->load() > 0.5f;
    p.freeze   = apvts.getRawParameterValue ("print")->load() > 0.5f;
    p.autoMode = apvts.getRawParameterValue ("auto")->load() > 0.5f;
    p.outDb    = apvts.getRawParameterValue ("output")->load();

    if (auto* playhead = getPlayHead())
        if (auto pos = playhead->getPosition())
        {
            if (auto bpm = pos->getBpm())
                p.bpm = *bpm;
            if (pos->getIsPlaying())
                if (auto ppq = pos->getPpqPosition())
                    p.ppq = *ppq;
        }

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

    // preset values win over cartridge memory — suppress the recall
    recalling = true;
    cancelPendingUpdate();

    for (auto* ap : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& [id, value] : presets[(size_t) index].values)
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));

    recalling = false;
    lastTheme = (int) apvts.getRawParameterValue ("theme")->load();
}

//==============================================================================
juce::AudioProcessorEditor* VocalGeekProcessor::createEditor()
{
    return new VocalGeekEditor (*this);
}

void VocalGeekProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    snapshotTheme (lastTheme);   // fold the live settings into the memory first

    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
        {
            // cartridge memory rides along inside the session state
            auto* mem = xml->createNewChildElement ("THEMEMEMORY");
            for (int t = 0; t < numThemes; ++t)
            {
                if (themeMemory[(size_t) t].empty())
                    continue;
                auto* slot = mem->createNewChildElement ("SLOT");
                slot->setAttribute ("theme", t);
                for (const auto& [id, value] : themeMemory[(size_t) t])
                    slot->setAttribute (id, value);
            }
            copyXmlToBinary (*xml, destData);
        }
}

void VocalGeekProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            for (auto& slot : themeMemory)
                slot.clear();
            if (auto* mem = xml->getChildByName ("THEMEMEMORY"))
                for (auto* slot : mem->getChildIterator())
                {
                    const int t = slot->getIntAttribute ("theme", -1);
                    if (! juce::isPositiveAndBelow (t, numThemes))
                        continue;
                    for (auto id = snapshotIds(); *id != nullptr; ++id)
                        if (slot->hasAttribute (*id))
                            themeMemory[(size_t) t][*id]
                                = (float) slot->getDoubleAttribute (*id);
                }
            xml->deleteAllChildElementsWithTagName ("THEMEMEMORY");

            recalling = true;    // restored values win over cartridge memory
            cancelPendingUpdate();
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            recalling = false;
            lastTheme = (int) apvts.getRawParameterValue ("theme")->load();
        }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalGeekProcessor();
}
