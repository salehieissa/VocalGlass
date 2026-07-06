#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
namespace
{
    // Musical divisions, in quarter-note beats, with display labels. One LFO
    // cycle spans the division when synced.
    struct Division { const char* label; double beats; };

    const std::array<Division, 9> kDivisions {{
        { "1/1",  4.0       },
        { "1/2",  2.0       },
        { "1/4",  1.0       },
        { "1/8",  0.5       },
        { "1/16", 0.25      },
        { "1/4T", 2.0 / 3.0 },
        { "1/8T", 1.0 / 3.0 },
        { "1/4D", 1.5       },
        { "1/8D", 0.75      },
    }};

    juce::StringArray divisionLabels()
    {
        juce::StringArray a;
        for (auto& d : kDivisions) a.add (d.label);
        return a;
    }
}

//==============================================================================
VocalModProcessor::VocalModProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    // Load any cached activation and validate online in the background.
    license.loadCachedAndValidate();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout VocalModProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "mode", 1 }, "Mode",
        StringArray { "Chorus", "Flanger", "Phaser" }, ModEngine::Chorus));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "rate", 1 }, "Rate",
        NormalisableRange<float> (0.02f, 10.0f, 0.001f, 0.35f), 0.5f));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "sync", 1 }, "Sync", false));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "syncDiv", 1 }, "Division", divisionLabels(), 2)); // 1/4

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "depth", 1 }, "Depth",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "feedback", 1 }, "Feedback",
        NormalisableRange<float> (-95.0f, 95.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "width", 1 }, "Width",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "tone", 1 }, "Tone",
        NormalisableRange<float> (1000.0f, 20000.0f, 1.0f, 0.4f), 12000.0f));

    return layout;
}

//==============================================================================
void VocalModProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    engine.prepare (spec);
}

bool VocalModProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void VocalModProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    // License gate: until activated, pass audio through clean (no processing).
    if (! license.isActivated())
        return;

    // Resolve the LFO rate: free Hz, or one cycle per division at the host BPM.
    float rateHz = apvts.getRawParameterValue ("rate")->load();

    if (apvts.getRawParameterValue ("sync")->load() > 0.5f)
    {
        float bpm = 120.0f;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto hostBpm = pos->getBpm())
                    bpm = (float) *hostBpm;
        bpm = juce::jlimit (40.0f, 300.0f, bpm);

        const int divIdx = juce::jlimit (0, (int) kDivisions.size() - 1,
                                         (int) apvts.getRawParameterValue ("syncDiv")->load());
        const double beats = kDivisions[(size_t) divIdx].beats;
        rateHz = juce::jlimit (0.02f, 20.0f, (float) (bpm / (60.0 * beats)));
    }

    displayRate.store (rateHz);

    ModEngine::Params p;
    p.mode     = (int) apvts.getRawParameterValue ("mode")->load();
    p.rateHz   = rateHz;
    p.depth    = apvts.getRawParameterValue ("depth")->load() * 0.01f;
    p.feedback = apvts.getRawParameterValue ("feedback")->load() * 0.01f;
    p.mix      = apvts.getRawParameterValue ("mix")->load() * 0.01f;
    p.width    = apvts.getRawParameterValue ("width")->load() * 0.01f;
    p.toneHz   = apvts.getRawParameterValue ("tone")->load();

    engine.setParams (p);
    engine.process (buffer);
}

//==============================================================================
juce::String VocalModProcessor::getDivisionText() const
{
    const int divIdx = juce::jlimit (0, (int) kDivisions.size() - 1,
                                     (int) apvts.getRawParameterValue ("syncDiv")->load());
    return kDivisions[(size_t) divIdx].label;
}

//==============================================================================
juce::AudioProcessorEditor* VocalModProcessor::createEditor()
{
    return new VocalModEditor (*this);
}

//==============================================================================
// Presets.
//==============================================================================
namespace
{
    struct PresetDef
    {
        juce::String name;
        int   mode;       // 0 Chorus, 1 Flanger, 2 Phaser
        float rate;
        bool  sync;
        int   syncDiv;    // index into kDivisions
        float depth;
        float feedback;
        float mix;
        float width;
        float tone;
    };

    const std::vector<PresetDef>& getPresets()
    {
        // div indices: 0=1/1 1=1/2 2=1/4 3=1/8 4=1/16 5=1/4T 6=1/8T 7=1/4D 8=1/8D
        static const std::vector<PresetDef> presets = {
            //                        mode                rate   sync  div  dpth fb   mix  wdth tone
            { "Default",              ModEngine::Chorus,  0.50f, false, 2,  50,  0,   50,  100, 12000 },
            { "Vocal Chorus",         ModEngine::Chorus,  0.40f, false, 2,  45,  0,   35,  100, 10000 },
            { "Lush Wide",            ModEngine::Chorus,  0.25f, false, 2,  65,  0,   45,  100, 12000 },
            { "Jet Flange",           ModEngine::Flanger, 0.15f, false, 2,  80,  70,  50,  100, 14000 },
            { "Soft Phase",           ModEngine::Phaser,  0.30f, false, 2,  50,  30,  40,  100, 12000 },
            { "Signature",            ModEngine::Chorus,  0.35f, false, 2,  30,  0,   22,  85,  9000  },
        };
        return presets;
    }
}

int VocalModProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String VocalModProcessor::getProgramName (int index)
{
    const auto& p = getPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? p[(size_t) index].name : juce::String();
}

void VocalModProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    applyProgram (index);
}

void VocalModProcessor::applyProgram (int index)
{
    const auto& presets = getPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    const auto& p = presets[(size_t) index];

    auto setChoice = [this] (const char* id, int v)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 ((float) v));
    };
    auto setFloat = [this] (const char* id, float v)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (v));
    };
    auto setBool = [this] (const char* id, bool v)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (v ? 1.0f : 0.0f);
    };

    setChoice ("mode",     p.mode);
    setFloat  ("rate",     p.rate);
    setBool   ("sync",     p.sync);
    setChoice ("syncDiv",  p.syncDiv);
    setFloat  ("depth",    p.depth);
    setFloat  ("feedback", p.feedback);
    setFloat  ("mix",      p.mix);
    setFloat  ("width",    p.width);
    setFloat  ("tone",     p.tone);
}

//==============================================================================
void VocalModProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalModProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalModProcessor();
}
