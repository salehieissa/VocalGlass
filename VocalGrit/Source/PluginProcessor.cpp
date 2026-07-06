#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Exact "did this value change since last block" test. We intentionally use
    // bit-exact comparison (not a tolerance) so a cached coefficient is only
    // skipped when the input is identical -> the result stays bit-identical to
    // recomputing every block. Wrapped to silence the project's -Wfloat-equal.
    inline bool valueChanged (float a, float b) noexcept
    {
        JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wfloat-equal")
        return a != b;
        JUCE_END_IGNORE_WARNINGS_GCC_LIKE
    }
}

//==============================================================================
VocalGritProcessor::VocalGritProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    // 4x oversampling (2^2). Higher = cleaner but more CPU/latency.
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        2,                                                    // num channels
        2,                                                    // factor = 2^2 = 4x
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true);                                                // max quality

    // Load any cached activation and validate online in the background.
    license.loadCachedAndValidate();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
VocalGritProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // ---- GRIT ----
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { "gritOn", 1 }, "Grit On", true));

    // Grit (the big dial): overall bite, mapped to input drive in dB.
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "drive", 1 }, "Grit",
        NormalisableRange<float> (0.0f, 40.0f, 0.01f), 16.0f));

    // Character: which waveshaping curve (clean -> blown).
    layout.add (std::make_unique<AudioParameterChoice>(
        ParameterID { "mode", 1 }, "Character",
        StringArray { "Clean", "Warm", "Dirty", "Blown" }, 2));

    // Drive slider: asymmetry/bias -> grittier harmonics.
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "bias", 1 }, "Drive",
        NormalisableRange<float> (0.0f, 0.8f, 0.001f), 0.3f));

    // Tone: post low-pass cutoff (tame harsh high fizz).
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "tone", 1 }, "Tone",
        NormalisableRange<float> (1000.0f, 20000.0f, 1.0f, 0.4f), 9000.0f));

    // Width: stereo widener (mid/side). 0 = mono, 1 = normal, 2 = wide.
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "stereoWidth", 1 }, "Width",
        NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.0f));

    // Formant: 0 = down/dark, 0.5 = neutral, 1 = up/bright.
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "formant", 1 }, "Formant",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    // Low Cut: pre high-pass cutoff (hidden, keeps lows clean).
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "lowcut", 1 }, "Low Cut",
        NormalisableRange<float> (20.0f, 400.0f, 1.0f, 0.4f), 90.0f));

    // Mix: raw vocal <-> processed vocal blend.
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    // ---- TEXTURE PILLS ----
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { "fuzzOn", 1 }, "Fuzz", false));
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { "ampOn", 1 }, "Amp", false));
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { "speakerOn", 1 }, "Speaker", false));
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { "presenceOn", 1 }, "Presence", false));

    // ---- DOUBLER ----
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { "doublerOn", 1 }, "Doubler On", false));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "doublerDetune", 1 }, "Dbl Detune",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "doublerWidth", 1 }, "Dbl Width",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "doublerMix", 1 }, "Dbl Mix",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    // ---- DELAY ----
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { "delayOn", 1 }, "Delay On", false));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "delayTime", 1 }, "Delay Time",
        NormalisableRange<float> (20.0f, 1000.0f, 1.0f, 0.5f), 300.0f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "delayFeedback", 1 }, "Delay FB",
        NormalisableRange<float> (0.0f, 0.95f, 0.001f), 0.35f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "delayMix", 1 }, "Delay Mix",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));
    // Host-tempo sync (off by default so existing behaviour is unchanged).
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { "delaySync", 1 }, "Delay Sync", false));
    // Note division used when sync is on (dotted = x1.5, triplet = x2/3).
    layout.add (std::make_unique<AudioParameterChoice>(
        ParameterID { "delayDiv", 1 }, "Delay Div",
        StringArray { "1/1", "1/2", "1/2.", "1/2T", "1/4", "1/4.", "1/4T",
                      "1/8", "1/8.", "1/8T", "1/16", "1/16.", "1/16T", "1/32" },
        7)); // default 1/8

    // ---- REVERB ----
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { "reverbOn", 1 }, "Reverb On", false));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "reverbSize", 1 }, "Verb Size",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "reverbDamp", 1 }, "Verb Damp",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "reverbMix", 1 }, "Verb Mix",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.25f));

    // ---- GLITCH (tempo-synced gate / stutter) ----
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { "glitchOn", 1 }, "Glitch On", false));
    // Rate selects a note division (0..1 -> 1/4 .. 1/32, snapped in the DSP).
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "glitchRate", 1 }, "Glitch Rate",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.6f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "glitchDepth", 1 }, "Glitch Depth",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.85f));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "glitchMix", 1 }, "Glitch Mix",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    // ---- OUTPUT trim (per-preset level matching) ----
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { "output", 1 }, "Output",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    return layout;
}

//==============================================================================
void VocalGritProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = 2;

    oversampling->reset();
    oversampling->initProcessing ((size_t) samplesPerBlock);

    formantProc.prepare (spec);

    formantHP.prepare (spec);
    formantHP.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    formantHP.setCutoffFrequency (20.0f);

    formantLowShelf.prepare (spec);
    *formantLowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        sampleRate, 200.0f, 0.7f, 1.0f);
    lastFormantUp  = -1.0f;
    lastLowCut     = -1.0f;
    lastTone       = -1.0f;
    lastFormantHPc = -1.0f;

    // Tell the host our total latency (oversampling + formant STFT block) so it
    // can align tracks and stay phase-correct.
    setLatencySamples ((int) oversampling->getLatencyInSamples()
                       + formantProc.getLatencySamples());

    preHighPass.prepare (spec);
    preHighPass.setType (juce::dsp::StateVariableTPTFilterType::highpass);

    toneLowPass.prepare (spec);
    toneLowPass.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    // "Amp" = low-mid warmth (low shelf boost).
    ampShelf.prepare (spec);
    *ampShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        sampleRate, 220.0f, 0.7f, juce::Decibels::decibelsToGain (4.0f));

    // "Presence" = air/clarity (high shelf boost).
    presenceShelf.prepare (spec);
    *presenceShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, 5000.0f, 0.7f, juce::Decibels::decibelsToGain (5.0f));

    // "Speaker" = guitar-cab-ish band limit.
    speakerHP.prepare (spec);
    speakerHP.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    speakerHP.setCutoffFrequency (130.0f);
    speakerLP.prepare (spec);
    speakerLP.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    speakerLP.setCutoffFrequency (4200.0f);

    doubler.prepare (spec);
    delay.prepare (spec);
    reverb.prepare (spec);
    glitch.prepare (spec);

    dcX1.fill (0.0f);
    dcY1.fill (0.0f);

    dryBuffer.setSize (2, samplesPerBlock);
}

bool VocalGritProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

//==============================================================================
float VocalGritProcessor::shape (float x, int mode, float bias)
{
    // Add bias BEFORE shaping to create asymmetry (even harmonics).
    x += bias;

    switch (mode)
    {
        case 0: // Clean — gentle, lots of headroom
            return std::tanh (0.7f * x);

        case 1: // Warm — smooth tube-like saturation
            return std::tanh (x);

        case 2: // Dirty — harder knee, between tanh and clip
        {
            const float t = std::tanh (1.5f * x);
            const float c = juce::jlimit (-1.0f, 1.0f, x);
            return 0.5f * t + 0.5f * c;
        }

        case 3: // Blown — asymmetric exponential fuzz, ragged
            return x >= 0.0f ? 1.0f - std::exp (-x)
                             : -1.0f + std::exp (x * 0.7f);

        default:
            return std::tanh (x);
    }
}

void VocalGritProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh      = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Clear any output channels the host gave us beyond our inputs.
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // License gate: until activated, pass audio through clean (no processing).
    if (! license.isActivated())
        return;

    // Input metering (peak of the incoming signal).
    inputLevel.store (buffer.getMagnitude (0, numSamples));

    // --- read parameters (atomically) ---
    const bool  gritOn  = apvts.getRawParameterValue ("gritOn")->load() > 0.5f;
    const float driveDb = apvts.getRawParameterValue ("drive")->load();
    const int   mode    = (int) apvts.getRawParameterValue ("mode")->load();
    const float bias    = apvts.getRawParameterValue ("bias")->load();
    const float lowCut  = apvts.getRawParameterValue ("lowcut")->load();
    const float tone    = apvts.getRawParameterValue ("tone")->load();
    const float width   = apvts.getRawParameterValue ("stereoWidth")->load();
    const float mix     = apvts.getRawParameterValue ("mix")->load();

    const bool fuzzOn     = apvts.getRawParameterValue ("fuzzOn")->load()     > 0.5f;
    const bool ampOn      = apvts.getRawParameterValue ("ampOn")->load()      > 0.5f;
    const bool speakerOn  = apvts.getRawParameterValue ("speakerOn")->load()  > 0.5f;
    const bool presenceOn = apvts.getRawParameterValue ("presenceOn")->load() > 0.5f;

    const float driveGain = juce::Decibels::decibelsToGain (driveDb);

    //==========================================================================
    // STAGE 1 — GRIT
    //==========================================================================
    if (gritOn)
    {
        // Keep a clean copy for the grit dry/wet mix.
        dryBuffer.makeCopyOf (buffer, true);

        // 1a. Pre high-pass (keeps low end out of the distortion).
        if (valueChanged (lowCut, lastLowCut)) { preHighPass.setCutoffFrequency (lowCut); lastLowCut = lowCut; }
        {
            juce::dsp::AudioBlock<float> block (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            preHighPass.process (ctx);
        }

        // 1b. "Amp" — low-mid warmth before the drive.
        if (ampOn)
        {
            juce::dsp::AudioBlock<float> block (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            ampShelf.process (ctx);
        }

        // 1c. Input drive.
        buffer.applyGain (driveGain);

        // 1d. Oversample -> waveshape (+ optional fuzz) -> downsample.
        juce::dsp::AudioBlock<float> baseBlock (buffer);
        auto osBlock = oversampling->processSamplesUp (baseBlock);

        for (size_t ch = 0; ch < osBlock.getNumChannels(); ++ch)
        {
            auto* data = osBlock.getChannelPointer (ch);
            const size_t n = osBlock.getNumSamples();
            for (size_t i = 0; i < n; ++i)
            {
                float s = shape (data[i], mode, bias);
                if (fuzzOn) // a second, harder asymmetric stage = gnarly fuzz
                    s = s >= 0.0f ? 1.0f - std::exp (-2.5f * s)
                                  : -1.0f + std::exp (2.5f * s);
                data[i] = s;
            }
        }

        oversampling->processSamplesDown (baseBlock);

        // 1e. DC blocker (removes offset from biasing).
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            const int safeCh = juce::jmin (ch, 1);

            for (int i = 0; i < numSamples; ++i)
            {
                const float in  = data[i];
                // y[n] = x[n] - x[n-1] + R*y[n-1]   (R≈0.995 one-pole DC block)
                const float out = in - dcX1[safeCh] + 0.995f * dcY1[safeCh];
                dcX1[safeCh] = in;
                dcY1[safeCh] = out;
                data[i] = out;
            }
        }

        // 1f. "Speaker" — guitar-cab band limit.
        if (speakerOn)
        {
            juce::dsp::AudioBlock<float> block (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            speakerHP.process (ctx);
            speakerLP.process (ctx);
        }

        // 1g. "Presence" — high-shelf air boost.
        if (presenceOn)
        {
            juce::dsp::AudioBlock<float> block (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            presenceShelf.process (ctx);
        }

        // 1h. Tone low-pass.
        if (valueChanged (tone, lastTone)) { toneLowPass.setCutoffFrequency (tone); lastTone = tone; }
        {
            juce::dsp::AudioBlock<float> block (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            toneLowPass.process (ctx);
        }

        // 1i. Grit dry/wet blend (raw vocal <-> processed vocal).
        const float wet = mix;
        const float dry = 1.0f - mix;
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* wetData = buffer.getWritePointer (ch);
            const auto* dryData = dryBuffer.getReadPointer (juce::jmin (ch, dryBuffer.getNumChannels() - 1));
            for (int i = 0; i < numSamples; ++i)
                wetData[i] = wetData[i] * wet + dryData[i] * dry;
        }

        // 1j. Stereo width (mid/side).
        if (numCh >= 2 && std::abs (width - 1.0f) > 1.0e-4f)
        {
            auto* L = buffer.getWritePointer (0);
            auto* R = buffer.getWritePointer (1);
            for (int i = 0; i < numSamples; ++i)
            {
                const float mid  = 0.5f * (L[i] + R[i]);
                const float side = 0.5f * (L[i] - R[i]) * width;
                L[i] = mid + side;
                R[i] = mid - side;
            }
        }
    }

    //==========================================================================
    // STAGE 1.5 — FORMANT (true spectral shift; always runs for constant
    // latency — at neutral it's transparent, just the reported STFT delay).
    //==========================================================================
    {
        const float fShift = (apvts.getRawParameterValue ("formant")->load() - 0.5f) * 2.0f;
        formantProc.setShift (fShift);
        formantProc.process (buffer);

        const float downAmt = juce::jmax (0.0f, -fShift);   // 0..1 deep
        const float upAmt   = juce::jmax (0.0f,  fShift);   // 0..1 bright

        // Deep voice -> sweep a high-pass up toward 140 Hz to clear the mud.
        if (downAmt > 0.001f)
        {
            const float hpCut = juce::jmap (downAmt, 0.0f, 1.0f, 20.0f, 140.0f);
            if (valueChanged (hpCut, lastFormantHPc)) { formantHP.setCutoffFrequency (hpCut); lastFormantHPc = hpCut; }
            juce::dsp::AudioBlock<float> block (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            formantHP.process (ctx);
        }

        // Bright voice -> add low-end body back with a low shelf (up to +5 dB).
        if (upAmt > 0.001f)
        {
            if (std::abs (upAmt - lastFormantUp) > 1.0e-3f)
            {
                lastFormantUp = upAmt;
                *formantLowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
                    currentSampleRate, 200.0f, 0.7f,
                    juce::Decibels::decibelsToGain (5.0f * upAmt));
            }
            juce::dsp::AudioBlock<float> block (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            formantLowShelf.process (ctx);
        }
    }

    //==========================================================================
    // STAGE 2 — DOUBLER
    //==========================================================================
    if (apvts.getRawParameterValue ("doublerOn")->load() > 0.5f)
    {
        doubler.setParams (apvts.getRawParameterValue ("doublerDetune")->load(),
                           apvts.getRawParameterValue ("doublerWidth")->load(),
                           apvts.getRawParameterValue ("doublerMix")->load());
        doubler.process (buffer);
    }

    //==========================================================================
    // STAGE 2.5 — GLITCH (tempo-synced gate / stutter). Sits before delay &
    // reverb so the chopped signal blooms into the tails.
    //==========================================================================
    if (apvts.getRawParameterValue ("glitchOn")->load() > 0.5f)
    {
        double bpm = 120.0;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm())
                    bpm = *b;
        bpm = juce::jlimit (40.0, 300.0, bpm);

        // Snap the rate knob (0..1) to a musical note division.
        static const double beatsPerStep[6] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        const float rate = apvts.getRawParameterValue ("glitchRate")->load();
        const int   idx  = juce::jlimit (0, 5, juce::roundToInt (rate * 5.0f));
        const double stepLen = (60.0 / bpm) * beatsPerStep[idx] * currentSampleRate;

        glitch.setParams (stepLen,
                          apvts.getRawParameterValue ("glitchDepth")->load(),
                          apvts.getRawParameterValue ("glitchMix")->load());
        glitch.process (buffer);
    }

    //==========================================================================
    // STAGE 3 — DELAY
    //==========================================================================
    if (apvts.getRawParameterValue ("delayOn")->load() > 0.5f)
    {
        float delayTimeMs = apvts.getRawParameterValue ("delayTime")->load();

        // When synced, derive the echo time from the host tempo + note division
        // instead of the free time knob (same playhead/BPM read as the glitch).
        if (apvts.getRawParameterValue ("delaySync")->load() > 0.5f)
        {
            double bpm = 120.0;
            if (auto* ph = getPlayHead())
                if (auto pos = ph->getPosition())
                    if (auto b = pos->getBpm())
                        bpm = *b;
            bpm = juce::jlimit (40.0, 300.0, bpm);

            // Beats (in quarter-note units) per division. Quarter = 60/bpm sec.
            static const double beatsPerDiv[14] =
            {
                4.0,        // 1/1
                2.0,        // 1/2
                3.0,        // 1/2.  (dotted = x1.5)
                4.0 / 3.0,  // 1/2T  (triplet = x2/3)
                1.0,        // 1/4
                1.5,        // 1/4.
                2.0 / 3.0,  // 1/4T
                0.5,        // 1/8
                0.75,       // 1/8.
                1.0 / 3.0,  // 1/8T
                0.25,       // 1/16
                0.375,      // 1/16.
                1.0 / 6.0,  // 1/16T
                0.125       // 1/32
            };
            const int idx = juce::jlimit (0, 13,
                (int) apvts.getRawParameterValue ("delayDiv")->load());
            const double seconds = beatsPerDiv[idx] * (60.0 / bpm);
            // Clamp to the delay line's max time (2 s); setParams clamps too.
            delayTimeMs = (float) (juce::jmin (seconds, 2.0) * 1000.0);
        }

        delay.setParams (delayTimeMs,
                         apvts.getRawParameterValue ("delayFeedback")->load(),
                         apvts.getRawParameterValue ("delayMix")->load());
        delay.process (buffer);
    }

    //==========================================================================
    // STAGE 4 — REVERB
    //==========================================================================
    if (apvts.getRawParameterValue ("reverbOn")->load() > 0.5f)
    {
        reverb.setParams (apvts.getRawParameterValue ("reverbSize")->load(),
                          apvts.getRawParameterValue ("reverbDamp")->load(),
                          apvts.getRawParameterValue ("reverbMix")->load());
        reverb.process (buffer);
    }

    //==========================================================================
    // OUTPUT — global trim (used to level-match presets).
    //==========================================================================
    const float outDb = apvts.getRawParameterValue ("output")->load();
    if (std::abs (outDb) > 1.0e-4f)
        buffer.applyGain (juce::Decibels::decibelsToGain (outDb));

    // Output metering (peak of the final signal).
    outputLevel.store (buffer.getMagnitude (0, numSamples));
}

//==============================================================================
// Presets — these drive the header browser in the UI.
namespace
{
    struct PresetDef
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values;
    };

    const std::vector<PresetDef>& getPresets()
    {
        // applyProgram() resets every parameter to its default first, so each
        // preset only needs to list the values that make it what it is.
        // "output" is a per-preset trim (dB) used to keep loudness consistent —
        // hotter/distorted presets get pulled down so switching presets doesn't
        // jump in level. Some also lean on "mix" to stay tasteful.
        static const std::vector<PresetDef> presets =
        {
            // First program reproduces the plugin's DEFAULT parameter state so
            // the header browser and the loaded patch can't disagree on launch.
            { "default",
              { {"drive",16}, {"mode",2}, {"bias",0.3f}, {"tone",9000}, {"lowcut",90},
                {"mix",1.0f}, {"output",0.0f} } },

            // Clean/bright presets get a lower low-cut (more body retained) and
            // "amp" low-mid warmth so the high-shelf "presence" doesn't thin
            // them out — the main anti-tinny move across this bank.
            { "clean vocal",
              { {"drive",8}, {"mode",1}, {"bias",0.12f}, {"tone",12000}, {"lowcut",70},
                {"mix",0.85f}, {"ampOn",1}, {"presenceOn",1}, {"output",-1.0f} } },

            { "warm tape",
              { {"drive",12}, {"mode",1}, {"bias",0.22f}, {"tone",7500}, {"lowcut",75},
                {"mix",0.85f}, {"ampOn",1},
                {"delayOn",1}, {"delayTime",300}, {"delayFeedback",0.26f}, {"delayMix",0.16f},
                {"output",-1.5f} } },

            { "gritty lead",
              { {"drive",20}, {"mode",2}, {"bias",0.32f}, {"tone",9000}, {"lowcut",80},
                {"stereoWidth",1.1f}, {"mix",0.95f}, {"ampOn",1}, {"presenceOn",1},
                {"reverbOn",1}, {"reverbSize",0.35f}, {"reverbMix",0.12f},
                {"output",-4.0f} } },

            // Heavy band-limited presets keep "amp" body and drop the late
            // presence shelf so the fuzz stays thick rather than fizzy.
            { "blown out",
              { {"drive",30}, {"mode",3}, {"bias",0.45f}, {"tone",6000}, {"lowcut",85},
                {"mix",0.95f}, {"fuzzOn",1}, {"ampOn",1}, {"speakerOn",1},
                {"output",-7.5f} } },

            { "telephone",
              { {"drive",18}, {"mode",2}, {"bias",0.28f}, {"tone",4500}, {"stereoWidth",0.3f},
                {"formant",0.58f}, {"mix",1.0f}, {"speakerOn",1}, {"presenceOn",1},
                {"output",-3.0f} } },

            { "megaphone",
              { {"drive",24}, {"mode",2}, {"bias",0.42f}, {"tone",4500}, {"stereoWidth",0.4f},
                {"mix",1.0f}, {"fuzzOn",1}, {"ampOn",1}, {"speakerOn",1},
                {"output",-6.5f} } },

            { "deep demon",
              { {"drive",22}, {"mode",3}, {"bias",0.48f}, {"tone",6500}, {"formant",0.14f},
                {"mix",1.0f}, {"fuzzOn",1}, {"ampOn",1},
                {"reverbOn",1}, {"reverbSize",0.4f}, {"reverbDamp",0.5f}, {"reverbMix",0.15f},
                {"output",-5.5f} } },

            { "helium high",
              { {"drive",6}, {"mode",0}, {"bias",0.1f}, {"tone",14000}, {"formant",0.9f},
                {"mix",0.9f}, {"output",-0.5f} } },

            { "bright angel",
              { {"drive",6}, {"mode",0}, {"bias",0.1f}, {"tone",13000}, {"lowcut",75},
                {"stereoWidth",1.3f}, {"formant",0.66f}, {"mix",0.82f}, {"presenceOn",1},
                {"doublerOn",1}, {"doublerDetune",0.4f}, {"doublerWidth",0.9f}, {"doublerMix",0.5f},
                {"reverbOn",1}, {"reverbSize",0.6f}, {"reverbDamp",0.35f}, {"reverbMix",0.26f},
                {"output",-1.0f} } },

            { "wide double",
              { {"drive",8}, {"mode",0}, {"bias",0.12f}, {"tone",12000}, {"lowcut",75},
                {"stereoWidth",1.3f}, {"mix",0.8f}, {"ampOn",1}, {"presenceOn",1},
                {"doublerOn",1}, {"doublerDetune",0.45f}, {"doublerWidth",0.95f}, {"doublerMix",0.55f},
                {"reverbOn",1}, {"reverbSize",0.4f}, {"reverbMix",0.16f},
                {"output",-1.0f} } },

            { "slapback",
              { {"drive",12}, {"mode",1}, {"bias",0.2f}, {"tone",11000}, {"lowcut",75},
                {"mix",0.88f}, {"ampOn",1},
                {"delayOn",1}, {"delayTime",130}, {"delayFeedback",0.18f}, {"delayMix",0.22f},
                {"output",-1.0f} } },

            { "trap adlib",
              { {"drive",14}, {"mode",2}, {"bias",0.3f}, {"tone",9500}, {"lowcut",80},
                {"stereoWidth",1.2f}, {"formant",0.57f}, {"mix",0.85f}, {"presenceOn",1},
                {"doublerOn",1}, {"doublerDetune",0.4f}, {"doublerWidth",0.85f}, {"doublerMix",0.5f},
                {"delayOn",1}, {"delayTime",250}, {"delayFeedback",0.38f}, {"delayMix",0.26f},
                {"reverbOn",1}, {"reverbSize",0.45f}, {"reverbDamp",0.5f}, {"reverbMix",0.18f},
                {"output",-2.5f} } },

            { "ambient wash",
              { {"drive",10}, {"mode",1}, {"bias",0.15f}, {"tone",12000}, {"lowcut",75},
                {"stereoWidth",1.5f}, {"mix",0.6f}, {"ampOn",1}, {"presenceOn",1},
                {"doublerOn",1}, {"doublerDetune",0.5f}, {"doublerWidth",1.0f}, {"doublerMix",0.5f},
                {"delayOn",1}, {"delayTime",450}, {"delayFeedback",0.42f}, {"delayMix",0.28f},
                {"reverbOn",1}, {"reverbSize",0.85f}, {"reverbDamp",0.35f}, {"reverbMix",0.42f},
                {"output",-2.0f} } },

            { "vintage radio",
              { {"drive",26}, {"mode",3}, {"bias",0.55f}, {"tone",5500}, {"stereoWidth",0.6f},
                {"mix",1.0f}, {"fuzzOn",1}, {"ampOn",1}, {"speakerOn",1},
                {"output",-5.5f} } },

            //==================================================================
            // GLITCH presets (rhythmic gate / stutter — tempo synced).
            // Gating drops average loudness, so their output trims sit a touch
            // hotter than the equivalent un-gated presets to stay matched.
            //==================================================================
            { "trance gate",
              { {"drive",8}, {"mode",1}, {"bias",0.15f}, {"tone",12000}, {"lowcut",75},
                {"mix",0.85f}, {"ampOn",1}, {"presenceOn",1},
                {"glitchOn",1}, {"glitchRate",0.6f}, {"glitchDepth",0.9f}, {"glitchMix",1.0f},
                {"reverbOn",1}, {"reverbSize",0.4f}, {"reverbDamp",0.4f}, {"reverbMix",0.16f},
                {"output",0.0f} } },

            { "stutter chop",
              { {"drive",12}, {"mode",2}, {"bias",0.25f}, {"tone",10000}, {"lowcut",80},
                {"stereoWidth",1.1f}, {"mix",0.9f}, {"presenceOn",1},
                {"glitchOn",1}, {"glitchRate",0.6f}, {"glitchDepth",1.0f}, {"glitchMix",1.0f},
                {"delayOn",1}, {"delayTime",250}, {"delayFeedback",0.35f}, {"delayMix",0.22f},
                {"reverbOn",1}, {"reverbSize",0.5f}, {"reverbDamp",0.4f}, {"reverbMix",0.2f},
                {"output",-1.5f} } },

            { "glitch wash",
              { {"drive",10}, {"mode",1}, {"bias",0.15f}, {"tone",12000}, {"lowcut",75},
                {"stereoWidth",1.4f}, {"mix",0.7f}, {"ampOn",1}, {"presenceOn",1},
                {"glitchOn",1}, {"glitchRate",0.8f}, {"glitchDepth",0.95f}, {"glitchMix",0.9f},
                {"doublerOn",1}, {"doublerDetune",0.45f}, {"doublerWidth",0.9f}, {"doublerMix",0.5f},
                {"delayOn",1}, {"delayTime",375}, {"delayFeedback",0.45f}, {"delayMix",0.3f},
                {"reverbOn",1}, {"reverbSize",0.8f}, {"reverbDamp",0.35f}, {"reverbMix",0.4f},
                {"output",-2.0f} } },

            { "robot gate",
              { {"drive",16}, {"mode",2}, {"bias",0.3f}, {"tone",8000}, {"stereoWidth",0.8f},
                {"formant",0.62f}, {"mix",1.0f}, {"presenceOn",1},
                {"glitchOn",1}, {"glitchRate",0.4f}, {"glitchDepth",0.85f}, {"glitchMix",1.0f},
                {"output",-1.5f} } },

            { "half-time chop",
              { {"drive",10}, {"mode",1}, {"bias",0.2f}, {"tone",11000}, {"lowcut",75},
                {"mix",0.9f}, {"ampOn",1},
                {"glitchOn",1}, {"glitchRate",0.2f}, {"glitchDepth",0.8f}, {"glitchMix",1.0f},
                {"delayOn",1}, {"delayTime",300}, {"delayFeedback",0.3f}, {"delayMix",0.2f},
                {"output",-0.5f} } },

            // Saturn-style gentle warm saturation — low drive, tape-like Warm
            // mode, parallel blend, every extra module off (defaults).
            { "Signature",
              { {"drive",6}, {"mode",1}, {"bias",0.15f}, {"tone",9000}, {"lowcut",90},
                {"mix",0.35f}, {"output",0.0f} } },
        };
        return presets;
    }
}

int VocalGritProcessor::getNumPrograms()
{
    return (int) getPresets().size();
}

const juce::String VocalGritProcessor::getProgramName (int index)
{
    const auto& presets = getPresets();
    if (juce::isPositiveAndBelow (index, (int) presets.size()))
        return presets[(size_t) index].name;
    return {};
}

void VocalGritProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;
    currentProgram = index;
    applyProgram (index);
}

void VocalGritProcessor::applyProgram (int index)
{
    const auto& presets = getPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    // Reset everything to its default first so presets are deterministic and
    // don't inherit leftover values from the previously selected preset.
    for (auto* ap : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& [id, value] : presets[(size_t) index].values)
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
}

//==============================================================================
juce::AudioProcessorEditor* VocalGritProcessor::createEditor()
{
    return new VocalGritEditor (*this);
}

void VocalGritProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void VocalGritProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// This creates new instances of the plugin — required entry point.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalGritProcessor();
}
