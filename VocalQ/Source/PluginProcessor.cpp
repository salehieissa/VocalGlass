#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
namespace
{
    struct BandDefault { BandType type; float freq; };

    const std::array<BandDefault, VocalQProcessor::kNumBands> kDefaults = { {
        { BandType::LowCut,  30.0f   },
        { BandType::Bell,    80.0f   },
        { BandType::Bell,    200.0f  },
        { BandType::Bell,    600.0f  },
        { BandType::Bell,    1500.0f },
        { BandType::Bell,    4000.0f },
        { BandType::Bell,    8000.0f },
        { BandType::HighCut, 18000.0f },
    } };
}

juce::String VocalQProcessor::bandParamId (int band, const char* suffix)
{
    return "b" + juce::String (band) + "_" + suffix;
}

//==============================================================================
VocalQProcessor::VocalQProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    cacheParamPointers();
}

void VocalQProcessor::cacheParamPointers()
{
    for (int b = 0; b < kNumBands; ++b)
    {
        bp[(size_t) b].on     = apvts.getRawParameterValue (bandParamId (b, "on"));
        bp[(size_t) b].type   = apvts.getRawParameterValue (bandParamId (b, "type"));
        bp[(size_t) b].freq   = apvts.getRawParameterValue (bandParamId (b, "freq"));
        bp[(size_t) b].q      = apvts.getRawParameterValue (bandParamId (b, "q"));
        bp[(size_t) b].gain   = apvts.getRawParameterValue (bandParamId (b, "gain"));
        bp[(size_t) b].range  = apvts.getRawParameterValue (bandParamId (b, "range"));
        bp[(size_t) b].thresh = apvts.getRawParameterValue (bandParamId (b, "thresh"));
        bp[(size_t) b].atk    = apvts.getRawParameterValue (bandParamId (b, "atk"));
        bp[(size_t) b].rel    = apvts.getRawParameterValue (bandParamId (b, "rel"));
        bp[(size_t) b].chan   = apvts.getRawParameterValue (bandParamId (b, "chan"));
    }
    outPtr = apvts.getRawParameterValue ("out");
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout VocalQProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto freqRange = NormalisableRange<float> (16.0f, 21000.0f);
    freqRange.setSkewForCentre (700.0f);
    auto qRange = NormalisableRange<float> (0.1f, 60.0f);
    qRange.setSkewForCentre (1.2f);
    auto atkRange = NormalisableRange<float> (0.5f, 500.0f);
    atkRange.setSkewForCentre (30.0f);
    auto relRange = NormalisableRange<float> (5.0f, 5000.0f);
    relRange.setSkewForCentre (200.0f);

    const StringArray typeChoices  { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch" };
    const StringArray chanChoices  { "Stereo", "Mid", "Side" };

    for (int b = 0; b < kNumBands; ++b)
    {
        const auto& d = kDefaults[(size_t) b];
        auto pid = [b] (const char* s) { return ParameterID { bandParamId (b, s), 1 }; };
        const auto bn = juce::String (b + 1);

        layout.add (std::make_unique<AudioParameterBool>  (pid ("on"), "B" + bn + " On", true));
        layout.add (std::make_unique<AudioParameterChoice> (pid ("type"), "B" + bn + " Type", typeChoices, (int) d.type));
        layout.add (std::make_unique<AudioParameterFloat> (pid ("freq"), "B" + bn + " Freq", freqRange, d.freq));
        layout.add (std::make_unique<AudioParameterFloat> (pid ("q"), "B" + bn + " Q", qRange, 1.0f));
        layout.add (std::make_unique<AudioParameterFloat> (pid ("gain"), "B" + bn + " Gain",
                        NormalisableRange<float> (-30.0f, 30.0f, 0.1f), 0.0f));
        layout.add (std::make_unique<AudioParameterFloat> (pid ("range"), "B" + bn + " Range",
                        NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
        layout.add (std::make_unique<AudioParameterFloat> (pid ("thresh"), "B" + bn + " Threshold",
                        NormalisableRange<float> (-60.0f, 0.0f, 0.1f), 0.0f));
        layout.add (std::make_unique<AudioParameterFloat> (pid ("atk"), "B" + bn + " Attack", atkRange, 16.0f));
        layout.add (std::make_unique<AudioParameterFloat> (pid ("rel"), "B" + bn + " Release", relRange, 160.0f));
        layout.add (std::make_unique<AudioParameterChoice> (pid ("chan"), "B" + bn + " Channel", chanChoices, 0));
    }

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "out", 1 }, "Output",
                    NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    return layout;
}

//==============================================================================
void VocalQProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // Oversampling for the band processing (2x via low-latency IIR halfband).
    const auto chans = (size_t) juce::jmax (1, getTotalNumOutputChannels());
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        chans, (size_t) kOsLog2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampling->initProcessing ((size_t) samplesPerBlock);

    const int    osFactor = 1 << kOsLog2;
    eqDesignRate = sampleRate * (double) osFactor;

    // Bands run at the oversampled rate; solo audition runs at the host rate.
    juce::dsp::ProcessSpec osSpec  { eqDesignRate,    (juce::uint32) (samplesPerBlock * osFactor), 2 };
    juce::dsp::ProcessSpec baseSpec { sampleRate,     (juce::uint32) samplesPerBlock,              2 };

    for (auto& band : bands) band.prepare (osSpec);
    for (auto& f : soloFilt)  { f.prepare (baseSpec); f.reset(); }
    for (auto& f : soloFiltB) { f.prepare (baseSpec); f.reset(); }
    soloCoeffBand = -1;   // force a solo-coeff rebuild after (re)prepare

    analyzer.prepare (sampleRate);
    dryBuffer.setSize (2, samplesPerBlock);

    setLatencySamples ((int) oversampling->getLatencyInSamples());
}

bool VocalQProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void VocalQProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    // Feed the live analyzer with the unprocessed input.
    analyzer.pushBlock (buffer);

    const float outGain = juce::Decibels::decibelsToGain (outPtr->load());
    const int   solo    = soloBand.load();

    if (solo >= 0 && solo < kNumBands)
    {
        // Band audition (like Pro-Q's solo): play exactly the region the band acts
        // on, shaped by the band's TYPE and Q, through two cascaded biquads so the
        // listening window is tight and well-defined.
        //   * Bell / Notch  -> band-pass centred on the band, width from its Q.
        //   * Low Shelf/Cut  -> low-pass at the corner  (hear the low region).
        //   * High Shelf/Cut -> high-pass at the corner (hear the high region).
        const auto  stype = (BandType) (int) bp[(size_t) solo].type->load();
        const double freq = juce::jlimit (20.0, currentSampleRate * 0.49,
                                          (double) bp[(size_t) solo].freq->load());
        const double bandQ = juce::jlimit (0.7, 12.0, (double) bp[(size_t) solo].q->load());

        // Rebuild only when the soloed band / type / freq / q actually changes.
        if (solo != soloCoeffBand || stype != (BandType) soloCoeffType
            || ! juce::exactlyEqual (freq, soloCoeffFreq) || ! juce::exactlyEqual (bandQ, soloCoeffQ))
        {
            using C = juce::dsp::IIR::Coefficients<float>;
            C::Ptr c;
            switch (stype)
            {
                case BandType::LowShelf:
                case BandType::LowCut:   c = C::makeLowPass  (currentSampleRate, freq, 0.707f); break;
                case BandType::HighShelf:
                case BandType::HighCut:  c = C::makeHighPass (currentSampleRate, freq, 0.707f); break;
                case BandType::Bell:
                case BandType::Notch:
                default:                 c = C::makeBandPass (currentSampleRate, freq, (float) bandQ); break;
            }
            for (auto& f : soloFilt)  *f.coefficients = *c;
            for (auto& f : soloFiltB) *f.coefficients = *c;
            soloCoeffBand = solo; soloCoeffType = (int) stype;
            soloCoeffFreq = freq; soloCoeffQ = bandQ;
        }

        // Band-pass auditions narrow the energy, so give them gentle makeup; the
        // pass-band (shelf/cut) auditions already sit near unity, so leave them be.
        const bool isBandPass = (stype == BandType::Bell || stype == BandType::Notch);
        const float soloGain  = outGain * juce::Decibels::decibelsToGain (isBandPass ? 6.0f : 0.0f);
        for (int ch = 0; ch < juce::jmin (numCh, 2); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
            {
                float x = soloFilt[(size_t) ch].processSample (d[i]);
                x = soloFiltB[(size_t) ch].processSample (x);
                d[i] = x * soloGain;
            }
        }
    }
    else
    {
        for (int b = 0; b < kNumBands; ++b)
        {
            EQBand::Params p;
            p.on     = bp[(size_t) b].on->load()   > 0.5f;
            p.type   = (int) bp[(size_t) b].type->load();
            p.freq   = bp[(size_t) b].freq->load();
            p.q      = bp[(size_t) b].q->load();
            p.gain   = bp[(size_t) b].gain->load();
            p.range  = bp[(size_t) b].range->load();
            p.thresh = bp[(size_t) b].thresh->load();
            p.atkMs  = bp[(size_t) b].atk->load();
            p.relMs  = bp[(size_t) b].rel->load();
            p.chan   = (int) bp[(size_t) b].chan->load();
            p.scMode = 0; // band-focused detection
            bands[(size_t) b].setParams (p);
        }

        // Oversample -> run the 8 bands at 2x -> downsample. Designing and running
        // the biquads above the audio band keeps bell/shelf curves accurate right
        // up to 20 kHz instead of cramping near the host Nyquist.
        juce::dsp::AudioBlock<float> baseBlock (buffer);
        auto os = oversampling->processSamplesUp (baseBlock);

        const int osCh = (int) os.getNumChannels();
        const int osN  = (int) os.getNumSamples();
        float* ptrs[2] = { os.getChannelPointer (0),
                           osCh > 1 ? os.getChannelPointer (1) : os.getChannelPointer (0) };
        juce::AudioBuffer<float> osBuf (ptrs, juce::jmin (osCh, 2), osN);

        for (auto& band : bands) band.process (osBuf);

        oversampling->processSamplesDown (baseBlock);

        if (std::abs (outGain - 1.0f) > 1.0e-4f)
            buffer.applyGain (outGain);
    }

    // ---- output metering ----
    const float l = buffer.getMagnitude (0, 0, n);
    const float r = numCh > 1 ? buffer.getMagnitude (1, 0, n) : l;
    outLDb.store (juce::Decibels::gainToDecibels (l + 1.0e-6f));
    outRDb.store (juce::Decibels::gainToDecibels (r + 1.0e-6f));
}

//==============================================================================
juce::AudioProcessorEditor* VocalQProcessor::createEditor()
{
    return new VocalQEditor (*this);
}

//==============================================================================
// Presets
//==============================================================================
namespace
{
    struct PresetDef
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values;
    };

    const std::vector<PresetDef>& getPresets()
    {
        // Band index map (param ids b0..b7): b0 LowCut(30), b1 Bell(80),
        // b2 Bell(200), b3 Bell(600), b4 Bell(1500), b5 Bell(4000),
        // b6 Bell(8000), b7 HighCut(18000). Types: Bell=0, LowShelf=1,
        // HighShelf=2, LowCut=3, HighCut=4, Notch=5.
        //
        // All moves are deliberately moderate (well under the ±30 dB max) so the
        // result stays full and warm — gentle presence instead of brittle highs,
        // and low-end support so nothing reads thin or tinny.
        static const std::vector<PresetDef> presets = {
            { "Flat", {} },
            { "Vocal Clarity", {
                { "b0_freq", 80.0f },                                  // light low-cut for cleanup
                { "b5_freq", 3500.0f }, { "b5_gain", 2.5f }, { "b5_q", 1.0f },
                { "b6_type", (float) (int) BandType::HighShelf }, { "b6_freq", 9000.0f }, { "b6_gain", 2.5f } } },
            { "Warmth", {
                { "b2_freq", 220.0f }, { "b2_gain", 2.5f }, { "b2_q", 0.9f },  // low-mid body
                { "b5_freq", 4000.0f }, { "b5_gain", -2.0f },                   // tame harsh
                { "b6_type", (float) (int) BandType::HighShelf }, { "b6_freq", 10000.0f }, { "b6_gain", -1.0f } } },
            { "De-Mud", {
                { "b2_freq", 320.0f }, { "b2_gain", -3.5f }, { "b2_q", 1.3f } } },
            { "Air & Presence", {
                { "b5_freq", 3200.0f }, { "b5_gain", 1.5f },
                { "b6_type", (float) (int) BandType::HighShelf }, { "b6_freq", 11000.0f }, { "b6_gain", 3.0f } } },
            { "Tame Harsh", {
                { "b5_freq", 4500.0f }, { "b5_q", 2.0f },
                { "b5_range", -6.0f }, { "b5_thresh", -28.0f }, { "b5_atk", 2.0f }, { "b5_rel", 90.0f } } },
            { "Radio", {
                { "b0_freq", 180.0f },                                  // roll off lows
                { "b4_freq", 1500.0f }, { "b4_gain", 2.5f }, { "b4_q", 0.9f },  // mid forward
                { "b7_freq", 7000.0f } } },                            // roll off air
            { "Full & Round", {
                { "b1_type", (float) (int) BandType::LowShelf }, { "b1_freq", 110.0f }, { "b1_gain", 3.0f },
                { "b6_type", (float) (int) BandType::HighShelf }, { "b6_freq", 10000.0f }, { "b6_gain", -1.0f } } },
        };
        return presets;
    }
}

int VocalQProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String VocalQProcessor::getProgramName (int index)
{
    const auto& p = getPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? p[(size_t) index].name : juce::String();
}

void VocalQProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    applyProgram (index);
}

void VocalQProcessor::applyProgram (int index)
{
    const auto& presets = getPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;

    // Reset everything to defaults first for deterministic loading.
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& [id, value] : presets[(size_t) index].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->getNormalisableRange().convertTo0to1 (value));
}

//==============================================================================
void VocalQProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void VocalQProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    return new VocalQProcessor();
}
