#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Length of a tempo division expressed in quarter-note beats.
    // beats = 4 * divisionFraction, with dotted x1.5 and triplet x2/3.
    // Mod divisions:  "1/1","1/2","1/4","1/4.","1/4T","1/8","1/8.","1/8T","1/16","1/16T"
    constexpr double kModDivBeats[] = {
        4.0,            // 1/1
        2.0,            // 1/2
        1.0,            // 1/4
        1.5,            // 1/4.  (dotted)
        2.0 / 3.0,      // 1/4T  (triplet)
        0.5,            // 1/8
        0.75,           // 1/8.  (dotted)
        0.5 * 2.0 / 3.0,// 1/8T  (triplet)
        0.25,           // 1/16
        0.25 * 2.0 / 3.0// 1/16T (triplet)
    };

    // Predelay divisions: "1/64","1/32","1/16","1/16.","1/8T","1/8","1/4"
    constexpr double kPreDivBeats[] = {
        4.0 / 64.0,     // 1/64
        4.0 / 32.0,     // 1/32
        0.25,           // 1/16
        0.375,          // 1/16. (dotted)
        0.5 * 2.0 / 3.0,// 1/8T  (triplet)
        0.5,            // 1/8
        1.0             // 1/4
    };
}

//==============================================================================
VocalVerbProcessor::VocalVerbProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    // Load any cached activation and validate online in the background.
    license.loadCachedAndValidate();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout VocalVerbProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 18.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "predelay", 1 }, "Predelay",
        NormalisableRange<float> (0.0f, 250.0f, 0.1f, 0.5f), 20.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "decay", 1 }, "Decay",
        NormalisableRange<float> (0.1f, 15.0f, 0.01f, 0.5f), 4.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "dampHighFreq", 1 }, "Damp High Freq",
        NormalisableRange<float> (500.0f, 18000.0f, 1.0f, 0.35f), 6000.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "dampHighShelf", 1 }, "Damp High Shelf",
        NormalisableRange<float> (-48.0f, 0.0f, 0.1f), -24.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "bassFreq", 1 }, "Bass Freq",
        NormalisableRange<float> (40.0f, 1000.0f, 1.0f, 0.5f), 700.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "bassMult", 1 }, "Bass Mult",
        NormalisableRange<float> (0.2f, 4.0f, 0.01f), 1.5f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "size", 1 }, "Size",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "attack", 1 }, "Attack",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "diffEarly", 1 }, "Diff Early",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "diffLate", 1 }, "Diff Late",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "modRate", 1 }, "Mod Rate",
        NormalisableRange<float> (0.05f, 12.0f, 0.01f, 0.5f), 2.53f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "modDepth", 1 }, "Mod Depth",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 38.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "highCut", 1 }, "High Cut",
        NormalisableRange<float> (500.0f, 20000.0f, 1.0f, 0.35f), 8000.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "lowCut", 1 }, "Low Cut",
        NormalisableRange<float> (10.0f, 1000.0f, 1.0f, 0.5f), 10.0f));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "mode", 1 }, "Mode",
        StringArray { "concert hall", "plate", "room", "chamber", "ambience" }, 0));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "color", 1 }, "Color",
        StringArray { "1970s", "modern", "vintage", "dark", "bright" }, 0));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "bypass", 1 }, "Bypass", false));

    // ---- host-tempo sync (off by default to preserve existing behaviour) ----
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "modSync", 1 }, "Mod Sync", false));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "modDiv", 1 }, "Mod Division",
        StringArray { "1/1", "1/2", "1/4", "1/4.", "1/4T",
                      "1/8", "1/8.", "1/8T", "1/16", "1/16T" }, 2)); // default 1/4

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "preSync", 1 }, "Predelay Sync", false));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "preDiv", 1 }, "Predelay Division",
        StringArray { "1/64", "1/32", "1/16", "1/16.", "1/8T", "1/8", "1/4" }, 2)); // default 1/16

    return layout;
}

//==============================================================================
void VocalVerbProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    engine.prepare (spec);
}

bool VocalVerbProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void VocalVerbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    // License gate: until activated, pass audio through clean (no processing).
    if (! license.isActivated())
        return;

    if (raw ("bypass") > 0.5f)
        return;

    // ---- host tempo (JUCE 8 AudioPlayHead API), fallback 120 BPM ----
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto hostBpm = pos->getBpm())
                bpm = *hostBpm;
    bpm = juce::jlimit (20.0, 999.0, bpm);
    const double quarterSec = 60.0 / bpm;

    Reverb::Params p;
    p.mix         = raw ("mix") * 0.01f;
    p.predelayMs  = raw ("predelay");
    p.decaySec    = raw ("decay");
    p.dampHz      = raw ("dampHighFreq");
    p.dampShelfDb = raw ("dampHighShelf");
    p.bassHz      = raw ("bassFreq");
    p.bassMult    = raw ("bassMult");
    p.size        = raw ("size") * 0.01f;
    p.attack      = raw ("attack") * 0.01f;
    p.diffEarly   = raw ("diffEarly") * 0.01f;
    p.diffLate    = raw ("diffLate") * 0.01f;
    p.modRate     = raw ("modRate");
    p.modDepth    = raw ("modDepth") * 0.01f;
    p.highCut     = raw ("highCut");
    p.lowCut      = raw ("lowCut");
    p.mode        = (int) raw ("mode");
    p.color       = (int) raw ("color");

    // ---- host-tempo sync overrides ----
    if (raw ("modSync") > 0.5f)
    {
        const int idx = juce::jlimit (0, (int) std::size (kModDivBeats) - 1, (int) raw ("modDiv"));
        const double beats = kModDivBeats[idx];           // division length in beats
        const double periodSec = quarterSec * beats;      // LFO period
        const double hz = periodSec > 0.0 ? 1.0 / periodSec : p.modRate;
        p.modRate = (float) juce::jlimit (0.05, 12.0, hz); // clamp to LFO knob range
    }

    if (raw ("preSync") > 0.5f)
    {
        const int idx = juce::jlimit (0, (int) std::size (kPreDivBeats) - 1, (int) raw ("preDiv"));
        const double beats = kPreDivBeats[idx];
        const double ms = quarterSec * beats * 1000.0;
        p.predelayMs = (float) juce::jlimit (0.0, 250.0, ms); // clamp to predelay max
    }

    engine.setParams (p);
    engine.process (buffer);
}

//==============================================================================
juce::AudioProcessorEditor* VocalVerbProcessor::createEditor()
{
    return new VocalVerbEditor (*this);
}

//==============================================================================
// Presets — full parameter snapshots with vocal-y reverb names.
//==============================================================================
namespace
{
    struct PresetDef
    {
        juce::String name;
        float mix, predelay, decay, dampHF, dampHS, bassF, bassM, size, attack;
        float diffE, diffL, modR, modD, hiCut, loCut;
        int mode, color;
    };

    const std::vector<PresetDef>& getPresets()
    {
        // Anti-tinny rules baked in: mix stays vocal-friendly (14-26%), lowCut stays
        // low (<=60 Hz) so the tail keeps body, highs are tamed via dampHighFreq/highCut
        // rather than scooping lows, and bassMult >=1.3 keeps the tail full.
        static const std::vector<PresetDef> presets = {
            // name             mix  pre  dec   dHF   dHS  bF   bM   sz   atk  dE   dL   mR    mD   hiC    loC  mode col
            { "default",        18, 20,  4.00f, 6000, -24, 700, 1.50f, 100,  50, 100, 100, 2.53f, 38,  8000,  10,  0,  0 },
            { "Vocal Plate",    22, 12,  2.30f, 8500, -20, 520, 1.35f,  72,  32, 100, 100, 2.80f, 30, 11000,  60,  1,  1 },
            { "Lush Hall",      26, 30,  5.50f, 6000, -22, 620, 1.60f, 100,  55, 100, 100, 1.60f, 36,  8500,  50,  0,  2 },
            { "Short Ambience", 14,  5,  0.95f, 7000, -20, 600, 1.30f,  42,  25, 100,  96, 2.00f, 22,  9000,  45,  4,  1 },
            { "Long Tail",      24, 40,  8.00f, 5500, -26, 700, 1.70f, 100,  65,  95, 100, 1.20f, 40,  7500,  45,  0,  2 },
            { "Warm Chamber",   24, 22,  3.20f, 4200, -30, 750, 1.70f,  80,  45, 100, 100, 2.00f, 38,  6000,  35,  3,  3 },
            { "Wide Air",       20, 18,  4.50f, 7500, -20, 540, 1.45f,  95,  55, 100, 100, 1.80f, 46, 12000,  50,  0,  4 },
            { "Signature",      20, 15,  0.93f, 6000, -24, 300, 1.50f, 100,  50, 100, 100, 2.50f, 38,  8000,  10,  0,  3 },
        };
        return presets;
    }
}

int VocalVerbProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String VocalVerbProcessor::getProgramName (int index)
{
    const auto& p = getPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? p[(size_t) index].name : juce::String();
}

void VocalVerbProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    applyProgram (index);
}

void VocalVerbProcessor::applyProgram (int index)
{
    const auto& presets = getPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    const auto& d = presets[(size_t) index];

    auto setF = [this] (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (v));
    };
    auto setC = [this] (const char* id, int idx)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) idx));
    };

    setF ("mix", d.mix);            setF ("predelay", d.predelay);
    setF ("decay", d.decay);        setF ("dampHighFreq", d.dampHF);
    setF ("dampHighShelf", d.dampHS); setF ("bassFreq", d.bassF);
    setF ("bassMult", d.bassM);     setF ("size", d.size);
    setF ("attack", d.attack);      setF ("diffEarly", d.diffE);
    setF ("diffLate", d.diffL);     setF ("modRate", d.modR);
    setF ("modDepth", d.modD);      setF ("highCut", d.hiCut);
    setF ("lowCut", d.loCut);
    setC ("mode", d.mode);          setC ("color", d.color);
}

//==============================================================================
void VocalVerbProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalVerbProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalVerbProcessor();
}
