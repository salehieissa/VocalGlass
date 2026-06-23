#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
VocalDoublerProcessor::VocalDoublerProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    separationPtr = apvts.getRawParameterValue ("separation");
    variationPtr  = apvts.getRawParameterValue ("variation");
    amountPtr     = apvts.getRawParameterValue ("amount");
    effectOnlyPtr = apvts.getRawParameterValue ("effectOnly");
    modRatePtr    = apvts.getRawParameterValue ("modRate");
    modSyncPtr    = apvts.getRawParameterValue ("modSync");
    modDivPtr     = apvts.getRawParameterValue ("modDiv");
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout VocalDoublerProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "separation", 1 }, "Separation",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 56.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "variation", 1 }, "Variation",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "amount", 1 }, "Amount",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 60.0f));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "effectOnly", 1 }, "Effect Only", false));

    // Master modulation rate (Hz) scaling the voices' LFOs. Default matches the
    // engine's characteristic base rate so sync-off behaviour is unchanged.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "modRate", 1 }, "Mod Rate",
        NormalisableRange<float> (0.05f, 5.0f, 0.001f, 0.4f), 0.2f));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "modSync", 1 }, "Mod Sync", false));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "modDiv", 1 }, "Mod Division",
        StringArray { "1/1", "1/2", "1/4", "1/4.", "1/8", "1/8.", "1/8T", "1/16" }, 2));

    return layout;
}

//==============================================================================
namespace
{
    // Note value as a fraction of a whole note for each "modDiv" choice index.
    constexpr float kDivisionFraction[] = {
        1.0f,            // 1/1
        0.5f,            // 1/2
        0.25f,           // 1/4
        0.375f,          // 1/4.  (dotted quarter)
        0.125f,          // 1/8
        0.1875f,         // 1/8.  (dotted eighth)
        0.125f * 2.0f / 3.0f, // 1/8T (eighth triplet)
        0.0625f          // 1/16
    };
}

//==============================================================================
void VocalDoublerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    engine.prepare (spec);
}

bool VocalDoublerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void VocalDoublerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    if (bypassed.load())
        return; // dry signal passes through untouched

    // --- determine the master modulation rate (Hz) ---
    float baseRateHz = modRatePtr->load();

    if (modSyncPtr->load() > 0.5f)
    {
        double bpm = 120.0; // fallback when the host exposes no tempo
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto hostBpm = pos->getBpm())
                    bpm = *hostBpm;

        const double quarterSeconds = 60.0 / juce::jmax (1.0, bpm);

        const int divIdx = juce::jlimit (0, (int) (sizeof (kDivisionFraction) / sizeof (float)) - 1,
                                         (int) modDivPtr->load());
        const float beatsPerDivision = 4.0f * kDivisionFraction[divIdx];

        const float syncedHz = (float) (1.0 / (quarterSeconds * (double) beatsPerDivision));
        baseRateHz = juce::jlimit (0.05f, 5.0f, syncedHz); // clamp to a sane slow LFO range
    }

    engine.setModRateScale (baseRateHz / kReferenceModRate);

    engine.setParams (separationPtr->load() * 0.01f,
                      variationPtr->load()  * 0.01f,
                      amountPtr->load()     * 0.01f,
                      effectOnlyPtr->load() > 0.5f);
    engine.process (buffer);
}

//==============================================================================
juce::AudioProcessorEditor* VocalDoublerProcessor::createEditor()
{
    return new VocalDoublerEditor (*this);
}

//==============================================================================
// Presets — separation / variation / amount combinations with double-y names.
//==============================================================================
namespace
{
    struct PresetDef
    {
        juce::String name;
        float separation;
        float variation;
        float amount;
        bool  effectOnly;
        float modRate;   // Hz
        bool  modSync;
        int   modDiv;    // choice index
    };

    const std::vector<PresetDef>& getPresets()
    {
        // Tasteful doubling: keep the dry centre present (effectOnly stays off),
        // hold variation in a musical range so nothing turns flangy/metallic, and
        // keep wet "amount" moderate so the low-end body is never thinned out.
        static const std::vector<PresetDef> presets = {
            //  name              sep    var    amt    fxOnly  rate   sync   div
            { "Default",          56.0f, 100.0f, 60.0f, false, 0.20f, false, 2 }, // matches param defaults
            { "Subtle Width",     30.0f,  22.0f, 32.0f, false, 0.16f, false, 2 },
            { "Classic Double",   52.0f,  36.0f, 55.0f, false, 0.20f, false, 2 },
            { "Wide Chorus-y",    82.0f,  62.0f, 60.0f, false, 0.45f, false, 2 },
            { "Tight Stack",      40.0f,  16.0f, 52.0f, false, 0.14f, false, 2 },
            { "Lush Quad",        74.0f,  52.0f, 70.0f, false, 0.30f, false, 2 },
            { "Hard Pan Wide",   100.0f,  42.0f, 62.0f, false, 0.24f, false, 2 },
        };
        return presets;
    }
}

int VocalDoublerProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String VocalDoublerProcessor::getProgramName (int index)
{
    const auto& p = getPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? p[(size_t) index].name : juce::String();
}

void VocalDoublerProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    applyProgram (index);
}

void VocalDoublerProcessor::applyProgram (int index)
{
    const auto& presets = getPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    const auto& p = presets[(size_t) index];

    auto setF = [this] (const char* id, float value)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (value));
    };

    setF ("separation", p.separation);
    setF ("variation",  p.variation);
    setF ("amount",     p.amount);
    setF ("modRate",    p.modRate);

    if (auto* e = apvts.getParameter ("effectOnly"))
        e->setValueNotifyingHost (p.effectOnly ? 1.0f : 0.0f);

    if (auto* s = apvts.getParameter ("modSync"))
        s->setValueNotifyingHost (p.modSync ? 1.0f : 0.0f);

    if (auto* dv = apvts.getParameter ("modDiv"))
        dv->setValueNotifyingHost (dv->getNormalisableRange().convertTo0to1 ((float) p.modDiv));
}

//==============================================================================
void VocalDoublerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalDoublerProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    return new VocalDoublerProcessor();
}
