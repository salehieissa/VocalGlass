#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

//==============================================================================
// VocalGeek — a handheld "dose console" for vocals. Five cartridges (themes),
// each a different character chain, one DOSE macro that pushes the whole chain
// harder, plus a live performance section: HIT A = stutter, HIT B = tape stop,
// PRINT = freeze the stutter loop, TAP cycles the stutter/delay division.
//
//   lean   — syrup: pitch drooped, low-passed, drowned in slow reverb
//   smoke  — haze: warm tube saturation, soft top, wide chorus, room wash
//   acid   — trip: deep phaser sweeps into a feedback delay
//   snow   — glitter: bright shelf, crushed sparkle, tight slap
//   geeked — zombie: hard clip, bitcrush, phone-speaker band
//==============================================================================
class GeekEngine
{
public:
    enum Theme { Lean = 0, Smoke, Acid, Snow, Geeked, Overdose };

    struct Params
    {
        int    theme    = Lean;
        float  dose     = 0.35f;   // 0..1 macro
        float  texture  = 0.5f;    // 0..1 (d-pad up/down)
        float  space    = 0.5f;    // 0..1 (d-pad left/right)
        int    rate     = 1;       // 0=1/4 1=1/8 2=1/16 3=1/32
        bool   hitA     = false;   // stutter while held
        bool   hitB     = false;   // tape stop while held
        bool   freeze   = false;   // print: latch the stutter loop
        bool   autoMode = false;   // tempo-synced auto performance
        float  outDb    = 0.0f;
        double bpm      = 120.0;
        double ppq      = -1.0;    // host beat position (<0 = not playing)
    };

    void prepare (double sampleRate, int blockSize, int numChannels)
    {
        sr = sampleRate;
        const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize,
                                            (juce::uint32) juce::jmax (1, numChannels) };
        chorus.prepare (spec);
        phaser.prepare (spec);
        for (auto& f : lowpass)  { f.prepare ({ sampleRate, (juce::uint32) blockSize, 1 }); f.setType (juce::dsp::StateVariableTPTFilterType::lowpass); }
        for (auto& f : bandpass) { f.prepare ({ sampleRate, (juce::uint32) blockSize, 1 }); f.setType (juce::dsp::StateVariableTPTFilterType::bandpass); }
        for (auto& f : shelf)
            f.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 6000.0, 0.7071, 1.0f);

        reverb.setSampleRate (sampleRate);

        const int maxDelay = (int) (sampleRate * 2.0) + blockSize;
        for (auto& d : delay) { d.setMaximumDelayInSamples (maxDelay); d.prepare ({ sampleRate, (juce::uint32) blockSize, 1 }); }
        for (auto& d : pitchDelay) { d.setMaximumDelayInSamples ((int) (sampleRate * 0.12) + blockSize); d.prepare ({ sampleRate, (juce::uint32) blockSize, 1 }); }

        histLen = (int) (sampleRate * 2.0);
        for (auto& h : history) { h.assign ((size_t) histLen, 0.0f); }
        histWrite = 0;
        stutterPos = 0.0; tapeSpeed = 1.0f; tapePos = 0.0;

        lastLoop.setSize (2, histLen, false, true);
        lastLoopLen.store (0);

        compGain = 1.0f;
        freerunPpq = 0.0;

        scopeWrite.store (0);
        scope.fill (0.0f);
        reset();
    }

    void reset()
    {
        chorus.reset(); phaser.reset(); reverb.reset();
        for (auto& f : lowpass) f.reset();
        for (auto& f : bandpass) f.reset();
        for (auto& f : shelf) f.reset();
        for (auto& d : delay) d.reset();
        for (auto& d : pitchDelay) d.reset();
        crushHold = { 0.0f, 0.0f }; crushCount = { 0, 0 };
        pitchPhase = 0.0f;
    }

    void setParams (const Params& newParams) { p = newParams; }

    //==========================================================================
    void process (juce::AudioBuffer<float>& buffer)
    {
        const int n  = buffer.getNumSamples();
        const int ch = juce::jmin (2, buffer.getNumChannels());
        if (n == 0 || ch == 0) return;

        // input level for the rec LED
        float inPeak = 0.0f;
        for (int c = 0; c < ch; ++c)
            inPeak = juce::jmax (inPeak, buffer.getMagnitude (c, 0, n));
        inLevel.store (0.9f * inLevel.load() + 0.1f * inPeak);

        // pre-chain loudness for the compensation stage
        float inMS = 1.0e-9f;
        for (int c = 0; c < ch; ++c)
        {
            const auto* d = buffer.getReadPointer (c);
            for (int i = 0; i < n; ++i) inMS += d[i] * d[i];
        }

        switch (p.theme)
        {
            case Lean:   processLean   (buffer, n, ch); break;
            case Smoke:  processSmoke  (buffer, n, ch); break;
            case Acid:   processAcid   (buffer, n, ch); break;
            case Snow:   processSnow   (buffer, n, ch); break;
            case Geeked: processGeeked (buffer, n, ch); break;
            case Overdose:
                processGeeked (buffer, n, ch);
                processAcid   (buffer, n, ch);
                break;
        }

        // loudness compensation: these sit after finished vocal chains, so the
        // cartridges must change character, not level. Match post-chain RMS to
        // the input RMS with a slewed gain (echo tails make it conservative).
        {
            float outMS = 1.0e-9f;
            for (int c = 0; c < ch; ++c)
            {
                const auto* d = buffer.getReadPointer (c);
                for (int i = 0; i < n; ++i) outMS += d[i] * d[i];
            }
            const float target = juce::jlimit (0.35f, 2.5f, std::sqrt (inMS / outMS));
            compGain += (target - compGain) * 0.12f;
            buffer.applyGain (compGain);
        }

        processPerformance (buffer, n, ch);

        buffer.applyGain (juce::Decibels::decibelsToGain (p.outDb));

        // scope + output level for the pixel screen
        float outPeak = 0.0f;
        for (int c = 0; c < ch; ++c)
            outPeak = juce::jmax (outPeak, buffer.getMagnitude (c, 0, n));
        outLevel.store (0.85f * outLevel.load() + 0.15f * outPeak);

        const int step = juce::jmax (1, n / 4);
        for (int i = 0; i < n; i += step)
        {
            float v = 0.0f;
            for (int c = 0; c < ch; ++c)
                v = juce::jmax (v, std::abs (buffer.getReadPointer (c)[i]));
            const int w = scopeWrite.load();
            scope[(size_t) w] = v;
            scopeWrite.store ((w + 1) % (int) scope.size());
        }
    }

    //==========================================================================
    // UI taps (lock-free)
    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f };
    static constexpr int scopeSize = 96;
    std::array<float, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };
    std::atomic<bool> autoStutterActive { false }, autoBrakeActive { false };

    // Copy of the most recent stutter loop for the "print" drag-export.
    // Returns the number of valid samples (0 = nothing captured yet).
    int readLastLoop (juce::AudioBuffer<float>& dest)
    {
        const juce::SpinLock::ScopedTryLockType tl (loopLock);
        if (! tl.isLocked()) return 0;
        const int len = lastLoopLen.load();
        if (len <= 0) return 0;
        dest.setSize (2, len, false, true);
        for (int c = 0; c < 2; ++c)
            dest.copyFrom (c, 0, lastLoop, c, 0, len);
        return len;
    }

    double sampleRate() const noexcept { return sr; }

private:
    void captureLoop (int div, int ch)
    {
        const juce::SpinLock::ScopedTryLockType tl (loopLock);
        if (! tl.isLocked()) return;                      // UI is reading; skip
        const int len = juce::jmin (div, lastLoop.getNumSamples());
        for (int c = 0; c < 2; ++c)
        {
            auto* d = lastLoop.getWritePointer (c);
            const auto& h = history[(size_t) juce::jmin (c, ch - 1)];
            for (int i = 0; i < len; ++i)
            {
                int idx = stutterStart + i;
                if (idx >= histLen) idx -= histLen;
                d[i] = h[(size_t) idx];
            }
        }
        lastLoopLen.store (len);
    }

    //==========================================================================
    // theme chains
    void processLean (juce::AudioBuffer<float>& b, int n, int ch)
    {
        const float dose = p.dose;

        // pitch droop: modulated dual-tap delay, down up to ~4 semitones
        const float semis = -4.0f * dose;
        const float ratio = std::pow (2.0f, semis / 12.0f);
        const float win   = 0.055f * (float) sr;                 // grain window
        const float inc   = (1.0f - ratio) / win;                // read-head drift per sample
        for (int c = 0; c < ch; ++c)
        {
            auto* d = b.getWritePointer (c);
            float ph = pitchPhase;
            for (int i = 0; i < n; ++i)
            {
                pitchDelay[(size_t) c].pushSample (0, d[i]);
                ph += inc; if (ph >= 1.0f) ph -= 1.0f;
                const float d1 = ph * win;
                const float d2 = std::fmod (ph + 0.5f, 1.0f) * win;
                const float x1 = pitchDelay[(size_t) c].popSample (0, 1.0f + d1, false);
                const float x2 = pitchDelay[(size_t) c].popSample (0, 1.0f + d2, true);
                const float fade = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * ph);
                const float shifted = x1 * (1.0f - fade) + x2 * fade;
                d[i] = d[i] * (1.0f - dose * 0.85f) + shifted * (dose * 0.85f);
            }
            if (c == ch - 1) pitchPhase = ph;
        }

        // syrup lowpass closes with the dose
        const float cutoff = juce::jmap (dose, 16000.0f, 1400.0f);
        singleChannelFilter (lowpass, b, n, ch, cutoff, 0.9f);

        // slow wobble
        chorus.setRate (0.25f + p.texture * 0.8f);
        chorus.setDepth (0.25f + dose * 0.5f);
        chorus.setCentreDelay (12.0f);
        chorus.setFeedback (0.0f);
        chorus.setMix (dose * 0.5f);
        applyContext (chorus, b, ch);

        applyReverb (b, n, ch, 0.92f, 0.6f, juce::jlimit (0.0f, 0.85f, dose * 0.5f + p.space * 0.35f));
    }

    void processSmoke (juce::AudioBuffer<float>& b, int n, int ch)
    {
        const float dose  = p.dose;
        const float drive = juce::Decibels::decibelsToGain (dose * 18.0f);
        for (int c = 0; c < ch; ++c)
        {
            auto* d = b.getWritePointer (c);
            for (int i = 0; i < n; ++i)
                d[i] = std::tanh (d[i] * drive) / std::tanh (juce::jmax (1.0f, drive * 0.6f));
        }

        const float cutoff = juce::jmap (dose, 18000.0f, 4500.0f);
        singleChannelFilter (lowpass, b, n, ch, cutoff, 0.707f);

        chorus.setRate (0.4f);
        chorus.setDepth (0.2f + p.texture * 0.4f);
        chorus.setCentreDelay (18.0f);
        chorus.setFeedback (0.0f);
        chorus.setMix (0.3f);
        applyContext (chorus, b, ch);

        applyReverb (b, n, ch, 0.72f, 0.4f, juce::jlimit (0.0f, 0.6f, dose * 0.25f + p.space * 0.3f));
    }

    void processAcid (juce::AudioBuffer<float>& b, int n, int ch)
    {
        const float dose = p.dose;
        phaser.setRate (0.15f + p.texture * 1.4f);
        phaser.setDepth (0.4f + dose * 0.6f);
        phaser.setCentreFrequency (600.0f + dose * 800.0f);
        phaser.setFeedback (-0.2f - dose * 0.55f);
        phaser.setMix (juce::jmin (1.0f, 0.35f + dose * 0.65f));
        applyContext (phaser, b, ch);

        // feedback delay, division follows the tap rate
        const float delaySec = divisionSeconds() * 0.5f;
        const float dSamp = juce::jlimit (32.0f, (float) histLen - 8.0f, delaySec * (float) sr);
        const float fb    = juce::jlimit (0.0f, 0.72f, 0.15f + p.space * 0.55f);
        const float wet   = dose * 0.45f;
        for (int c = 0; c < ch; ++c)
        {
            auto* d = b.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                const float echo = delay[(size_t) c].popSample (0, dSamp, true);
                delay[(size_t) c].pushSample (0, d[i] + echo * fb);
                d[i] += echo * wet;
            }
        }
    }

    void processSnow (juce::AudioBuffer<float>& b, int n, int ch)
    {
        const float dose = p.dose;

        // bright shelf
        const float gainDb = dose * 9.0f;
        for (int c = 0; c < ch; ++c)
        {
            shelf[(size_t) c].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                sr, 5200.0, 0.7071, juce::Decibels::decibelsToGain (gainDb));
            auto* d = b.getWritePointer (c);
            for (int i = 0; i < n; ++i)
                d[i] = shelf[(size_t) c].processSample (d[i]);
        }

        // crushed sparkle blended in parallel
        const int holdN = 1 + (int) (p.texture * 6.0f);
        const float bits = juce::jmap (dose, 12.0f, 6.0f);
        const float q = std::pow (2.0f, bits);
        const float sparkle = dose * 0.35f;
        for (int c = 0; c < ch; ++c)
        {
            auto* d = b.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                if (++crushCount[(size_t) c] >= holdN)
                {
                    crushCount[(size_t) c] = 0;
                    crushHold[(size_t) c] = std::round (d[i] * q) / q;
                }
                d[i] += (crushHold[(size_t) c] - d[i]) * sparkle;
            }
        }

        // tight slap
        const float dSamp = juce::jlimit (32.0f, (float) histLen - 8.0f, 0.085f * (float) sr);
        const float wet = p.space * 0.4f;
        for (int c = 0; c < ch; ++c)
        {
            auto* d = b.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                const float echo = delay[(size_t) c].popSample (0, dSamp, true);
                delay[(size_t) c].pushSample (0, d[i]);
                d[i] += echo * wet;
            }
        }
    }

    void processGeeked (juce::AudioBuffer<float>& b, int n, int ch)
    {
        const float dose  = p.dose;
        const float drive = juce::Decibels::decibelsToGain (6.0f + dose * 22.0f);

        // hard clip + bitcrush
        const int holdN = 1 + (int) (dose * p.texture * 14.0f);
        const float bits = juce::jmap (dose, 12.0f, 4.0f);
        const float q = std::pow (2.0f, bits);
        for (int c = 0; c < ch; ++c)
        {
            auto* d = b.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                float x = juce::jlimit (-1.0f, 1.0f, d[i] * drive) * 0.7f;
                if (++crushCount[(size_t) c] >= holdN)
                {
                    crushCount[(size_t) c] = 0;
                    crushHold[(size_t) c] = std::round (x * q) / q;
                }
                d[i] = crushHold[(size_t) c];
            }
        }

        // phone-speaker band tightens with the dose
        const float centre = 1400.0f;
        const float blend = 0.35f + dose * 0.65f;
        for (int c = 0; c < ch; ++c)
        {
            bandpass[(size_t) c].setCutoffFrequency (centre);
            bandpass[(size_t) c].setResonance (0.55f + dose * 0.35f);
            auto* d = b.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                const float bp = bandpass[(size_t) c].processSample (0, d[i]);
                d[i] = d[i] * (1.0f - blend) + bp * blend * 1.6f;
            }
        }
    }

    //==========================================================================
    // performance: history buffer feeds stutter (hit a / print) + tape stop (hit b)
    void processPerformance (juce::AudioBuffer<float>& b, int n, int ch)
    {
        // ---- auto pilot: a tempo-synced pattern brain. Every 2-bar phrase a
        // deterministic hash picks a move for the phrase ending — a straight
        // stutter, an accelerating ramp stutter (1/8 -> 1/32), a gated double
        // chop, or a brake into the downbeat. Window length grows with the
        // dose, so higher tolerance = wilder autopilot.
        bool autoStut = false, autoBrake = false;
        int rateBoost = 0;
        if (p.autoMode)
        {
            double ppq = p.ppq;
            if (ppq < 0.0)
            {
                freerunPpq += (p.bpm / 60.0) * (double) n / sr;
                ppq = freerunPpq;
            }
            constexpr double phraseLen = 8.0;                    // 2 bars of 4/4
            const auto phrase = (juce::int64) std::floor (ppq / phraseLen);
            juce::uint32 h = (juce::uint32) phrase * 2654435761u;
            h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
            const int pat = (int) (h % 4u);

            const double pos = std::fmod (ppq, phraseLen);
            const double win = pat == 3 ? 1.5 : 0.5 + (double) p.dose * 1.5;
            if (pos >= phraseLen - win)
            {
                const double t = (pos - (phraseLen - win)) / win;   // 0..1
                switch (pat)
                {
                    case 0: autoStut = true; break;                            // straight
                    case 1: autoStut = true; rateBoost = (int) (t * 3.0); break; // ramp
                    case 2: autoStut = std::fmod (pos * 4.0, 1.0) < 0.55;      // gated
                            rateBoost = 1; break;
                    default: autoBrake = true; break;                          // brakes
                }
            }
        }

        const bool stutter = p.hitA || p.freeze || autoStut;
        const bool tape    = p.hitB || autoBrake;
        autoStutterActive.store (autoStut);
        autoBrakeActive.store (autoBrake);

        const int div = juce::jlimit (64, histLen - 4,
                                      (int) (divisionSeconds() * sr) >> rateBoost);

        for (int i = 0; i < n; ++i)
        {
            // always record history
            for (int c = 0; c < ch; ++c)
                history[(size_t) c][(size_t) histWrite] = b.getReadPointer (c)[i];

            if (stutter)
            {
                if (! wasStutter)         // capture loop start
                {
                    stutterStart = histWrite - div; if (stutterStart < 0) stutterStart += histLen;
                    stutterPos = 0.0;
                    wasStutter = true;
                    captureLoop (div, ch);
                }
                int idx = stutterStart + (int) stutterPos;
                if (idx >= histLen) idx -= histLen;
                for (int c = 0; c < ch; ++c)
                    b.getWritePointer (c)[i] = history[(size_t) c][(size_t) idx];
                stutterPos += 1.0;
                if (stutterPos >= (double) div) stutterPos = 0.0;
            }
            else
                wasStutter = false;

            if (tape)
            {
                if (! wasTape) { tapeSpeed = 1.0f; tapePos = 0.0; wasTape = true; }
                tapeSpeed = juce::jmax (0.0f, tapeSpeed - 1.0f / (0.7f * (float) sr));
                tapePos += (1.0 - (double) tapeSpeed);   // lag behind real time
                double lag = tapePos;
                if (lag > (double) histLen - 4.0) lag = (double) histLen - 4.0;
                int idx = histWrite - (int) lag;
                while (idx < 0) idx += histLen;
                for (int c = 0; c < ch; ++c)
                    b.getWritePointer (c)[i] = history[(size_t) c][(size_t) idx] * tapeSpeed;
            }
            else
                wasTape = false;

            if (++histWrite >= histLen) histWrite = 0;
        }
    }

    //==========================================================================
    float divisionSeconds() const
    {
        const double beat = 60.0 / juce::jlimit (40.0, 240.0, p.bpm);
        switch (p.rate)
        {
            case 0:  return (float) beat;          // 1/4
            case 1:  return (float) (beat * 0.5);  // 1/8
            case 2:  return (float) (beat * 0.25); // 1/16
            default: return (float) (beat * 0.125);// 1/32
        }
    }

    template <typename FilterArray>
    void singleChannelFilter (FilterArray& filters, juce::AudioBuffer<float>& b,
                              int n, int ch, float cutoff, float res)
    {
        for (int c = 0; c < ch; ++c)
        {
            filters[(size_t) c].setCutoffFrequency (cutoff);
            filters[(size_t) c].setResonance (res);
            auto* d = b.getWritePointer (c);
            for (int i = 0; i < n; ++i)
                d[i] = filters[(size_t) c].processSample (0, d[i]);
        }
    }

    template <typename Proc>
    void applyContext (Proc& proc, juce::AudioBuffer<float>& b, int ch)
    {
        juce::dsp::AudioBlock<float> block (b.getArrayOfWritePointers(), (size_t) ch,
                                            (size_t) b.getNumSamples());
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        proc.process (ctx);
    }

    void applyReverb (juce::AudioBuffer<float>& b, int n, int ch,
                      float room, float damp, float wet)
    {
        if (wet <= 0.001f) return;
        juce::Reverb::Parameters rp;
        rp.roomSize = room; rp.damping = damp;
        rp.wetLevel = wet;  rp.dryLevel = 1.0f - wet * 0.4f;
        rp.width = 1.0f;
        reverb.setParameters (rp);
        if (ch >= 2)
            reverb.processStereo (b.getWritePointer (0), b.getWritePointer (1), n);
        else
            reverb.processMono (b.getWritePointer (0), n);
    }

    //==========================================================================
    Params p;
    double sr = 44100.0;

    juce::dsp::Chorus<float> chorus;
    juce::dsp::Phaser<float> phaser;
    juce::Reverb reverb;
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> lowpass, bandpass;
    std::array<juce::dsp::IIR::Filter<float>, 2> shelf;
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>, 2> delay, pitchDelay;

    std::array<float, 2> crushHold {};
    std::array<int, 2>   crushCount {};
    float pitchPhase = 0.0f;

    std::array<std::vector<float>, 2> history;
    int histLen = 0, histWrite = 0, stutterStart = 0;
    double stutterPos = 0.0, tapePos = 0.0;
    float tapeSpeed = 1.0f;
    bool wasStutter = false, wasTape = false;

    float compGain = 1.0f;
    double freerunPpq = 0.0;

    juce::SpinLock loopLock;
    juce::AudioBuffer<float> lastLoop;
    std::atomic<int> lastLoopLen { 0 };
};
