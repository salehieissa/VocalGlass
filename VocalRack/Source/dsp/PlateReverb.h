#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
#include <cmath>

//==============================================================================
// PlateReverb — VocalVerb's engine, ported verbatim (class renamed so it can't
// collide with juce::Reverb). An algorithmic Dattorro-style plate / FDN reverb
// tuned for vocals. The topology follows Jon Dattorro's 1997 "Effect Design"
// plate:
//
//   in -> predelay -> input bandwidth LP -> 4 series input diffusers
//      -> a figure-of-eight reverb tank (two cross-coupled halves, each with a
//         modulated decay-diffuser, a long delay, an in-loop high-shelf damping
//         filter, a second decay-diffuser and a second delay) -> stereo taps.
//
// The richer control set (vs. juce::dsp::Reverb) is mapped onto the tank:
//   decay      -> per-loop feedback gain (RT60 in seconds)
//   size       -> scales every tank delay length
//   predelay   -> pre-tank delay in ms
//   dampHF/HS  -> in-loop high-shelf (corner = dampHighFreq, gain = dampHighShelf)
//   bassFreq / bassMult -> longer (or shorter) decay below bassFreq
//   attack     -> transient softening / build-up of the tank drive
//   diffEarly  -> input diffuser coefficient
//   diffLate   -> tank (decay) diffuser coefficient
//   modRate/Depth -> chorus modulation of the decay-diffuser delays
//   highCut/lowCut -> output low-pass / high-pass
//   mode/color -> base length set + diffusion / tone tunings
//==============================================================================
class PlateReverb
{
public:
    //==========================================================================
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sr = spec.sampleRate;
        const float scale = (float) (sr / 29761.0); // Dattorro reference rate

        // Predelay: up to 250 ms.
        predelay.prepare ((int) (sr * 0.30) + 4);

        // Input diffusers (reference lengths 142, 107, 379, 277 samples).
        inAp[0].prepare (scaleLen (142, scale, 1.0f));
        inAp[1].prepare (scaleLen (107, scale, 1.0f));
        inAp[2].prepare (scaleLen (379, scale, 1.0f));
        inAp[3].prepare (scaleLen (277, scale, 1.0f));

        // Tank: the *buffers* are padded with headroom so the run-time "size" scale
        // (up to ~1.9 * lengthMul) plus modulation always fit. The *musical* tap
        // lengths and the RT60 loop-time use the nominal length (ref * scale) only.
        // Previously the padded buffer length (ref * scale * headroom) was reused as
        // the tap/loop length, which inflated the loop time ~2.4x, crushed the
        // feedback gain and made the Decay knob barely audible.
        const float headroom = 3.0f;
        tankApA.prepare (scaleLen (672,  scale, headroom));
        tankDelA.prepare (scaleLen (4453, scale, headroom));
        tankAp2A.prepare (scaleLen (1800, scale, headroom));
        tankDel2A.prepare (scaleLen (3720, scale, headroom));

        tankApB.prepare (scaleLen (908,  scale, headroom));
        tankDelB.prepare (scaleLen (4217, scale, headroom));
        tankAp2B.prepare (scaleLen (2656, scale, headroom));
        tankDel2B.prepare (scaleLen (3163, scale, headroom));

        // Nominal (musical) lengths = reference * sample-rate scale, no headroom.
        nomApA  = 672.0f  * scale;  nomDelA  = 4453.0f * scale;
        nomAp2A = 1800.0f * scale;  nomDel2A = 3720.0f * scale;
        nomApB  = 908.0f  * scale;  nomDelB  = 4217.0f * scale;
        nomAp2B = 2656.0f * scale;  nomDel2B = 3163.0f * scale;

        baseScale = scale;
        reset();
    }

    void reset()
    {
        predelay.clear();
        for (auto& a : inAp) a.clear();
        tankApA.clear(); tankDelA.clear(); tankAp2A.clear(); tankDel2A.clear();
        tankApB.clear(); tankDelB.clear(); tankAp2B.clear(); tankDel2B.clear();
        bwState = 0.0f;
        dampA = dampB = 0.0f;
        bassA = bassB = 0.0f;
        attackState[0] = attackState[1] = 0.0f;
        lpOut[0] = lpOut[1] = 0.0f;
        lpOut2[0] = lpOut2[1] = 0.0f;
        hpOut[0] = hpOut[1] = 0.0f;
        hpOut2[0] = hpOut2[1] = 0.0f;
        tiltState[0] = tiltState[1] = 0.0f;
        lfoPhase = 0.0f;
    }

    //==========================================================================
    struct Params
    {
        float mix = 0.3f;        // 0..1
        float predelayMs = 20.0f;
        float decaySec = 4.0f;
        float dampHz = 6000.0f;
        float dampShelfDb = -24.0f;
        float bassHz = 700.0f;
        float bassMult = 1.5f;
        float size = 1.0f;       // 0..1 -> mapped to length scale
        float attack = 0.5f;     // 0..1
        float diffEarly = 1.0f;  // 0..1
        float diffLate = 1.0f;   // 0..1
        float modRate = 2.5f;    // Hz
        float modDepth = 0.4f;   // 0..1
        float highCut = 8000.0f;
        float lowCut = 10.0f;
        int   mode = 0;          // concert hall / plate / room / chamber / ambience
        int   color = 0;         // 1970s / modern / vintage / dark / bright
    };

    void setParams (const Params& p) { pending = p; }

    //==========================================================================
    void process (juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples();
        const int nc = juce::jmin (buffer.getNumChannels(), 2);
        if (n == 0 || nc == 0) return;

        const Params p = pending;
        const ModeTuning mt = tuningFor (p.mode, p.color);

        // ----- derive coefficients -----
        const float sizeScale = juce::jmap (p.size, 0.0f, 1.0f, 0.55f, 1.9f) * mt.lengthMul;

        // tank loop time (s) ~ sum of the long delays of one half, used for RT60.
        // Uses the nominal (un-padded) lengths so Decay maps correctly to RT60.
        const float loopSamples = (nomDelA + nomDel2A + nomDelB + nomDel2B)
                                   * 0.5f * sizeScale;
        const float loopSec = juce::jmax (1.0e-4f, loopSamples / (float) sr);
        const float rt60 = juce::jlimit (0.1f, 20.0f, p.decaySec);
        float decayGain = std::pow (10.0f, -3.0f * loopSec / rt60);
        decayGain = juce::jlimit (0.0f, 0.9990f, decayGain);

        // bass multiplier: lows decay (mult>1 => longer => gain closer to 1).
        float bassDecay = std::pow (decayGain, 1.0f / juce::jlimit (0.2f, 4.0f, p.bassMult));
        bassDecay = juce::jlimit (0.0f, 0.9995f, bassDecay);
        const float bassCoef = onePoleCoef (juce::jlimit (40.0f, 1200.0f, p.bassHz));

        // in-loop high-shelf damping: corner = dampHz, HF gain from shelf dB.
        const float dampCoef = onePoleCoef (juce::jlimit (500.0f, 18000.0f, p.dampHz) * mt.dampMul);
        const float shelfGain = juce::Decibels::decibelsToGain (juce::jlimit (-48.0f, 0.0f, p.dampShelfDb));

        // input bandwidth (fixed-ish, slightly mode dependent).
        const float bwCoef = onePoleCoef (juce::jlimit (1000.0f, 20000.0f, mt.inputBW));

        const float inDiff1 = juce::jlimit (0.0f, 0.92f, 0.72f * p.diffEarly) * mt.inDiffMul;
        const float inDiff2 = juce::jlimit (0.0f, 0.92f, 0.60f * p.diffEarly) * mt.inDiffMul;
        const float decoDiff1 = juce::jlimit (0.0f, 0.92f, 0.70f * p.diffLate) * mt.tankDiffMul;
        const float decoDiff2 = juce::jlimit (0.0f, 0.92f, 0.50f * p.diffLate) * mt.tankDiffMul;

        // attack: 0 => instant, 1 => ~60 ms one-pole softening of tank drive.
        const float attMs = juce::jmap (p.attack, 0.0f, 1.0f, 0.2f, 60.0f);
        const float attCoef = std::exp (-1.0f / (juce::jmax (0.05f, attMs) * 0.001f * (float) sr));

        // output filters
        const float hcCoef = onePoleCoef (juce::jlimit (500.0f, 20000.0f, p.highCut));
        const float lcCoef = onePoleCoef (juce::jlimit (10.0f, 1000.0f, p.lowCut));

        // tilt EQ from "color"
        const float tiltCoef = onePoleCoef (900.0f);

        // predelay in samples
        const int pdSamples = juce::jlimit (1, predelay.maxLen() - 2,
                                            (int) (p.predelayMs * 0.001f * (float) sr));

        // modulation
        const float lfoInc = juce::MathConstants<float>::twoPi * p.modRate / (float) sr;
        const float modAmtA = p.modDepth * 12.0f * baseScale;
        const float modAmtB = p.modDepth * 9.0f  * baseScale;

        // tank delay lengths (nominal length scaled by size; never the padded buffer)
        const float lenApA  = nomApA  * sizeScale;
        const float lenDelA = nomDelA * sizeScale;
        const float lenAp2A = nomAp2A * sizeScale;
        const float lenDel2A= nomDel2A* sizeScale;
        const float lenApB  = nomApB  * sizeScale;
        const float lenDelB = nomDelB * sizeScale;
        const float lenAp2B = nomAp2B * sizeScale;
        const float lenDel2B= nomDel2B* sizeScale;

        const float mix = juce::jlimit (0.0f, 1.0f, p.mix);
        const float wetGain = std::sqrt (mix);
        const float dryGain = std::sqrt (1.0f - mix);

        auto* left  = buffer.getWritePointer (0);
        auto* right = nc > 1 ? buffer.getWritePointer (1) : left;

        for (int i = 0; i < n; ++i)
        {
            const float inL = left[i];
            const float inR = right[i];
            const float dryL = inL, dryR = inR;
            float monoIn = 0.5f * (inL + inR);

            // attack: soften the drive into the tank (build-up control)
            attackState[0] += (monoIn - attackState[0]) * (1.0f - attCoef);
            attackState[0] = flushDenorm (attackState[0]);
            const float drive = monoIn + (attackState[0] - monoIn) * p.attack;

            // predelay
            predelay.push (drive);
            float x = predelay.tap (pdSamples);

            // input bandwidth lowpass
            bwState += (x - bwState) * (1.0f - bwCoef);
            bwState = flushDenorm (bwState);
            x = bwState;

            // input diffusion (4 series allpasses)
            x = inAp[0].process (x, inAp[0].base, inDiff1);
            x = inAp[1].process (x, inAp[1].base, inDiff1);
            x = inAp[2].process (x, inAp[2].base, inDiff2);
            x = inAp[3].process (x, inAp[3].base, inDiff2);

            // LFO for modulated decay diffusers
            const float modA = modAmtA * std::sin (lfoPhase);
            const float modB = modAmtB * std::sin (lfoPhase + 1.7f);
            lfoPhase += lfoInc;
            if (lfoPhase > juce::MathConstants<float>::twoPi)
                lfoPhase -= juce::MathConstants<float>::twoPi;

            // ---- tank: two cross-coupled halves (figure of eight) ----
            // The feedback into each half is the opposite half's final delay
            // output from the previous sample (read before we write this one).
            // Flush the feedback taps so subnormals can't recirculate forever in
            // the tank delay lines once the input falls silent (inaudible, < -300 dB).
            const float fromB = flushDenorm (tankDel2B.tap ((int) lenDel2B));
            const float fromA = flushDenorm (tankDel2A.tap ((int) lenDel2A));

            // ---- Half A ---- (fed by input + band-weighted decay * fromB)
            float aIn = x + bandDecay (fromB, bassA, bassCoef, bassDecay, decayGain);
            float a = tankApA.processLin (aIn, lenApA + modA, -decoDiff1);
            tankDelA.push (a);
            float aDel = tankDelA.tapLin (lenDelA);
            aDel = highShelf (aDel, dampA, dampCoef, shelfGain); // in-loop damping
            float a2 = tankAp2A.process (aDel, (int) lenAp2A, decoDiff2);
            tankDel2A.push (a2);

            // ---- Half B ---- (fed by input + band-weighted decay * fromA)
            float bIn = x + bandDecay (fromA, bassB, bassCoef, bassDecay, decayGain);
            float b = tankApB.processLin (bIn, lenApB + modB, -decoDiff1);
            tankDelB.push (b);
            float bDel = tankDelB.tapLin (lenDelB);
            bDel = highShelf (bDel, dampB, dampCoef, shelfGain);
            float b2 = tankAp2B.process (bDel, (int) lenAp2B, decoDiff2);
            tankDel2B.push (b2);

            // ---- output taps (Dattorro stereo taps, scaled by size) ----
            const float s = sizeScale;
            float yL =  0.6f * tankDelB.tap  ((int) (266.0f  * baseScale * s))
                      + 0.6f * tankDelB.tap  ((int) (2974.0f * baseScale * s))
                      - 0.6f * tankAp2B.tap  ((int) (1913.0f * baseScale * s))
                      + 0.6f * tankDel2B.tap ((int) (1996.0f * baseScale * s))
                      - 0.6f * tankDelA.tap  ((int) (1990.0f * baseScale * s))
                      - 0.6f * tankAp2A.tap  ((int) (187.0f  * baseScale * s))
                      - 0.6f * tankDel2A.tap ((int) (1066.0f * baseScale * s));

            float yR =  0.6f * tankDelA.tap  ((int) (353.0f  * baseScale * s))
                      + 0.6f * tankDelA.tap  ((int) (3627.0f * baseScale * s))
                      - 0.6f * tankAp2A.tap  ((int) (1228.0f * baseScale * s))
                      + 0.6f * tankDel2A.tap ((int) (2673.0f * baseScale * s))
                      - 0.6f * tankDelB.tap  ((int) (2111.0f * baseScale * s))
                      - 0.6f * tankAp2B.tap  ((int) (335.0f  * baseScale * s))
                      - 0.6f * tankDel2B.tap ((int) (121.0f  * baseScale * s));

            // ---- color tilt EQ ----
            if (mt.tilt != 0.0f)
            {
                tiltState[0] += (yL - tiltState[0]) * (1.0f - tiltCoef);
                tiltState[1] += (yR - tiltState[1]) * (1.0f - tiltCoef);
                tiltState[0] = flushDenorm (tiltState[0]);
                tiltState[1] = flushDenorm (tiltState[1]);
                yL = tiltState[0] + (yL - tiltState[0]) * (1.0f + mt.tilt);
                yR = tiltState[1] + (yR - tiltState[1]) * (1.0f + mt.tilt);
            }

            // ---- output high-cut (LP) + low-cut (HP) ----
            // Two cascaded one-poles per filter => 12 dB/oct, so the cut knobs make
            // an obvious, musical difference rather than a barely-there 6 dB/oct tilt.
            const float hcA = 1.0f - hcCoef;
            lpOut[0]  += (yL       - lpOut[0])  * hcA;
            lpOut[1]  += (yR       - lpOut[1])  * hcA;
            lpOut2[0] += (lpOut[0] - lpOut2[0]) * hcA;
            lpOut2[1] += (lpOut[1] - lpOut2[1]) * hcA;
            lpOut[0]  = flushDenorm (lpOut[0]);  lpOut[1]  = flushDenorm (lpOut[1]);
            lpOut2[0] = flushDenorm (lpOut2[0]); lpOut2[1] = flushDenorm (lpOut2[1]);
            yL = lpOut2[0]; yR = lpOut2[1];

            const float lcA = 1.0f - lcCoef;
            hpOut[0]  += (yL       - hpOut[0])  * lcA;
            hpOut[1]  += (yR       - hpOut[1])  * lcA;
            hpOut2[0] += (hpOut[0] - hpOut2[0]) * lcA;
            hpOut2[1] += (hpOut[1] - hpOut2[1]) * lcA;
            hpOut[0]  = flushDenorm (hpOut[0]);  hpOut[1]  = flushDenorm (hpOut[1]);
            hpOut2[0] = flushDenorm (hpOut2[0]); hpOut2[1] = flushDenorm (hpOut2[1]);
            yL -= hpOut2[0]; yR -= hpOut2[1];

            left[i]  = dryL * dryGain + yL * wetGain;
            right[i] = dryR * dryGain + yR * wetGain;
        }
    }

private:
    //==========================================================================
    struct Delay
    {
        std::vector<float> buf;
        int mask = 0, writeIdx = 0, base = 1;

        void prepare (int maxLen)
        {
            base = juce::jmax (1, maxLen);
            const int size = juce::nextPowerOfTwo (maxLen + 8);
            buf.assign ((size_t) size, 0.0f);
            mask = size - 1;
            writeIdx = 0;
        }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); writeIdx = 0; }
        int maxLen() const { return (int) buf.size(); }

        inline void push (float x) { buf[(size_t) writeIdx] = x; writeIdx = (writeIdx + 1) & mask; }

        inline float tap (int d) const
        {
            d = juce::jlimit (0, mask, d);
            return buf[(size_t) ((writeIdx - d) & mask)];
        }
        inline float tapLin (float d) const
        {
            d = juce::jlimit (0.0f, (float) mask - 1.0f, d);
            const int d0 = (int) d;
            const float frac = d - (float) d0;
            const float a = buf[(size_t) ((writeIdx - d0) & mask)];
            const float b = buf[(size_t) ((writeIdx - d0 - 1) & mask)];
            return a + frac * (b - a);
        }
    };

    // Schroeder allpass living on its own delay line (stable for |g| < 1).
    struct Allpass : Delay
    {
        inline float process (float x, int d, float g)
        {
            const float z = tap (d);
            const float in = x + g * z;
            push (in);
            return z - g * in;
        }
        inline float processLin (float x, float d, float g)
        {
            const float z = tapLin (d);
            const float in = x + g * z;
            push (in);
            return z - g * in;
        }
    };

    struct ModeTuning
    {
        float lengthMul = 1.0f;
        float inDiffMul = 1.0f;
        float tankDiffMul = 1.0f;
        float dampMul = 1.0f;
        float inputBW = 12000.0f;
        float tilt = 0.0f;   // >0 brighter, <0 darker
    };

    static int scaleLen (int ref, float scale, float headroom)
    {
        return (int) std::ceil ((float) ref * scale * headroom) + 4;
    }

    float onePoleCoef (float hz) const
    {
        const float c = std::exp (-juce::MathConstants<float>::twoPi * hz / (float) sr);
        return juce::jlimit (0.0f, 0.99999f, c);
    }

    // Anti-denormal flush. When the input goes silent the recirculating tank
    // (delay lines + recursive one-pole states) keeps multiplying its contents
    // by the decay gain, driving values toward subnormal floats. Subnormals are
    // hideously slow on most CPUs and cause sustained spikes on the audio thread
    // even though they are ~ -300 dB and totally inaudible. We zero anything
    // below 1e-15: that is far under the noise floor of 32-bit audio, so it adds
    // no DC offset, thump or tone — it just lets silent tails settle to true 0.
    static inline float flushDenorm (float x) noexcept
    {
        return std::abs (x) < 1.0e-15f ? 0.0f : x;
    }

    // in-loop high-shelf: keep lows, attenuate highs by shelfGain above corner.
    static inline float highShelf (float x, float& lpState, float coef, float shelfGain)
    {
        lpState += (x - lpState) * (1.0f - coef);
        lpState = flushDenorm (lpState);
        const float high = x - lpState;
        return lpState + high * shelfGain;
    }

    // feedback gain with extra/less decay below bassHz (band-dependent gain).
    static inline float bandDecay (float x, float& lpState, float coef,
                                   float lowGain, float highGain)
    {
        lpState += (x - lpState) * (1.0f - coef);
        lpState = flushDenorm (lpState);
        const float low = lpState;
        const float high = x - low;
        return low * lowGain + high * highGain;
    }

    ModeTuning tuningFor (int mode, int color) const
    {
        ModeTuning mt;
        switch (mode)
        {
            case 0: mt.lengthMul = 1.15f; mt.inDiffMul = 1.0f;  mt.tankDiffMul = 1.0f;  mt.dampMul = 1.0f;  mt.inputBW = 11000.0f; break; // concert hall
            case 1: mt.lengthMul = 0.85f; mt.inDiffMul = 1.08f; mt.tankDiffMul = 1.05f; mt.dampMul = 1.25f; mt.inputBW = 15000.0f; break; // plate
            case 2: mt.lengthMul = 0.62f; mt.inDiffMul = 0.92f; mt.tankDiffMul = 0.95f; mt.dampMul = 1.1f;  mt.inputBW = 12000.0f; break; // room
            case 3: mt.lengthMul = 0.80f; mt.inDiffMul = 1.0f;  mt.tankDiffMul = 1.0f;  mt.dampMul = 0.9f;  mt.inputBW = 10000.0f; break; // chamber
            case 4:
            default: mt.lengthMul = 1.35f; mt.inDiffMul = 0.85f; mt.tankDiffMul = 0.9f; mt.dampMul = 1.15f; mt.inputBW = 13000.0f; break; // ambience
        }
        switch (color)
        {
            case 0: mt.tilt = -0.22f; mt.dampMul *= 0.80f; break; // 1970s (dark, rolled off)
            case 1: mt.tilt =  0.05f; mt.dampMul *= 1.10f; break; // modern (neutral/clean)
            case 2: mt.tilt = -0.10f; mt.dampMul *= 0.95f; break; // vintage
            case 3: mt.tilt = -0.35f; mt.dampMul *= 0.70f; break; // dark
            case 4:
            default: mt.tilt = 0.30f; mt.dampMul *= 1.30f; break; // bright
        }
        return mt;
    }

    //==========================================================================
    double sr = 44100.0;
    float baseScale = 1.0f;

    // Nominal (musical) tank lengths in samples = reference * sample-rate scale.
    float nomApA = 1.0f, nomDelA = 1.0f, nomAp2A = 1.0f, nomDel2A = 1.0f;
    float nomApB = 1.0f, nomDelB = 1.0f, nomAp2B = 1.0f, nomDel2B = 1.0f;

    Delay   predelay;
    Allpass inAp[4];

    Allpass tankApA, tankAp2A;
    Delay   tankDelA, tankDel2A;
    Allpass tankApB, tankAp2B;
    Delay   tankDelB, tankDel2B;

    float bwState = 0.0f;
    float dampA = 0.0f, dampB = 0.0f;
    float bassA = 0.0f, bassB = 0.0f;
    std::array<float, 2> attackState { 0.0f, 0.0f };
    std::array<float, 2> lpOut { 0.0f, 0.0f };
    std::array<float, 2> lpOut2 { 0.0f, 0.0f };
    std::array<float, 2> hpOut { 0.0f, 0.0f };
    std::array<float, 2> hpOut2 { 0.0f, 0.0f };
    std::array<float, 2> tiltState { 0.0f, 0.0f };
    float lfoPhase = 0.0f;

    Params pending;
};
