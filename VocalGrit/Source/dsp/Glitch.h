#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <cstdint>
#include <cmath>

//==============================================================================
// Glitch — a tempo-synced rhythmic gate with retrigger rolls and beat-repeat
// stutters.
//
//   • GATE   : a 16-step pattern chops the signal on/off. "depth" sets how far
//              the off-steps duck (0 = open, 1 = full silence), with a short
//              click-free ramp on every edge.
//   • ROLLS  : on some steps the gate retriggers 2x/4x faster (a stutter roll).
//   • REPEAT : on some steps the previous grain is frozen and looped (beat
//              repeat), with micro-fades so it never clicks.
//
// All glitch density scales with "depth", so the same knob takes you from a
// gentle groove gate to full-on mangled chaos. Everything is gain/most-buffer
// based and self-limiting, so it can't run away or blow up the level.
//==============================================================================
class Glitch
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sr      = spec.sampleRate;
        ringLen = (int) (sr * 1.0) + 8;             // 1 s of history
        for (auto& b : ring)  b.assign ((size_t) ringLen, 0.0f);
        for (auto& b : grain) b.assign ((size_t) ringLen, 0.0f);
        rampK = 1.0f - std::exp (-1.0f / (0.003f * (float) sr)); // ~3 ms gate edges
        reset();
    }

    void reset()
    {
        writePos = 0; phase = 0.0; stepIndex = 0; gateGain = 1.0f;
        replaying = false; replayPos = 0; replayLeft = 0; grainLen = 1;
        computeStep();
    }

    // stepLenSamples: length of one 16th-style step; depth/mix: 0..1
    void setParams (double stepLenSamples, float depth, float mix) noexcept
    {
        stepLen  = juce::jmax (32.0, stepLenSamples);
        depthAmt = juce::jlimit (0.0f, 1.0f, depth);
        mixAmt   = juce::jlimit (0.0f, 1.0f, mix);
    }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int numCh = juce::jmin (2, buffer.getNumChannels());
        const int n      = buffer.getNumSamples();
        float* ch0 = buffer.getWritePointer (0);
        float* ch1 = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

        for (int i = 0; i < n; ++i)
        {
            const float inL = ch0[i];
            const float inR = ch1 ? ch1[i] : inL;

            ring[0][(size_t) writePos] = inL;
            ring[1][(size_t) writePos] = inR;

            if (phase >= stepLen) { phase -= stepLen; ++stepIndex; computeStep(); }

            // ---- gate target (with optional retrigger roll) ----
            float target = currentOn ? 1.0f : (1.0f - depthAmt);
            if (rollDiv > 1)
            {
                const double rp = (phase / stepLen) * (double) rollDiv;
                const bool rollOn = std::fmod (rp, 1.0) < 0.5;
                target = rollOn ? 1.0f : (1.0f - depthAmt);
            }
            gateGain += (target - gateGain) * rampK;

            // ---- source: live or frozen grain (beat repeat) ----
            float sL = inL, sR = inR;
            if (replaying)
            {
                const int gi = replayPos % grainLen;
                const float env = grainEnv (gi);
                sL = grain[0][(size_t) gi] * env;
                sR = grain[1][(size_t) gi] * env;
                ++replayPos;
                if (--replayLeft <= 0) replaying = false;
            }

            const float wetL = sL * gateGain;
            const float wetR = sR * gateGain;
            ch0[i] = inL * (1.0f - mixAmt) + wetL * mixAmt;
            if (ch1) ch1[i] = inR * (1.0f - mixAmt) + wetR * mixAmt;

            writePos = (writePos + 1) % ringLen;
            phase += 1.0;
        }
    }

private:
    float grainEnv (int gi) const noexcept
    {
        const int fade = juce::jmin (grainLen / 4, (int) (0.003 * sr) + 1);
        if (fade <= 0) return 1.0f;
        if (gi < fade)              return (float) gi / (float) fade;
        if (gi > grainLen - fade)   return (float) (grainLen - gi) / (float) fade;
        return 1.0f;
    }

    void captureGrain (int len) noexcept
    {
        for (int j = 0; j < len; ++j)
        {
            int idx = (writePos - len + j) % ringLen;
            if (idx < 0) idx += ringLen;
            grain[0][(size_t) j] = ring[0][(size_t) idx];
            grain[1][(size_t) j] = ring[1][(size_t) idx];
        }
    }

    void computeStep() noexcept
    {
        const int s = stepIndex % 16;
        currentOn = pattern[(size_t) s] != 0;
        rollDiv = 0;

        // Deterministic, on-grid glitch placement so the gate is fully timed and
        // repeatable (same input -> same chop, locked to the beat). Density rises
        // with "depth": none below ~0.25 so the gate stays clean/musical, then a
        // 2x stutter roll, a faster 4x roll when pushed, and a single beat-repeat
        // freeze near the top of the bar at high depth.
        if (depthAmt > 0.25f && (s == 6 || s == 14))
            rollDiv = 2;
        if (depthAmt > 0.55f && (s == 7 || s == 15))
            rollDiv = 4;

        if (depthAmt > 0.70f && s == 10 && ! replaying)
        {
            grainLen   = juce::jlimit (64, ringLen - 1, (int) (stepLen * 0.5));
            captureGrain (grainLen);                          // beat repeat
            replaying  = true;
            replayPos  = 0;
            replayLeft = (int) stepLen;
        }
    }

    double sr = 44100.0;
    int ringLen = 44108;
    std::array<std::vector<float>, 2> ring, grain;
    int writePos = 0;

    double phase = 0.0, stepLen = 11025.0;
    int stepIndex = 0;
    float gateGain = 1.0f, depthAmt = 0.0f, mixAmt = 1.0f, rampK = 0.01f;
    bool currentOn = true;
    int rollDiv = 0;

    bool replaying = false;
    int replayPos = 0, replayLeft = 0, grainLen = 1;

    // A groovy 16-step gate pattern (1 = open, 0 = ducked).
    static constexpr int pattern[16] = { 1,1,0,1, 1,0,1,0, 1,1,0,1, 1,0,1,1 };
};
