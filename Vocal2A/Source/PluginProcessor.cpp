#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Vocal2AProcessor::Vocal2AProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    gainPtr   = apvts.getRawParameterValue ("gain");
    peakPtr   = apvts.getRawParameterValue ("peakReduction");
    modePtr   = apvts.getRawParameterValue ("mode");
    autoPtr   = apvts.getRawParameterValue ("autoMakeup");
    analogPtr = apvts.getRawParameterValue ("analog");
    hiFreqPtr = apvts.getRawParameterValue ("hiFreq");
    mixPtr    = apvts.getRawParameterValue ("mix");
    trimPtr   = apvts.getRawParameterValue ("trim");
    attackPtr  = apvts.getRawParameterValue ("attack");
    releasePtr = apvts.getRawParameterValue ("release");

    // Load any cached activation and validate online in the background.
    license.loadCachedAndValidate();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout Vocal2AProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "gain", 1 }, "Gain",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "peakReduction", 1 }, "Peak Reduction",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 80.0f));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "mode", 1 }, "Mode",
        StringArray { "Compress", "Limit" }, OptoLeveler::Compress));

    // Opto attack / release. The cell stays program-dependent; these set the
    // overall speed of the optical attack and the long release tail.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "attack", 1 }, "Attack",
        NormalisableRange<float> (0.1f, 120.0f, 0.1f, 0.4f), 10.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "release", 1 }, "Release",
        NormalisableRange<float> (80.0f, 3000.0f, 1.0f, 0.4f), 1200.0f));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "autoMakeup", 1 }, "Auto Makeup", true));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "analog", 1 }, "Analog",
        StringArray { "50Hz", "60Hz", "Off" }, OptoLeveler::AnalogOff));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "hiFreq", 1 }, "Hi Freq",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "trim", 1 }, "Trim",
        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "vuSource", 1 }, "VU Source",
        StringArray { "Input", "GR", "Output" }, 1));

    return layout;
}

//==============================================================================
void Vocal2AProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    engine.prepare (spec);
}

bool Vocal2AProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void Vocal2AProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    // License gate: until activated, pass audio through clean (no processing).
    if (! license.isActivated())
        return;

    engine.setParams (gainPtr->load(), peakPtr->load(), (int) modePtr->load(),
                      autoPtr->load() > 0.5f, (int) analogPtr->load(),
                      hiFreqPtr->load(), mixPtr->load(), trimPtr->load(),
                      attackPtr->load(), releasePtr->load());
    engine.process (buffer);
}

//==============================================================================
juce::AudioProcessorEditor* Vocal2AProcessor::createEditor()
{
    return new Vocal2AEditor (*this);
}

//==============================================================================
// Presets — gain / peakReduction / mode / autoMakeup / mix combinations.
//==============================================================================
namespace
{
    struct PresetDef
    {
        juce::String name;
        float gain, peak;
        int   mode;
        bool  autoMakeup;
        int   analog;     // 0=50Hz, 1=60Hz, 2=Off
        float hiFreq;     // detector HF emphasis (0 = no scoop, keeps body)
        float mix;
        float trim;       // output trim for loudness matching (dB)
    };

    const std::vector<PresetDef>& getPresets()
    {
        // Default matches the plugin's default parameter state exactly.
        // autoMakeup stays on everywhere so programs are roughly equal loudness;
        // small trim nudges keep heavier-reduction presets full, not thin.
        static const std::vector<PresetDef> presets = {
            //  name             gain  peak  mode                  auto  analog                hiFreq  mix    trim
            { "Default",         50.0f, 80.0f, OptoLeveler::Compress, true,  OptoLeveler::AnalogOff,  0.0f, 100.0f,  0.0f },
            { "Silky Leveler",   50.0f, 38.0f, OptoLeveler::Compress, true,  OptoLeveler::AnalogOff,  8.0f, 100.0f,  0.0f },
            { "Lead Vocal",      55.0f, 60.0f, OptoLeveler::Compress, true,  OptoLeveler::AnalogOff, 14.0f, 100.0f,  0.0f },
            { "Smooth Ride",     52.0f, 72.0f, OptoLeveler::Compress, true,  OptoLeveler::AnalogOff,  8.0f, 100.0f,  0.5f },
            { "Vintage Glue",    55.0f, 64.0f, OptoLeveler::Compress, true,  OptoLeveler::Hum60,     20.0f, 100.0f,  1.0f },
            { "Heavy Level",     58.0f, 90.0f, OptoLeveler::Compress, true,  OptoLeveler::AnalogOff, 12.0f, 100.0f,  1.5f },
            { "Parallel",        56.0f, 88.0f, OptoLeveler::Compress, true,  OptoLeveler::AnalogOff,  6.0f,  55.0f,  1.0f },
            { "Signature",       50.0f, 60.0f, OptoLeveler::Compress, false, OptoLeveler::AnalogOff,  0.0f, 100.0f,  0.0f },
        };
        return presets;
    }
}

int Vocal2AProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String Vocal2AProcessor::getProgramName (int index)
{
    const auto& p = getPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? p[(size_t) index].name : juce::String();
}

void Vocal2AProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    applyProgram (index);
}

void Vocal2AProcessor::applyProgram (int index)
{
    const auto& presets = getPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    const auto& p = presets[(size_t) index];

    auto setParam = [this] (const juce::String& id, float value)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (value));
    };

    setParam ("gain", p.gain);
    setParam ("peakReduction", p.peak);
    setParam ("mode", (float) p.mode);
    setParam ("autoMakeup", p.autoMakeup ? 1.0f : 0.0f);
    setParam ("analog", (float) p.analog);
    setParam ("hiFreq", p.hiFreq);
    setParam ("mix", p.mix);
    setParam ("trim", p.trim);
}

//==============================================================================
void Vocal2AProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void Vocal2AProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Vocal2AProcessor();
}
