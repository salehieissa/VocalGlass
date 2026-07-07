#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
VocalRackProcessor::VocalRackProcessor()
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
VocalRackProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // ---- per-module power switches ----
    layout.add (std::make_unique<AudioParameterBool>(ParameterID { "gateOn", 1 }, "Gate On",   true));
    layout.add (std::make_unique<AudioParameterBool>(ParameterID { "essOn",  1 }, "De-Ess On", true));
    layout.add (std::make_unique<AudioParameterBool>(ParameterID { "eqOn",   1 }, "EQ On",     true));
    layout.add (std::make_unique<AudioParameterBool>(ParameterID { "compOn", 1 }, "Comp On",   true));
    layout.add (std::make_unique<AudioParameterBool>(ParameterID { "heatOn", 1 }, "Heat On",   false));
    layout.add (std::make_unique<AudioParameterBool>(ParameterID { "airOn",  1 }, "Air On",    true));
    layout.add (std::make_unique<AudioParameterBool>(ParameterID { "dlyOn",  1 }, "Delay On",  true));
    layout.add (std::make_unique<AudioParameterBool>(ParameterID { "verbOn", 1 }, "Reverb On", true));
    layout.add (std::make_unique<AudioParameterBool>(ParameterID { "clipOn", 1 }, "Clip On",   true));

    // ---- GATE ----
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "gateThresh", 1 }, "Gate Threshold",
        NormalisableRange<float> (-80.0f, 0.0f, 0.1f), -48.0f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "gateRelease", 1 }, "Gate Release",
        NormalisableRange<float> (20.0f, 500.0f, 1.0f, 0.5f), 120.0f));

    // ---- DE-ESS ----
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "essAmount", 1 }, "De-Ess Amount",
        NormalisableRange<float> (0.0f, 100.0f, 0.5f), 50.0f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "essFreq", 1 }, "De-Ess Frequency",
        NormalisableRange<float> (4000.0f, 9000.0f, 1.0f, 0.6f), 6800.0f));

    // ---- EQ ----
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "eqHpf", 1 }, "EQ High-Pass",
        NormalisableRange<float> (20.0f, 300.0f, 1.0f, 0.5f), 95.0f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "eqMud", 1 }, "EQ Mud",
        NormalisableRange<float> (-6.0f, 0.0f, 0.1f), -2.5f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "eqPresence", 1 }, "EQ Presence",
        NormalisableRange<float> (-3.0f, 6.0f, 0.1f), 2.0f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "eqAir", 1 }, "EQ Air",
        NormalisableRange<float> (0.0f, 6.0f, 0.1f), 2.5f));

    // ---- COMP ----
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "compAmount", 1 }, "Comp Amount",
        NormalisableRange<float> (0.0f, 100.0f, 0.5f), 45.0f));

    // ---- HEAT ----
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "heatDrive", 1 }, "Heat Drive",
        NormalisableRange<float> (0.0f, 100.0f, 0.5f), 25.0f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "heatTone", 1 }, "Heat Tone",
        NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f));

    // ---- AIR ----
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "airAmount", 1 }, "Air Amount",
        NormalisableRange<float> (0.0f, 100.0f, 0.5f), 30.0f));

    // ---- DELAY send ----
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "dlySend", 1 }, "Delay Send",
        NormalisableRange<float> (0.0f, 100.0f, 0.5f), 12.0f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "dlyTime", 1 }, "Delay Time",
        NormalisableRange<float> (50.0f, 800.0f, 1.0f, 0.6f), 280.0f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "dlyFeedback", 1 }, "Delay Feedback",
        NormalisableRange<float> (0.0f, 90.0f, 0.5f), 30.0f));

    // ---- REVERB send ----
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "verbSend", 1 }, "Reverb Send",
        NormalisableRange<float> (0.0f, 100.0f, 0.5f), 18.0f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "verbSize", 1 }, "Reverb Size",
        NormalisableRange<float> (0.0f, 100.0f, 0.5f), 45.0f));

    // ---- CLIP ----
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "clipAmount", 1 }, "Clip Amount",
        NormalisableRange<float> (0.0f, 100.0f, 0.5f), 35.0f));

    // ---- OUTPUT ----
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "outGain", 1 }, "Output Gain",
        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    return layout;
}

//==============================================================================
void VocalRackProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = 2;

    // ---- GATE ----
    gateScFilter.prepare (spec);
    *gateScFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (
        sampleRate, 100.0f, 0.707f);
    gateScBuffer.setSize (2, samplesPerBlock);

    // Hybrid detector timing (same voicing as VocalGate).
    gatePeakAtkCoeff = std::exp (-1.0f / (0.0001f * (float) sampleRate)); // ~0.1 ms
    gatePeakRelCoeff = std::exp (-1.0f / (0.010f  * (float) sampleRate)); // ~10 ms
    gateRmsCoeff     = std::exp (-1.0f / (0.005f  * (float) sampleRate)); // ~5 ms
    gatePeakEnv = 0.0f;
    gateRmsSq   = 0.0f;
    gateState = GateState::Closed;
    gateHoldRemaining = 0;
    gateGain = 1.0f;

    // ---- DE-ESS ----
    essCrossover.prepare (spec);
    essCrossover.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    essCrossover.setCutoffFrequency (6800.0f);
    essScFilter.prepare (spec);
    *essScFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (
        sampleRate, 6800.0f, 0.707f);
    lastEssFreq = 6800.0f;
    essScBuffer.setSize (2, samplesPerBlock);
    essAtkCoeff = std::exp (-1.0f / (0.0004f * (float) sampleRate)); // ~0.4 ms
    essRelCoeff = std::exp (-1.0f / (0.080f  * (float) sampleRate)); // ~80 ms
    essEnv = 0.0f;

    // ---- EQ + AIR (coefficients rebuilt lazily in processBlock) ----
    eqHpfFilter.prepare (spec);
    eqMudFilter.prepare (spec);
    eqPresFilter.prepare (spec);
    eqAirFilter.prepare (spec);
    lastEqHpf = -1.0f; lastEqMud = -999.0f; lastEqPres = -999.0f; lastEqAir = -999.0f;

    // ---- COMP (VocalComp engine) ----
    comp.prepare (spec);

    // ---- HEAT (VocalGrit-style oversampled shaper; the up/down path always
    // runs so the plugin's latency is constant while the module is off) ----
    heatOversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    heatOversampling->initProcessing ((size_t) samplesPerBlock);
    heatDcX1[0] = heatDcX1[1] = 0.0f;
    heatDcY1[0] = heatDcY1[1] = 0.0f;
    heatTiltLp[0] = heatTiltLp[1] = 0.0f;

    // ---- AIR (VocalAir engine) ----
    air.prepare (spec);

    // ---- DELAY send (VocalDelay engine) ----
    delayEngine.prepare (spec);
    dlyBuf.setSize (2, samplesPerBlock);
    wasDlyOn = false;

    // ---- REVERB send (VocalVerb plate) ----
    verbEngine.prepare (spec);
    verbHpfFilter.prepare (spec);
    *verbHpfFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (
        sampleRate, 250.0f, 0.707f);
    verbBuf.setSize (2, samplesPerBlock);
    wasVerbOn = false;

    // ---- CLIP (VocalClip engine, 4x oversampled) ----
    clipEngine.prepare (sampleRate, samplesPerBlock, 2);

    // Both oversamplers always run (transparently when their module is off),
    // so the reported latency is constant.
    setLatencySamples (juce::roundToInt (heatOversampling->getLatencyInSamples())
                       + clipEngine.latencySamples (true));
}

bool VocalRackProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

//==============================================================================
void VocalRackProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh      = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // License gate: until activated, pass audio through clean (no processing).
    if (! license.isActivated())
        return;

    if (numSamples == 0 || numCh == 0)
        return;

    const float sr = (float) currentSampleRate;

    // ---- parameters ----
    const bool gateOn = apvts.getRawParameterValue ("gateOn")->load() > 0.5f;
    const bool essOn  = apvts.getRawParameterValue ("essOn") ->load() > 0.5f;
    const bool eqOn   = apvts.getRawParameterValue ("eqOn")  ->load() > 0.5f;
    const bool compOn = apvts.getRawParameterValue ("compOn")->load() > 0.5f;
    const bool heatOn = apvts.getRawParameterValue ("heatOn")->load() > 0.5f;
    const bool airOn  = apvts.getRawParameterValue ("airOn") ->load() > 0.5f;
    const bool dlyOn  = apvts.getRawParameterValue ("dlyOn") ->load() > 0.5f;
    const bool verbOn = apvts.getRawParameterValue ("verbOn")->load() > 0.5f;
    const bool clipOn = apvts.getRawParameterValue ("clipOn")->load() > 0.5f;

    const float gateThresh  = apvts.getRawParameterValue ("gateThresh")->load();
    const float gateRelease = apvts.getRawParameterValue ("gateRelease")->load();
    const float essAmount   = apvts.getRawParameterValue ("essAmount")->load();
    const float essFreq     = apvts.getRawParameterValue ("essFreq")->load();
    const float eqHpf       = apvts.getRawParameterValue ("eqHpf")->load();
    const float eqMud       = apvts.getRawParameterValue ("eqMud")->load();
    const float eqPresence  = apvts.getRawParameterValue ("eqPresence")->load();
    const float eqAir       = apvts.getRawParameterValue ("eqAir")->load();
    const float compAmount  = apvts.getRawParameterValue ("compAmount")->load();
    const float heatDrive   = apvts.getRawParameterValue ("heatDrive")->load();
    const float heatTone    = apvts.getRawParameterValue ("heatTone")->load();
    const float airAmount   = apvts.getRawParameterValue ("airAmount")->load();
    const float dlySend     = apvts.getRawParameterValue ("dlySend")->load();
    const float dlyTime     = apvts.getRawParameterValue ("dlyTime")->load();
    const float dlyFeedback = apvts.getRawParameterValue ("dlyFeedback")->load();
    const float verbSend    = apvts.getRawParameterValue ("verbSend")->load();
    const float verbSize    = apvts.getRawParameterValue ("verbSize")->load();
    const float clipAmount  = apvts.getRawParameterValue ("clipAmount")->load();
    const float outGain     = apvts.getRawParameterValue ("outGain")->load();

    float gateGrMax = 0.0f, essGrMax = 0.0f, compGrMax = 0.0f, clipMax = 0.0f;

    // ---- input meter (pre-chain) ----
    float inPeak = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
        inPeak = juce::jmax (inPeak, buffer.getMagnitude (ch, 0, numSamples));
    inDb.store (juce::Decibels::gainToDecibels (inPeak + 1.0e-9f));

    //==========================================================================
    // GATE — VocalGate's hybrid detector + state machine with fixed internals:
    // attack 0.5 ms, hold 25 ms, range -25 dB, hysteresis 4 dB, SC HPF 100 Hz.
    //==========================================================================
    if (gateOn)
    {
        constexpr float attackMs   = 0.5f;
        constexpr float holdMs     = 25.0f;
        constexpr float rangeDb    = 25.0f;
        constexpr float hysteresis = 4.0f;

        const float atkCoeff = std::exp (-1.0f / (attackMs * 0.001f * sr));
        const float relCoeff = std::exp (-1.0f / (juce::jmax (1.0f, gateRelease) * 0.001f * sr));
        const int   holdSamples = (int) std::round (holdMs * 0.001f * sr);
        const float floorGain   = juce::Decibels::decibelsToGain (-rangeDb);
        const float closeThresh = gateThresh - hysteresis;

        gateScBuffer.makeCopyOf (buffer, true);
        {
            juce::dsp::AudioBlock<float> block (gateScBuffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            gateScFilter.process (ctx);
        }

        const float* scL = gateScBuffer.getReadPointer (0);
        const float* scR = gateScBuffer.getReadPointer (juce::jmin (1, numCh - 1));

        for (int i = 0; i < numSamples; ++i)
        {
            const float scMono = 0.5f * (scL[i] + scR[i]);
            const float rect   = std::abs (scMono);

            if (rect > gatePeakEnv) gatePeakEnv = gatePeakAtkCoeff * (gatePeakEnv - rect) + rect;
            else                    gatePeakEnv = gatePeakRelCoeff * (gatePeakEnv - rect) + rect;

            gateRmsSq = gateRmsCoeff * gateRmsSq + (1.0f - gateRmsCoeff) * scMono * scMono;

            const float env     = juce::jmax (gatePeakEnv, std::sqrt (gateRmsSq)) + 1.0e-9f;
            const float levelDb = juce::Decibels::gainToDecibels (env);

            switch (gateState)
            {
                case GateState::Closed:
                    if (levelDb > gateThresh) gateState = GateState::Open;
                    break;
                case GateState::Open:
                    if (levelDb < closeThresh) { gateState = GateState::Hold; gateHoldRemaining = holdSamples; }
                    break;
                case GateState::Hold:
                    if (levelDb > gateThresh)          gateState = GateState::Open;
                    else if (--gateHoldRemaining <= 0) gateState = GateState::Closed;
                    break;
            }

            const float targetGain = (gateState == GateState::Closed) ? floorGain : 1.0f;
            if (targetGain > gateGain) gateGain = atkCoeff * (gateGain - targetGain) + targetGain;
            else                       gateGain = relCoeff * (gateGain - targetGain) + targetGain;
            gateGain += 1.0e-12f; gateGain -= 1.0e-12f;   // flush denormals

            gateGrMax = juce::jmax (gateGrMax,
                                    -juce::Decibels::gainToDecibels (gateGain + 1.0e-9f));

            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer (ch)[i] *= gateGain;
        }
    }
    else
    {
        gateGain = 1.0f;
        gateState = GateState::Closed;
    }

    //==========================================================================
    // DE-ESS — Linkwitz-Riley split at essFreq; only the high band is ducked.
    // essAmount maps to threshold depth, capped at ~8 dB of reduction.
    //==========================================================================
    if (essOn && essAmount > 0.01f)
    {
        if (std::abs (essFreq - lastEssFreq) >= 0.5f)
        {
            lastEssFreq = essFreq;
            essCrossover.setCutoffFrequency (essFreq);
            *essScFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (
                currentSampleRate, essFreq, 0.707f);
        }

        const float maxGr  = 8.0f * essAmount * 0.01f;           // depth cap
        const float thresh = -22.0f - 0.20f * essAmount;         // deeper at higher amounts
        constexpr float ratio = 6.0f;
        const float slope = 1.0f - 1.0f / ratio;

        essScBuffer.makeCopyOf (buffer, true);
        {
            juce::dsp::AudioBlock<float> block (essScBuffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            essScFilter.process (ctx);
        }

        const float* scL = essScBuffer.getReadPointer (0);
        const float* scR = essScBuffer.getReadPointer (juce::jmin (1, numCh - 1));

        for (int i = 0; i < numSamples; ++i)
        {
            const float scMono = 0.5f * (scL[i] + scR[i]);
            const float rect   = std::abs (scMono);

            if (rect > essEnv) essEnv = essAtkCoeff * (essEnv - rect) + rect;
            else               essEnv = essRelCoeff * (essEnv - rect) + rect;

            const float levelDb = juce::Decibels::gainToDecibels (essEnv + 1.0e-9f);
            const float grDb    = juce::jlimit (0.0f, maxGr,
                                                levelDb > thresh ? (levelDb - thresh) * slope : 0.0f);
            essGrMax = juce::jmax (essGrMax, grDb);

            const float gain = juce::Decibels::decibelsToGain (-grDb);

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* data = buffer.getWritePointer (ch);
                float low = 0.0f, high = 0.0f;
                essCrossover.processSample (ch, data[i], low, high);
                data[i] = low + high * gain;
            }
        }
    }

    //==========================================================================
    // EQ — HPF + mud dip + presence + air shelf (per-channel IIR chains).
    //==========================================================================
    if (eqOn)
    {
        if (std::abs (eqHpf - lastEqHpf) >= 0.5f)
        {
            lastEqHpf = eqHpf;
            *eqHpfFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (
                currentSampleRate, eqHpf, 0.707f);           // 12 dB/oct
        }
        if (std::abs (eqMud - lastEqMud) >= 0.05f)
        {
            lastEqMud = eqMud;
            *eqMudFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                currentSampleRate, 350.0f, 1.1f, juce::Decibels::decibelsToGain (eqMud));
        }
        if (std::abs (eqPresence - lastEqPres) >= 0.05f)
        {
            lastEqPres = eqPresence;
            *eqPresFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                currentSampleRate, 3500.0f, 0.9f, juce::Decibels::decibelsToGain (eqPresence));
        }
        if (std::abs (eqAir - lastEqAir) >= 0.05f)
        {
            lastEqAir = eqAir;
            *eqAirFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                currentSampleRate, 12000.0f, 0.707f, juce::Decibels::decibelsToGain (eqAir));
        }

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        eqHpfFilter.process (ctx);
        eqMudFilter.process (ctx);
        eqPresFilter.process (ctx);
        eqAirFilter.process (ctx);
    }

    //==========================================================================
    // COMP — VocalComp's Compressor engine (ARC voicing: 6 dB knee, 20% RMS
    // blend). The one-knob macro drives the threshold from -10 dB (0%) down to
    // -40 dB (100%) at fixed 3:1, attack 5 ms / release 80 ms, auto makeup.
    //==========================================================================
    if (compOn && compAmount > 0.01f)
    {
        const float threshold = -10.0f - 0.30f * compAmount;
        constexpr float ratio = 3.0f;
        const float makeupDb  = std::abs (threshold) * (1.0f - 1.0f / ratio) * 0.6f;

        comp.setParams (threshold, ratio, 5.0f, 80.0f, makeupDb,
                        1.0f /*mix*/, 0.0f /*trim*/, Compressor::Arc,
                        -80.0f /*internal gate off*/);
        comp.process (buffer);
        compGrMax = comp.meterGR.load();
    }

    //==========================================================================
    // HEAT — VocalGrit's GRIT stage, Warm curve: drive into a 4x-oversampled
    // tanh(x + bias) shaper (bias scales with drive for Grit's even-harmonic
    // asymmetry), DC blocker, level compensation at a -12 dBFS reference, then
    // a one-pole tilt tone around 1.2 kHz. The oversampler always runs so the
    // reported latency is constant while the module is off.
    //==========================================================================
    {
        const bool heatActive = heatOn && (heatDrive > 0.01f || std::abs (heatTone) > 0.5f);

        juce::dsp::AudioBlock<float> block (buffer);
        auto sub = block.getSubsetChannelBlock (0, (size_t) numCh);

        if (heatActive)
        {
            const float driveDb    = heatDrive * 0.01f * 18.0f;
            const float g          = juce::Decibels::decibelsToGain (driveDb);
            const float bias       = 0.30f * heatDrive * 0.01f;
            const float heatMakeup = 0.25f / std::tanh (0.25f * g); // unity @ -12 dBFS ref

            // Grit's order: input drive, then the oversampled waveshaper.
            buffer.applyGain (0, numSamples, g);
            auto up = heatOversampling->processSamplesUp (sub);
            for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
            {
                auto* data = up.getChannelPointer (ch);
                const size_t nOs = up.getNumSamples();
                for (size_t i = 0; i < nOs; ++i)
                    data[i] = std::tanh (data[i] + bias);        // Warm curve
            }
            heatOversampling->processSamplesDown (sub);

            const float tiltDb = heatTone * 0.01f * 4.5f;
            const float gHi = juce::Decibels::decibelsToGain ( tiltDb * 0.5f);
            const float gLo = juce::Decibels::decibelsToGain (-tiltDb * 0.5f);
            const float lpCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 1200.0f / sr);

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* data = buffer.getWritePointer (ch);
                float lp = heatTiltLp[ch];
                for (int i = 0; i < numSamples; ++i)
                {
                    // DC blocker (removes the bias offset), Grit-style one-pole
                    const float in  = data[i];
                    const float out = in - heatDcX1[ch] + 0.995f * heatDcY1[ch];
                    heatDcX1[ch] = in;
                    heatDcY1[ch] = out;

                    const float sat = out * heatMakeup;
                    lp += lpCoeff * (sat - lp);
                    data[i] = lp * gLo + (sat - lp) * gHi;
                }
                heatTiltLp[ch] = lp;
            }
        }
        else
        {
            heatOversampling->processSamplesUp (sub);
            heatOversampling->processSamplesDown (sub);
        }
    }

    //==========================================================================
    // AIR — VocalAir's AirEnhancer, high band only (13.5 kHz dynamic shelf +
    // harmonic excitement). The one-knob amount maps to 0..+6 dB of shelf.
    //==========================================================================
    if (airOn && airAmount > 0.01f)
    {
        air.setParams (0.0f /*mid*/, airAmount * 0.01f * 0.5f /*high: 0..+6 dB*/,
                       0.0f /*trim*/, true);
        air.process (buffer);
    }

    //==========================================================================
    // DELAY send — VocalDelay's DelayEngine on a send bus tapped post-AIR. The
    // dry path is untouched: the send amount scales what enters the engine and
    // the 100%-wet return is summed straight back in (pre-CLIP). The engine's
    // repeat filters run at 300 Hz HPF / 6 kHz LPF so every repeat sits behind
    // the vocal; Dual routing, no chorus modulation, subtle analog colour.
    //==========================================================================
    if (dlyOn && dlySend > 0.01f)
    {
        if (! wasDlyOn)   // don't replay stale line contents after re-enabling
            delayEngine.reset();
        wasDlyOn = true;

        dlyBuf.makeCopyOf (buffer, true);
        dlyBuf.applyGain (0, numSamples, dlySend * 0.01f);

        DelayEngine::Params dp;
        const float dSamp = dlyTime * 0.001f * sr;
        dp.delaySamplesL = dSamp;
        dp.delaySamplesR = dSamp;
        dp.mode          = DelayEngine::Dual;
        dp.feedback      = dlyFeedback * 0.01f;
        dp.depth         = 0.0f;
        dp.rate          = 0.2f;
        dp.hpHz          = 300.0f;    // send input / repeats high-passed
        dp.lpHz          = 6000.0f;   // feedback path low-passed
        dp.dryWet        = 1.0f;      // 100% wet return
        dp.outputGain    = 1.0f;
        dp.analog        = 0.2f;      // VocalDelay's default tape colour
        dp.lofi          = false;
        delayEngine.setParams (dp);
        delayEngine.process (dlyBuf);

        for (int ch = 0; ch < numCh; ++ch)
            buffer.addFrom (ch, 0, dlyBuf, juce::jmin (ch, dlyBuf.getNumChannels() - 1),
                            0, numSamples);
    }
    else
    {
        wasDlyOn = false;
    }

    //==========================================================================
    // REVERB send — VocalVerb's Dattorro plate on a send bus tapped post-AIR.
    // Send input HPF 250 Hz; the engine's output high-cut sits at 9 kHz (the
    // spec'd wet low-pass). Size maps to tank size + decay (1..4 s), plate
    // mode / modern colour.
    //==========================================================================
    if (verbOn && verbSend > 0.01f)
    {
        if (! wasVerbOn)
            verbEngine.reset();
        wasVerbOn = true;

        verbBuf.makeCopyOf (buffer, true);
        verbBuf.applyGain (0, numSamples, verbSend * 0.01f);
        {
            juce::dsp::AudioBlock<float> block (verbBuf);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            verbHpfFilter.process (ctx);
        }

        PlateReverb::Params vp;
        vp.mix         = 1.0f;                      // 100% wet return
        vp.predelayMs  = 20.0f;
        vp.decaySec    = 1.0f + verbSize * 0.03f;   // 1 .. 4 s
        vp.dampHz      = 6000.0f;
        vp.dampShelfDb = -24.0f;
        vp.bassHz      = 700.0f;
        vp.bassMult    = 1.2f;
        vp.size        = verbSize * 0.01f;
        vp.attack      = 0.3f;
        vp.diffEarly   = 1.0f;
        vp.diffLate    = 1.0f;
        vp.modRate     = 2.5f;
        vp.modDepth    = 0.4f;
        vp.highCut     = 9000.0f;                   // wet low-pass per spec
        vp.lowCut      = 20.0f;
        vp.mode        = 1;                         // plate
        vp.color       = 1;                         // modern
        verbEngine.setParams (vp);
        verbEngine.process (verbBuf);

        for (int ch = 0; ch < numCh; ++ch)
            buffer.addFrom (ch, 0, verbBuf, juce::jmin (ch, verbBuf.getNumChannels() - 1),
                            0, numSamples);
    }
    else
    {
        wasVerbOn = false;
    }

    //==========================================================================
    // CLIP — VocalClip's ClipEngine: drive (0..12 dB) into the fixed SOFT
    // cubic curve at a -0.3 dBFS ceiling, 4x oversampled, 100% wet, no makeup.
    // When off, the oversampler still runs (transparently) so the reported
    // latency stays constant.
    //==========================================================================
    {
        ClipEngine::Params cp;
        cp.driveDb   = clipAmount * 0.01f * 12.0f;
        cp.shape     = ClipEngine::Soft;
        cp.ceilingDb = -0.3f;
        cp.mix       = 1.0f;
        cp.outDb     = 0.0f;
        cp.hq        = true;
        clipEngine.setParams (cp);

        if (clipOn)
        {
            clipEngine.process (buffer);
            clipMax = clipEngine.clipDb.load();
        }
        else
        {
            clipEngine.processTransparent (buffer);
        }
    }

    //==========================================================================
    // OUTPUT
    //==========================================================================
    buffer.applyGain (0, numSamples, juce::Decibels::decibelsToGain (outGain));

    float outPeak = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
        outPeak = juce::jmax (outPeak, buffer.getMagnitude (ch, 0, numSamples));

    // ---- meters ----
    outDb.store    (juce::Decibels::gainToDecibels (outPeak + 1.0e-9f));
    gateGrDb.store (gateGrMax);
    essGrDb.store  (essGrMax);
    compGrDb.store (compGrMax);
    clipDb.store   (clipMax);
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
            // Index 0 == the parameter defaults (incl. dlySend 12 / verbSend 18).
            { "Drake Mix",      { } },
            { "Clean Pop",      { {"essAmount",40}, {"eqPresence",1.5f}, {"compAmount",35},
                                  {"heatOn",0}, {"airAmount",45}, {"clipAmount",20},
                                  {"verbSend",25}, {"dlySend",8} } },
            { "Aggressive Rap", { {"gateThresh",-42}, {"compAmount",65}, {"heatOn",1},
                                  {"heatDrive",45}, {"clipAmount",55}, {"eqPresence",3},
                                  {"dlySend",20}, {"verbSend",10} } },
            { "Warm R&B",       { {"heatOn",1}, {"heatDrive",30}, {"heatTone",-30},
                                  {"airAmount",40}, {"compAmount",40}, {"clipAmount",25},
                                  {"eqMud",-3.5f}, {"verbSend",30}, {"verbSize",60},
                                  {"dlySend",10} } },
            { "Bypass All",     { {"gateOn",0}, {"essOn",0}, {"eqOn",0}, {"compOn",0},
                                  {"heatOn",0}, {"airOn",0}, {"dlyOn",0}, {"verbOn",0},
                                  {"clipOn",0} } },
        };
        return presets;
    }
}

int VocalRackProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String VocalRackProcessor::getProgramName (int index)
{
    const auto& presets = getPresets();
    if (juce::isPositiveAndBelow (index, (int) presets.size()))
        return presets[(size_t) index].name;
    return {};
}

void VocalRackProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;
    currentProgram = index;
    applyProgram (index);
}

void VocalRackProcessor::applyProgram (int index)
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
juce::AudioProcessorEditor* VocalRackProcessor::createEditor()
{
    return new VocalRackEditor (*this);
}

void VocalRackProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void VocalRackProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalRackProcessor();
}
