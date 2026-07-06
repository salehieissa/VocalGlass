#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
VocalGateProcessor::VocalGateProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    // Load any cached activation and validate online in the background.
    license.loadCachedAndValidate();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
VocalGateProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // Open threshold in dB.
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "threshold", 1 }, "Threshold",
        NormalisableRange<float> (-80.0f, 0.0f, 0.1f), -45.0f));

    // Maximum attenuation when closed (displayed as negative dB).
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "range", 1 }, "Range",
        NormalisableRange<float> (0.0f, 80.0f, 0.1f), 60.0f));

    // Attack (gate opening) in ms — skewed toward the fast end.
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "attack", 1 }, "Attack",
        NormalisableRange<float> (0.05f, 50.0f, 0.01f, 0.35f), 0.5f));

    // Hold time before the release starts.
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "hold", 1 }, "Hold",
        NormalisableRange<float> (0.0f, 500.0f, 1.0f, 0.6f), 40.0f));

    // Release (gate closing) in ms — skewed.
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "release", 1 }, "Release",
        NormalisableRange<float> (5.0f, 2000.0f, 1.0f, 0.4f), 120.0f));

    // Hysteresis: close threshold = open threshold minus this.
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "hysteresis", 1 }, "Hysteresis",
        NormalisableRange<float> (0.0f, 12.0f, 0.1f), 4.0f));

    // Sidechain high-pass frequency (detector only).
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "scHpf", 1 }, "SC HPF",
        NormalisableRange<float> (20.0f, 2000.0f, 1.0f, 0.4f), 80.0f));

    // Monitor the filtered sidechain instead of the gated audio.
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { "scListen", 1 }, "SC Listen", false));

    return layout;
}

//==============================================================================
void VocalGateProcessor::updateSidechainFilter (float freq)
{
    if (std::abs (freq - lastScFreq) < 0.5f)
        return;
    lastScFreq = freq;

    *scFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (
        currentSampleRate, freq, 0.707f);
}

//==============================================================================
void VocalGateProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = 2;

    scFilter.prepare (spec);
    lastScFreq = -1.0f;
    updateSidechainFilter (80.0f);

    scBuffer.setSize (2, samplesPerBlock);

    // Hybrid detector timing: near-instant peak attack, short peak decay, and
    // an ~5 ms RMS window so held vowels register as steadily "loud".
    peakAtkCoeff = std::exp (-1.0f / (0.0001f * (float) sampleRate)); // ~0.1 ms
    peakRelCoeff = std::exp (-1.0f / (0.010f  * (float) sampleRate)); // ~10 ms
    rmsCoeff     = std::exp (-1.0f / (0.005f  * (float) sampleRate)); // ~5 ms

    peakEnv = 0.0f;
    rmsSq   = 0.0f;
    gateState = GateState::Closed;
    holdRemaining = 0;
    gain    = 1.0f;
}

bool VocalGateProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

//==============================================================================
void VocalGateProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh      = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // License gate: until activated, pass audio through clean (no processing).
    if (! license.isActivated())
        return;

    // --- parameters ---
    const float threshold  = apvts.getRawParameterValue ("threshold")->load();
    const float range      = apvts.getRawParameterValue ("range")->load();
    const float attackMs   = apvts.getRawParameterValue ("attack")->load();
    const float holdMs     = apvts.getRawParameterValue ("hold")->load();
    const float releaseMs  = apvts.getRawParameterValue ("release")->load();
    const float hysteresis = apvts.getRawParameterValue ("hysteresis")->load();
    const float scFreq     = apvts.getRawParameterValue ("scHpf")->load();
    const bool  scListen   = apvts.getRawParameterValue ("scListen")->load() > 0.5f;

    updateSidechainFilter (scFreq);

    const float sr = (float) currentSampleRate;
    const float atkCoeff = std::exp (-1.0f / (juce::jmax (0.01f, attackMs)  * 0.001f * sr));
    const float relCoeff = std::exp (-1.0f / (juce::jmax (1.0f,  releaseMs) * 0.001f * sr));
    const int   holdSamples = (int) std::round (holdMs * 0.001f * sr);
    const float floorGain   = juce::Decibels::decibelsToGain (-range);
    const float closeThresh = threshold - hysteresis;

    // --- build the sidechain (filtered copy used only for detection) ---
    scBuffer.makeCopyOf (buffer, true);
    {
        juce::dsp::AudioBlock<float> block (scBuffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        scFilter.process (ctx);
    }

    const float* scL = scBuffer.getReadPointer (0);
    const float* scR = scBuffer.getReadPointer (juce::jmin (1, numCh - 1));

    float inPeak     = 0.0f;
    float blockGrMax = 0.0f;
    float outPeak    = 0.0f;

    // --- main loop ---
    for (int i = 0; i < numSamples; ++i)
    {
        // Detector: mono sum of the filtered sidechain, hybrid peak/RMS.
        const float scMono = 0.5f * (scL[i] + scR[i]);
        const float rect   = std::abs (scMono);

        if (rect > peakEnv) peakEnv = peakAtkCoeff * (peakEnv - rect) + rect;
        else                peakEnv = peakRelCoeff * (peakEnv - rect) + rect;

        rmsSq = rmsCoeff * rmsSq + (1.0f - rmsCoeff) * scMono * scMono;

        const float env     = juce::jmax (peakEnv, std::sqrt (rmsSq)) + 1.0e-9f;
        const float levelDb = juce::Decibels::gainToDecibels (env);

        // Gate state machine: closed -> open above threshold; open -> hold once
        // the level drops below threshold - hysteresis; hold -> closed when the
        // timer runs out. Any re-trigger above threshold reopens instantly.
        switch (gateState)
        {
            case GateState::Closed:
                if (levelDb > threshold)
                    gateState = GateState::Open;
                break;

            case GateState::Open:
                if (levelDb < closeThresh)
                {
                    gateState = GateState::Hold;
                    holdRemaining = holdSamples;
                }
                break;

            case GateState::Hold:
                if (levelDb > threshold)
                    gateState = GateState::Open;
                else if (--holdRemaining <= 0)
                    gateState = GateState::Closed;
                break;
        }

        // Smooth the gain toward the target with attack/release one-poles.
        const float targetGain = (gateState == GateState::Closed) ? floorGain : 1.0f;
        if (targetGain > gain) gain = atkCoeff * (gain - targetGain) + targetGain;
        else                   gain = relCoeff * (gain - targetGain) + targetGain;
        gain += 1.0e-12f; gain -= 1.0e-12f;   // flush denormals

        const float grNow = -juce::Decibels::gainToDecibels (gain + 1.0e-9f);
        blockGrMax = juce::jmax (blockGrMax, grNow);

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            const float in = data[i];
            inPeak = juce::jmax (inPeak, std::abs (in));

            const float out = in * gain;
            data[i] = out;
            outPeak = juce::jmax (outPeak, std::abs (out));
        }
    }

    // --- monitor the filtered sidechain instead of the gated audio ---
    if (scListen)
    {
        for (int ch = 0; ch < numCh; ++ch)
            buffer.copyFrom (ch, 0, scBuffer, juce::jmin (ch, scBuffer.getNumChannels() - 1),
                             0, numSamples);
        outPeak = buffer.getMagnitude (0, numSamples);
    }

    // --- meters ---
    inDb.store  (juce::Decibels::gainToDecibels (inPeak  + 1.0e-9f));
    grDb.store  (blockGrMax);
    outDb.store (juce::Decibels::gainToDecibels (outPeak + 1.0e-9f));
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
            { "Default",       { {"threshold",-45}, {"range",60}, {"attack",0.5f}, {"hold",40},
                                 {"release",120}, {"hysteresis",4}, {"scHpf",80},  {"scListen",0} } }, // matches param defaults
            { "Vocal Cleanup", { {"threshold",-50}, {"range",40}, {"attack",0.3f}, {"hold",60},
                                 {"release",180}, {"hysteresis",4}, {"scHpf",100}, {"scListen",0} } },
            { "Tight Rap",     { {"threshold",-38}, {"range",70}, {"attack",0.1f}, {"hold",25},
                                 {"release",60},  {"hysteresis",6}, {"scHpf",120}, {"scListen",0} } },
            { "Gentle Gate",   { {"threshold",-55}, {"range",25}, {"attack",1.0f}, {"hold",80},
                                 {"release",300}, {"hysteresis",3}, {"scHpf",80},  {"scListen",0} } },
            // Tuned to kill a rap-vocal-chain noise floor between phrases
            // without clipping word tails.
            { "Signature",     { {"threshold",-48}, {"range",55}, {"attack",0.2f}, {"hold",50},
                                 {"release",150}, {"hysteresis",5}, {"scHpf",110}, {"scListen",0} } },
        };
        return presets;
    }
}

int VocalGateProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String VocalGateProcessor::getProgramName (int index)
{
    const auto& presets = getPresets();
    if (juce::isPositiveAndBelow (index, (int) presets.size()))
        return presets[(size_t) index].name;
    return {};
}

void VocalGateProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;
    currentProgram = index;
    applyProgram (index);
}

void VocalGateProcessor::applyProgram (int index)
{
    const auto& presets = getPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    for (auto* ap : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& [id, value] : presets[(size_t) index].values)
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
}

//==============================================================================
juce::AudioProcessorEditor* VocalGateProcessor::createEditor()
{
    return new VocalGateEditor (*this);
}

void VocalGateProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void VocalGateProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalGateProcessor();
}
