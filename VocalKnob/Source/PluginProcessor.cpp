#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
VocalKnobProcessor::VocalKnobProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    amountPtr = apvts.getRawParameterValue ("amount");
    modePtr   = apvts.getRawParameterValue ("mode");
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout VocalKnobProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "amount", 1 }, "Amount",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 64.0f));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "mode", 1 }, "Mode",
        StringArray { "Clean", "Warm", "Dirty", "Blown" }, Maximizer::Dirty));

    return layout;
}

//==============================================================================
void VocalKnobProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto chans = (juce::uint32) juce::jmax (1, getTotalNumInputChannels());

    // 4x oversampling (2 half-band stages) so the per-band tanh + master
    // soft-clip generate their harmonics above Nyquist instead of aliasing.
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        chans, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampling->initProcessing ((size_t) samplesPerBlock);

    juce::dsp::ProcessSpec spec { sampleRate * 4.0, (juce::uint32) (samplesPerBlock * 4), chans };
    engine.prepare (spec);

    setLatencySamples ((int) oversampling->getLatencyInSamples());
}

bool VocalKnobProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void VocalKnobProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    engine.setParams (amountPtr->load() * 0.01f, (int) modePtr->load());

    // Oversample -> run the maximizer at 4x -> downsample.
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
juce::AudioProcessorEditor* VocalKnobProcessor::createEditor()
{
    return new VocalKnobEditor (*this);
}

//==============================================================================
// Presets — each is an amount + voicing combination with a vocal-y name.
//==============================================================================
namespace
{
    struct PresetDef { juce::String name; float amount; int mode; };

    const std::vector<PresetDef>& getPresets()
    {
        // Clean mode adds make-up gain to the highs (tinny at high amounts),
        // so Clean presets stay at low/moderate amounts. Warm boosts the lows
        // and cuts highs — the go-to for body. Amounts are spread across modes
        // so loudness scales gently rather than jumping.
        static const std::vector<PresetDef> presets = {
            { "underground lead", 64.0f, Maximizer::Dirty }, // matches param defaults
            { "clean polish",   28.0f, Maximizer::Clean },  // gentle, transparent
            { "pop sheen",      40.0f, Maximizer::Clean },  // moderate clean lift
            { "warm glue",      45.0f, Maximizer::Warm  },  // smooth low-end glue
            { "vocal thick",    58.0f, Maximizer::Warm  },  // full, thick body
            { "tape grit",      52.0f, Maximizer::Dirty },  // saturated character
            { "blown",          80.0f, Maximizer::Blown },  // aggressive
            { "full send",     100.0f, Maximizer::Blown },  // max
        };
        return presets;
    }
}

int VocalKnobProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String VocalKnobProcessor::getProgramName (int index)
{
    const auto& p = getPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? p[(size_t) index].name : juce::String();
}

void VocalKnobProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    applyProgram (index);
}

void VocalKnobProcessor::applyProgram (int index)
{
    const auto& presets = getPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    const auto& p = presets[(size_t) index];

    if (auto* a = apvts.getParameter ("amount"))
        a->setValueNotifyingHost (a->getNormalisableRange().convertTo0to1 (p.amount));
    if (auto* m = apvts.getParameter ("mode"))
        m->setValueNotifyingHost (m->getNormalisableRange().convertTo0to1 ((float) p.mode));
}

//==============================================================================
void VocalKnobProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalKnobProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    return new VocalKnobProcessor();
}
