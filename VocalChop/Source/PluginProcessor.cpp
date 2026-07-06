#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
namespace
{
    // Musical divisions in quarter-note beats. CHOP gets the fast/dotted end,
    // REFRESH the slower capture intervals.
    struct Division { const char* label; double beats; };

    const std::array<Division, 10> kChopDivs {{
        { "1/4",  1.0        },
        { "1/8",  0.5        },
        { "1/8D", 0.75       },
        { "1/8T", 1.0 / 3.0  },
        { "1/16", 0.25       },
        { "1/16D", 0.375     },
        { "1/16T", 1.0 / 6.0 },
        { "1/32", 0.125      },
        { "1/32T", 1.0 / 12.0 },
        { "1/64", 0.0625     },
    }};

    const std::array<Division, 6> kRefreshDivs {{
        { "1/1", 4.0  },
        { "1/2", 2.0  },
        { "1/2D", 3.0 },
        { "1/4", 1.0  },
        { "1/4D", 1.5 },
        { "1/8", 0.5  },
    }};

    juce::StringArray labelsOf (const auto& divs)
    {
        juce::StringArray a;
        for (auto& d : divs) a.add (d.label);
        return a;
    }
}

//==============================================================================
VocalChopProcessor::VocalChopProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    // Load any cached activation and validate online in the background.
    license.loadCachedAndValidate();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout VocalChopProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "chopDiv", 1 }, "Chop", labelsOf (kChopDivs), 4));      // 1/16

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "refreshDiv", 1 }, "Refresh", labelsOf (kRefreshDivs), 3)); // 1/4

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "freeze", 1 }, "Freeze", false));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "gate", 1 }, "Gate",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "fade", 1 }, "Fade",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "output", 1 }, "Output",
        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    return layout;
}

//==============================================================================
void VocalChopProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    engine.prepare (spec);
}

bool VocalChopProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void VocalChopProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    // License gate: until activated, pass audio through clean (no processing).
    if (! license.isActivated())
        return;

    double bpm = 120.0, ppq = -1.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto hostBpm = pos->getBpm()) bpm = *hostBpm;
            if (auto hostPpq = pos->getPpqPosition()) ppq = *hostPpq;
            playing = pos->getIsPlaying();
        }

    const int chopIdx = juce::jlimit (0, (int) kChopDivs.size() - 1,
                                      (int) apvts.getRawParameterValue ("chopDiv")->load());
    const int refIdx = juce::jlimit (0, (int) kRefreshDivs.size() - 1,
                                     (int) apvts.getRawParameterValue ("refreshDiv")->load());

    ChopEngine::Params p;
    p.bpm          = bpm;
    p.chopBeats    = kChopDivs[(size_t) chopIdx].beats;
    p.refreshBeats = juce::jmax (kChopDivs[(size_t) chopIdx].beats,
                                 kRefreshDivs[(size_t) refIdx].beats);
    p.hostPpq      = ppq;
    p.hostPlaying  = playing;
    p.freeze = apvts.getRawParameterValue ("freeze")->load() > 0.5f;
    p.gate   = apvts.getRawParameterValue ("gate")->load() * 0.01f;
    p.fade   = apvts.getRawParameterValue ("fade")->load() * 0.01f;
    p.mix    = apvts.getRawParameterValue ("mix")->load() * 0.01f;
    p.outDb  = apvts.getRawParameterValue ("output")->load();

    engine.setParams (p);
    engine.process (buffer);
}

//==============================================================================
juce::String VocalChopProcessor::getChopText() const
{
    const int i = juce::jlimit (0, (int) kChopDivs.size() - 1,
                                (int) apvts.getRawParameterValue ("chopDiv")->load());
    return kChopDivs[(size_t) i].label;
}

juce::String VocalChopProcessor::getRefreshText() const
{
    const int i = juce::jlimit (0, (int) kRefreshDivs.size() - 1,
                                (int) apvts.getRawParameterValue ("refreshDiv")->load());
    return kRefreshDivs[(size_t) i].label;
}

//==============================================================================
juce::AudioProcessorEditor* VocalChopProcessor::createEditor()
{
    return new VocalChopEditor (*this);
}

//==============================================================================
// Presets.
//==============================================================================
namespace
{
    struct PresetDef
    {
        juce::String name;
        int   chopDiv, refreshDiv;
        bool  freeze;
        float gate, fade, mix, output;
    };

    const std::vector<PresetDef>& getPresets()
    {
        // chop: 0=1/4 1=1/8 2=1/8D 3=1/8T 4=1/16 5=1/16D 6=1/16T 7=1/32 8=1/32T 9=1/64
        // refresh: 0=1/1 1=1/2 2=1/2D 3=1/4 4=1/4D 5=1/8
        static const std::vector<PresetDef> presets = {
            //                     chop ref  frz    gate fade mix  out
            { "Default",            4,   3,  false,  0,   0,  100, 0.0f },
            { "Classic Chop",       4,   1,  false,  35,  0,  100, 0.0f },
            { "Stutter Build",      7,   3,  false,  20,  25, 100, 0.0f },
            { "Half-Time Echoes",   1,   0,  false,  0,   45, 85,  0.0f },
            { "Glitch Tail",        8,   3,  false,  50,  60, 90,  0.0f },
            { "Trance Gate",        6,   1,  false,  65,  0,  100, 0.0f },
            { "Signature",          4,   1,  false,  30,  15, 95,  0.0f },
        };
        return presets;
    }
}

int VocalChopProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String VocalChopProcessor::getProgramName (int index)
{
    const auto& p = getPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? p[(size_t) index].name : juce::String();
}

void VocalChopProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    applyProgram (index);
}

void VocalChopProcessor::applyProgram (int index)
{
    const auto& presets = getPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    const auto& p = presets[(size_t) index];

    auto setFloat = [this] (const char* id, float v)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (v));
    };

    setFloat ("chopDiv",    (float) p.chopDiv);
    setFloat ("refreshDiv", (float) p.refreshDiv);
    if (auto* param = apvts.getParameter ("freeze"))
        param->setValueNotifyingHost (p.freeze ? 1.0f : 0.0f);
    setFloat ("gate",   p.gate);
    setFloat ("fade",   p.fade);
    setFloat ("mix",    p.mix);
    setFloat ("output", p.output);
}

//==============================================================================
void VocalChopProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalChopProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalChopProcessor();
}
