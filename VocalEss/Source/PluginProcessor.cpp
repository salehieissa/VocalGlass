#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
VocalEssProcessor::VocalEssProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
VocalEssProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // Split (true) processes only the high band; wideband ducks everything.
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { "split", 1 }, "Split", true));

    // Sidechain / crossover frequency.
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "freq", 1 }, "Frequency",
        NormalisableRange<float> (1000.0f, 16000.0f, 1.0f, 0.5f), 5506.0f));

    // Threshold in dB.
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "threshold", 1 }, "Threshold",
        NormalisableRange<float> (-80.0f, 0.0f, 0.1f), -36.0f));

    // Sidechain filter shape used by the detector.
    layout.add (std::make_unique<AudioParameterChoice>(
        ParameterID { "scType", 1 }, "SC Filter",
        StringArray { "High Pass", "Bell", "High Shelf" }, 0));

    // What the output bus carries: the processed audio or the raw sidechain.
    layout.add (std::make_unique<AudioParameterChoice>(
        ParameterID { "monitor", 1 }, "Monitor",
        StringArray { "Audio", "S Chain" }, 0));

    return layout;
}

//==============================================================================
void VocalEssProcessor::updateSidechainFilter (int type, float freq)
{
    if (type == lastScType && std::abs (freq - lastScFreq) < 0.5f)
        return;
    lastScType = type;
    lastScFreq = freq;

    const auto sr = currentSampleRate;
    switch (type)
    {
        case 1: // Bell — focus a band around the frequency
            *scFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                sr, freq, 1.2f, juce::Decibels::decibelsToGain (6.0f));
            break;
        case 2: // High shelf — emphasise everything above the frequency
            *scFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                sr, freq, 0.7f, juce::Decibels::decibelsToGain (9.0f));
            break;
        default: // High pass — classic de-ess detection
            *scFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (
                sr, freq, 0.707f);
            break;
    }
}

//==============================================================================
void VocalEssProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = 2;

    crossover.prepare (spec);
    crossover.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    crossover.setCutoffFrequency (5506.0f);
    lastCrossoverFreq = 5506.0f;

    scFilter.prepare (spec);
    lastScType = -1;
    lastScFreq = -1.0f;
    updateSidechainFilter (0, 5506.0f);

    scBuffer.setSize (2, samplesPerBlock);

    // Fast attack, musical release — typical de-esser timing.
    atkCoeff = std::exp (-1.0f / (0.0004f * (float) sampleRate)); // ~0.4 ms
    relCoeff = std::exp (-1.0f / (0.080f  * (float) sampleRate)); // ~80 ms
    envelope = 0.0f;
}

bool VocalEssProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

//==============================================================================
void VocalEssProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh      = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // --- parameters ---
    const bool  split     = apvts.getRawParameterValue ("split")->load() > 0.5f;
    const float freq      = apvts.getRawParameterValue ("freq")->load();
    const float threshold = apvts.getRawParameterValue ("threshold")->load();
    const int   scType    = (int) apvts.getRawParameterValue ("scType")->load();
    const int   monitor   = (int) apvts.getRawParameterValue ("monitor")->load();

    constexpr float ratio = 6.0f;
    const float slope = 1.0f - 1.0f / ratio;

    if (freq != lastCrossoverFreq)
    {
        crossover.setCutoffFrequency (freq);
        lastCrossoverFreq = freq;
    }
    updateSidechainFilter (scType, freq);

    // --- build the sidechain (filtered copy used only for detection) ---
    scBuffer.makeCopyOf (buffer, true);
    {
        juce::dsp::AudioBlock<float> block (scBuffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        scFilter.process (ctx);
    }

    const float* scL = scBuffer.getReadPointer (0);
    const float* scR = scBuffer.getReadPointer (juce::jmin (1, numCh - 1));

    float blockScPeak = -100.0f;
    float blockGrMax  = 0.0f;
    float outPeak[2]  = { 0.0f, 0.0f };

    // --- main loop ---
    for (int i = 0; i < numSamples; ++i)
    {
        // Detector: mono sum of the filtered sidechain.
        const float scMono = 0.5f * (scL[i] + scR[i]);
        const float rect    = std::abs (scMono);

        if (rect > envelope) envelope = atkCoeff * (envelope - rect) + rect;
        else                 envelope = relCoeff * (envelope - rect) + rect;

        const float levelDb = juce::Decibels::gainToDecibels (envelope + 1.0e-9f);
        blockScPeak = juce::jmax (blockScPeak, levelDb);

        float grDb = 0.0f;
        if (levelDb > threshold)
            grDb = (levelDb - threshold) * slope;
        blockGrMax = juce::jmax (blockGrMax, grDb);

        const float gain = juce::Decibels::decibelsToGain (-grDb);

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            const float in = data[i];

            float low = 0.0f, high = 0.0f;
            crossover.processSample (ch, in, low, high);

            const float out = split ? (low + high * gain)  // duck only the sibilant band
                                    : (in * gain);          // wideband duck
            data[i] = out;
            outPeak[ch] = juce::jmax (outPeak[ch], std::abs (out));
        }
    }

    // --- monitor the sidechain instead of the audio ---
    if (monitor == 1)
        for (int ch = 0; ch < numCh; ++ch)
            buffer.copyFrom (ch, 0, scBuffer, juce::jmin (ch, scBuffer.getNumChannels() - 1), 0, numSamples);

    // --- meters ---
    scLevelDb.store (blockScPeak);
    attenDb.store (blockGrMax);

    // When monitoring the sidechain the buffer was just overwritten, so measure
    // it directly; otherwise reuse the running peaks gathered in the main loop
    // (identical to a full getMagnitude scan, but without the extra passes).
    float magL, magR;
    if (monitor == 1)
    {
        magL = buffer.getMagnitude (0, 0, numSamples);
        magR = buffer.getMagnitude (juce::jmin (1, numCh - 1), 0, numSamples);
    }
    else
    {
        magL = outPeak[0];
        magR = outPeak[juce::jmin (1, numCh - 1)];
    }
    outLDb.store (juce::Decibels::gainToDecibels (magL + 1.0e-9f));
    outRDb.store (juce::Decibels::gainToDecibels (magR + 1.0e-9f));
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
        // scType: 0=High Pass, 1=Bell, 2=High Shelf.  split: 1=split, 0=wideband.
        // Split mode is kept on so only the sibilant band is ducked — the body of
        // the vocal is untouched, so de-essing never dulls or thins the voice.
        static const std::vector<PresetDef> presets =
        {
            { "Default De-Ess",   { {"split",1}, {"freq",5506}, {"threshold",-36}, {"scType",0}, {"monitor",0} } }, // matches param defaults
            { "Gentle De-Ess",    { {"split",1}, {"freq",6500}, {"threshold",-28}, {"scType",0}, {"monitor",0} } },
            { "Female Vocal",     { {"split",1}, {"freq",7200}, {"threshold",-32}, {"scType",0}, {"monitor",0} } },
            { "Male Vocal",       { {"split",1}, {"freq",5200}, {"threshold",-32}, {"scType",0}, {"monitor",0} } },
            { "Aggressive",       { {"split",1}, {"freq",6500}, {"threshold",-44}, {"scType",0}, {"monitor",0} } },
            { "Sibilance Tamer",  { {"split",1}, {"freq",6800}, {"threshold",-34}, {"scType",1}, {"monitor",0} } },
            { "Bright Vocal Fix", { {"split",1}, {"freq",8000}, {"threshold",-30}, {"scType",2}, {"monitor",0} } },
        };
        return presets;
    }
}

int VocalEssProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String VocalEssProcessor::getProgramName (int index)
{
    const auto& presets = getPresets();
    if (juce::isPositiveAndBelow (index, (int) presets.size()))
        return presets[(size_t) index].name;
    return {};
}

void VocalEssProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;
    currentProgram = index;
    applyProgram (index);
}

void VocalEssProcessor::applyProgram (int index)
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
juce::AudioProcessorEditor* VocalEssProcessor::createEditor()
{
    return new VocalEssEditor (*this);
}

void VocalEssProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void VocalEssProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
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
    return new VocalEssProcessor();
}
