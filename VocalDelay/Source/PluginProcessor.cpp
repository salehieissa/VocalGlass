#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
namespace
{
    // Musical divisions, in quarter-note beats, with display labels.
    struct Division { const char* label; double beats; };

    const std::array<Division, 12> kDivisions {{
        { "1/1",   4.0      },
        { "1/2",   2.0      },
        { "1/4",   1.0      },
        { "1/4D",  1.5      },
        { "1/4T",  2.0 / 3.0 },
        { "1/8",   0.5      },
        { "1/8D",  0.75     },
        { "1/8T",  1.0 / 3.0 },
        { "1/16",  0.25     },
        { "1/16D", 0.375    },
        { "1/16T", 1.0 / 6.0 },
        { "1/32",  0.125    },
    }};

    juce::StringArray divisionLabels()
    {
        juce::StringArray a;
        for (auto& d : kDivisions) a.add (d.label);
        return a;
    }
}

//==============================================================================
VocalDelayProcessor::VocalDelayProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    // Load any cached activation and validate online in the background.
    license.loadCachedAndValidate();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout VocalDelayProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "division", 1 }, "Division", divisionLabels(), 6)); // 1/8D

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "bpm", 1 }, "Tempo",
        NormalisableRange<float> (40.0f, 300.0f, 0.01f), 120.0f));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "syncMode", 1 }, "Sync",
        StringArray { "BPM", "HOST", "MS" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "msTime", 1 }, "Time",
        NormalisableRange<float> (1.0f, 2000.0f, 0.1f, 0.4f), 350.0f));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "mode", 1 }, "Mode",
        StringArray { "Left", "Ping Pong", "Dual", "Right" }, DelayEngine::Dual));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "feedback", 1 }, "Feedback",
        NormalisableRange<float> (0.0f, 110.0f, 0.1f), 40.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "depth", 1 }, "Depth",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "rate", 1 }, "Rate",
        NormalisableRange<float> (0.05f, 10.0f, 0.01f, 0.4f), 0.2f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "hipass", 1 }, "Hi-Pass",
        NormalisableRange<float> (20.0f, 2000.0f, 1.0f, 0.4f), 120.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "lopass", 1 }, "Lo-Pass",
        NormalisableRange<float> (1000.0f, 20000.0f, 1.0f, 0.4f), 20000.0f));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "filterLink", 1 }, "Filter Link", false));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "drywet", 1 }, "Dry/Wet",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "output", 1 }, "Output",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "analog", 1 }, "Analog",
        NormalisableRange<float> (0.0f, 10.0f, 0.1f), 2.0f));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "lofi", 1 }, "Lo-Fi", false));

    return layout;
}

//==============================================================================
void VocalDelayProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    engine.prepare (spec);
}

bool VocalDelayProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void VocalDelayProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    // License gate: until activated, pass audio through clean (no processing).
    if (! license.isActivated())
        return;

    const int   syncMode = (int) apvts.getRawParameterValue ("syncMode")->load();
    const int   divIdx   = juce::jlimit (0, (int) kDivisions.size() - 1,
                                         (int) apvts.getRawParameterValue ("division")->load());
    const float bpmParam = apvts.getRawParameterValue ("bpm")->load();
    const float msParam  = apvts.getRawParameterValue ("msTime")->load();

    // Resolve the actual delay time in milliseconds.
    float bpm = bpmParam;
    float delayMs;

    if (syncMode == 2) // MS — free milliseconds
    {
        delayMs = msParam;
    }
    else
    {
        if (syncMode == 1) // HOST tempo
        {
            if (auto* ph = getPlayHead())
                if (auto pos = ph->getPosition())
                    if (auto hostBpm = pos->getBpm())
                        bpm = (float) *hostBpm;
        }
        bpm = juce::jlimit (40.0f, 300.0f, bpm);
        const double beats = kDivisions[(size_t) divIdx].beats;
        delayMs = (float) (beats * 60000.0 / bpm);
    }

    displayBpm.store (bpm);
    displayMs.store (delayMs);

    const float delaySamples = juce::jlimit (1.0f, (float) (currentSampleRate * 2.0),
                                             delayMs * 0.001f * (float) currentSampleRate);

    DelayEngine::Params p;
    p.delaySamplesL = delaySamples;
    p.delaySamplesR = delaySamples;
    p.mode       = (int) apvts.getRawParameterValue ("mode")->load();
    p.feedback   = apvts.getRawParameterValue ("feedback")->load() * 0.01f;
    p.depth      = apvts.getRawParameterValue ("depth")->load() * 0.01f;
    p.rate       = apvts.getRawParameterValue ("rate")->load();
    p.hpHz       = apvts.getRawParameterValue ("hipass")->load();
    p.lpHz       = apvts.getRawParameterValue ("lopass")->load();
    p.dryWet     = apvts.getRawParameterValue ("drywet")->load() * 0.01f;
    p.outputGain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load());
    p.analog     = apvts.getRawParameterValue ("analog")->load() * 0.1f;
    p.lofi       = apvts.getRawParameterValue ("lofi")->load() > 0.5f;

    engine.setParams (p);
    engine.process (buffer);
}

//==============================================================================
juce::String VocalDelayProcessor::getDivisionText() const
{
    const int syncMode = (int) apvts.getRawParameterValue ("syncMode")->load();
    if (syncMode == 2)
    {
        const float ms = displayMs.load();
        return juce::String (ms, ms < 100.0f ? 1 : 0) + " ms";
    }

    const int divIdx = juce::jlimit (0, (int) kDivisions.size() - 1,
                                     (int) apvts.getRawParameterValue ("division")->load());
    return kDivisions[(size_t) divIdx].label;
}

//==============================================================================
juce::AudioProcessorEditor* VocalDelayProcessor::createEditor()
{
    return new VocalDelayEditor (*this);
}

//==============================================================================
// Presets.
//==============================================================================
namespace
{
    struct PresetDef
    {
        juce::String name;
        int   division;   // index into kDivisions
        int   syncMode;   // 0 BPM, 1 HOST, 2 MS
        float msTime;
        int   mode;
        float feedback;
        float depth;
        float rate;
        float hipass;
        float lopass;
        bool  filterLink;
        float drywet;
        float output;
        float analog;
        bool  lofi;
    };

    const std::vector<PresetDef>& getPresets()
    {
        // div indices: 0=1/1 1=1/2 2=1/4 3=1/4D 4=1/4T 5=1/8 6=1/8D 7=1/8T 8=1/16 9=1/16D 10=1/16T 11=1/32
        // Anti-tinny: hipass stays low-to-moderate (<=180 Hz) so repeats keep body,
        // feedback stays safe (<=68%), and lopass tames highs without gutting the tone.
        // Tempo presets sync to HOST so they track the DAW; Default keeps BPM to match
        // the plugin's default parameter state, and Slap runs free in MS.
        static const std::vector<PresetDef> presets = {
            //                       div sync  ms   mode                  fb   dpth rate   hp    lp     link   mix  out  ana  lofi
            { "Default",              6,  0,   350, DelayEngine::Dual,    40,  0,   0.20f, 120, 20000, false,  50,  0,  2,   false },
            { "Slap",                 8,  2,   95,  DelayEngine::Dual,    10,  0,   0.20f, 150,  8500, false,  30,  0,  3,   false },
            { "1/4 Throw",            2,  1,   350, DelayEngine::Dual,    34,  0,   0.18f, 120, 14000, false,  35,  0,  2,   false },
            { "Dotted 8th",           6,  1,   350, DelayEngine::Dual,    44,  6,   0.20f, 120, 12000, false,  40,  0,  3,   false },
            { "Ping-Pong Wide",       5,  1,   350, DelayEngine::PingPong,50,  10,  0.25f, 130, 13000, false,  45,  0,  2,   false },
            { "Ambient Wash",         6,  1,   350, DelayEngine::PingPong,66,  32,  0.35f, 160,  7000, true,   34,  0,  5,   false },
            { "Lo-Fi Analog",         5,  1,   350, DelayEngine::Dual,    55,  22,  0.30f, 170,  5500, true,   40,  0,  7,   true  },
            { "Signature",            2,  1,   350, DelayEngine::PingPong,35,  10,  0.30f, 200,  7000, true,   25,  0,  1,   false },
        };
        return presets;
    }
}

int VocalDelayProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String VocalDelayProcessor::getProgramName (int index)
{
    const auto& p = getPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? p[(size_t) index].name : juce::String();
}

void VocalDelayProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    applyProgram (index);
}

void VocalDelayProcessor::applyProgram (int index)
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

    setChoice ("division", p.division);
    setChoice ("syncMode", p.syncMode);
    setFloat  ("msTime",   p.msTime);
    setChoice ("mode",     p.mode);
    setFloat  ("feedback", p.feedback);
    setFloat  ("depth",    p.depth);
    setFloat  ("rate",     p.rate);
    setFloat  ("hipass",   p.hipass);
    setFloat  ("lopass",   p.lopass);
    setBool   ("filterLink", p.filterLink);
    setFloat  ("drywet",   p.drywet);
    setFloat  ("output",   p.output);
    setFloat  ("analog",   p.analog);
    setBool   ("lofi",     p.lofi);
}

//==============================================================================
void VocalDelayProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalDelayProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalDelayProcessor();
}
