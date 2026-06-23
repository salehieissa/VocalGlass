#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

//==============================================================================
// PitchCorrector — a real-time, low-latency autotune engine.
//
//   * DETECTION: YIN (cumulative-mean-normalised difference function) on a
//     sliding mono-sum window, run once per hop. Restricting the tau search to
//     the active vocal range both speeds it up and rejects octave errors.
//   * SNAP: the detected f0 is mapped to the nearest ENABLED scale note
//     (12 pitch-class toggles), with A4 taken from the detune (reference Hz)
//     control. flex tune sets the capture range (0 = snap everything fully /
//     hard, 100 = only correct notes already close / natural); humanize re-
//     introduces a fraction of the natural vibrato.
//   * SHIFT: a clean, time-domain PSOLA-style shifter. A single fractional read
//     pointer scans the per-channel delay line at the smoothed correction ratio,
//     so the output is *exactly* repitched (continuous resampling — no average
//     pitch error). Whenever the read/write gap drifts out of a safe band the
//     pointer is re-seated by an integer number of detected pitch periods, so it
//     lands on phase-coherent material; a short raised-cosine crossfade hides the
//     seam. Because re-seats are pitch-synchronous there is no metallic comb, and
//     at unity ratio the pointer never moves so the unity case is bit-transparent
//     (aside from the fixed reporting latency).
//
// Everything is pre-allocated in prepare(); process() never allocates.
//==============================================================================
class PitchCorrector
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kDelayLen    = 1 << 14;   // 16384 samples per channel
    static constexpr int kMaxWindow   = 2048;      // HQ analysis window
    static constexpr int kHop         = 256;       // detection hop
    // Two latency profiles, selected by the Low Latency / HQ mode switch:
    //   HQ           -> 512 samples (~11.6 ms @44.1k): widest re-seat band, cleanest
    //                   on the lowest voices.
    //   Low Latency  -> 256 samples (~5.8 ms  @44.1k): half the delay for tracking /
    //                   live monitoring; tuned for typical (mid/high) vocal ranges.
    static constexpr int kLatencyHQ  = 512;
    static constexpr int kLatencyLow = 256;

    void prepare (double sr, int /*blockSize*/, int numCh)
    {
        sampleRate = sr;
        numChannels = juce::jlimit (1, kMaxChannels, numCh);

        for (auto& d : delay)
        {
            d.assign ((size_t) kDelayLen, 0.0f);
        }
        writePos = 0;

        analysis.assign ((size_t) kMaxWindow, 0.0f);
        anWrite = 0;

        yinBuffer.assign ((size_t) (kMaxWindow / 2 + 1), 0.0f);
        scratch.assign ((size_t) kMaxWindow, 0.0f);

        smoothedRatio = 1.0f;
        ratioTarget = 1.0f;
        lpCents = 0.0f;
        hopCounter = 0;

        configureForMode();   // sets window + latencySamp from the current mode

        readDelaySamp  = (float) latencySamp;
        xfadeDelaySamp = (float) latencySamp;
        xfadeRemain = 0;
        xfadeLen = 1;
        periodSamp = (float) latencySamp;
    }

    // Called per block on the audio thread with the current parameter values.
    void setParams (const std::array<bool, 12>& enabledNotes,
                    float retuneSpeed01, float humanize01, float flex01,
                    bool modern, bool hq, float refHz,
                    float minFreqHz, float maxFreqHz, bool powered)
    {
        notesEnabled = enabledNotes;
        retune       = juce::jlimit (0.0f, 1.0f, retuneSpeed01);
        humanize     = juce::jlimit (0.0f, 1.0f, humanize01);
        flex         = juce::jlimit (0.0f, 1.0f, flex01);
        modernMode   = modern;
        a4Hz         = refHz;
        rangeMin     = minFreqHz;
        rangeMax     = maxFreqHz;
        powerOn      = powered;

        if (hq != hqMode)
        {
            hqMode = hq;
            configureForMode();
        }

        // Glide coefficient: retune 0 -> instant (hard snap), 100 -> slow & natural.
        // The square law biases the lower half toward firm, audible correction so
        // even moderate settings sound like real tuning. Classic (non-modern)
        // snaps far faster for a robotic feel.
        float glideMs = retune * retune * 240.0f;
        if (! modernMode)
            glideMs *= 0.12f;
        glideAlpha = (glideMs <= 0.5f) ? 1.0f
                   : 1.0f - std::exp (-1.0f / (glideMs * 0.001f * (float) sampleRate));
    }

    // Depends on the Low Latency / HQ mode. The processor reads this each block
    // and re-notifies the host (via setLatencySamples) only when it changes.
    int  getLatencySamples() const { return latencySamp; }
    int  getDetectedNote()   const { return detectedNote.load(); }
    float getDetectedCents() const { return detectedCents.load(); }
    bool  hasPitch()         const { return pitchPresent.load(); }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int n  = buffer.getNumSamples();
        const int ch = juce::jmin (numChannels, buffer.getNumChannels());

        if (! powerOn)
            return; // pass through untouched when powered off

        float* data[kMaxChannels] = { nullptr, nullptr };
        for (int c = 0; c < ch; ++c)
            data[c] = buffer.getWritePointer (c);

        // Safe band for the read/write gap. The pointer is re-seated by whole
        // pitch periods whenever it strays outside this, which keeps it phase
        // coherent and well clear of the (future) write head and the buffer edge.
        //
        // The band is centred on latencySamp so the engine's *true* steady-state
        // delay equals what we report to the host. Width is 1.5 * latencySamp, so
        // re-seats stay pitch-synchronous for any period up to that span; the
        // period is clamped to the band below so even longer periods can't thrash.
        const float rdLo = (float) latencySamp * 0.25f;   // centre = latencySamp
        const float rdHi = (float) latencySamp * 1.75f;

        for (int i = 0; i < n; ++i)
        {
            // ---- mono sum for analysis ----
            float mono = 0.0f;
            for (int c = 0; c < ch; ++c)
                mono += data[c][i];
            mono /= (float) juce::jmax (1, ch);

            analysis[(size_t) anWrite] = mono;
            anWrite = (anWrite + 1) % window;

            // ---- write current input into delay lines ----
            for (int c = 0; c < ch; ++c)
                delay[(size_t) c][(size_t) writePos] = data[c][i];

            // ---- detection on hop boundaries ----
            if (++hopCounter >= kHop)
            {
                hopCounter = 0;
                runDetection();
                updateTargets();
            }

            // ---- per-sample ratio smoothing ----
            smoothedRatio += (ratioTarget - smoothedRatio) * glideAlpha;

            // ---- advance the read pointer (gap drifts at 1 - ratio / sample) ----
            const float drift = 1.0f - smoothedRatio;
            readDelaySamp += drift;

            // Re-seat by integer periods when outside the band (only when not
            // already crossfading, so seams never overlap).
            if (xfadeRemain == 0)
            {
                const float period = juce::jlimit (32.0f, rdHi - rdLo, periodSamp);
                if (readDelaySamp < rdLo || readDelaySamp > rdHi)
                {
                    const float centre = 0.5f * (rdLo + rdHi);
                    const int   k = juce::jmax (1, (int) std::ceil (std::abs (centre - readDelaySamp) / period));
                    xfadeDelaySamp = readDelaySamp;                 // old tap
                    readDelaySamp += (readDelaySamp < rdLo ? 1.0f : -1.0f) * (float) k * period;
                    xfadeLen    = juce::jlimit (32, 256, (int) (period * 0.5f));
                    xfadeRemain = xfadeLen;
                }
            }

            readDelaySamp  = juce::jmax (2.0f, readDelaySamp);
            xfadeDelaySamp = juce::jmax (2.0f, xfadeDelaySamp);

            float g = 1.0f; // weight of the new tap (1 = fully seated)
            if (xfadeRemain > 0)
            {
                const float lin = 1.0f - (float) xfadeRemain / (float) xfadeLen;
                g = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * lin);
            }

            for (int c = 0; c < ch; ++c)
            {
                float s = readDelay (c, readDelaySamp);
                if (xfadeRemain > 0)
                {
                    const float sOld = readDelay (c, xfadeDelaySamp);
                    s = sOld + g * (s - sOld);
                }
                data[c][i] = s;
            }

            if (xfadeRemain > 0)
            {
                xfadeDelaySamp += drift;   // old tap keeps drifting until faded out
                --xfadeRemain;
            }

            writePos = (writePos + 1) & (kDelayLen - 1);
        }
    }

private:
    //==========================================================================
    void configureForMode()
    {
        // HQ widens the analysis window (steadier detection) AND uses the longer,
        // cleanest re-seat band. Low Latency halves the delay for live use.
        window      = hqMode ? 2048 : 1024;
        latencySamp = hqMode ? kLatencyHQ : kLatencyLow;
        anWrite %= window;
    }

    float readDelay (int c, float delaySamples) const
    {
        float readPos = (float) writePos - delaySamples;
        while (readPos < 0.0f) readPos += (float) kDelayLen;
        const int i0 = (int) readPos;
        const float frac = readPos - (float) i0;
        const int i1 = (i0 + 1) & (kDelayLen - 1);
        const auto& d = delay[(size_t) c];
        return d[(size_t) (i0 & (kDelayLen - 1))] * (1.0f - frac) + d[(size_t) i1] * frac;
    }

    //==========================================================================
    // YIN pitch detection over the most-recent `window` samples.
    void runDetection()
    {
        // Linearise the circular analysis buffer into scratch (oldest -> newest)
        for (int i = 0; i < window; ++i)
            scratch[(size_t) i] = analysis[(size_t) ((anWrite + i) % window)];

        // Skip if the window is essentially silent.
        float rms = 0.0f;
        for (int i = 0; i < window; ++i) rms += scratch[(size_t) i] * scratch[(size_t) i];
        rms = std::sqrt (rms / (float) window);
        if (rms < 1.0e-4f) { lastF0 = 0.0f; lastConfident = false; return; }

        const int integ  = window / 2;                 // integration length
        // Always allow detection down to a generous low floor (~65 Hz, limited by
        // the analysis window) so a low/male voice still tracks even if the user
        // leaves Vocal Range on a higher preset. The range's main job is to set the
        // ceiling (reject high-harmonic octave errors).
        const double detMin = juce::jmin ((double) rangeMin, 65.0);
        int tauMin = juce::jmax (2,    (int) (sampleRate / (double) rangeMax));
        int tauMax = juce::jmin (integ, (int) (sampleRate / detMin));
        if (tauMax <= tauMin + 1) { lastF0 = 0.0f; lastConfident = false; return; }

        // Difference function d(tau)
        yinBuffer[0] = 1.0f;
        double runningSum = 0.0;
        float bestVal = 1.0f;
        int   bestTau = -1;
        const float threshold = 0.15f;

        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float sum = 0.0f;
            for (int j = 0; j < integ; ++j)
            {
                const float diff = scratch[(size_t) j] - scratch[(size_t) (j + tau)];
                sum += diff * diff;
            }
            runningSum += (double) sum;
            const float cmnd = (runningSum > 0.0) ? (float) (sum * tau / runningSum) : 1.0f;
            yinBuffer[(size_t) tau] = cmnd;

            if (tau >= tauMin)
            {
                // First dip below threshold that is a local minimum.
                if (cmnd < threshold && tau > tauMin
                    && yinBuffer[(size_t) (tau - 1)] < cmnd)
                {
                    bestTau = tau - 1;
                    bestVal = yinBuffer[(size_t) (tau - 1)];
                    break;
                }
                if (cmnd < bestVal) { bestVal = cmnd; bestTau = tau; }
            }
        }

        if (bestTau < tauMin || bestVal > 0.45f)
        {
            lastF0 = 0.0f; lastConfident = false; return;
        }

        // Parabolic interpolation around the minimum for sub-sample accuracy.
        float betterTau = (float) bestTau;
        if (bestTau > tauMin && bestTau < tauMax)
        {
            const float s0 = yinBuffer[(size_t) (bestTau - 1)];
            const float s1 = yinBuffer[(size_t) bestTau];
            const float s2 = yinBuffer[(size_t) (bestTau + 1)];
            const float denom = (2.0f * (2.0f * s1 - s2 - s0));
            if (std::abs (denom) > 1.0e-9f)
                betterTau = (float) bestTau + (s2 - s0) / denom;
        }

        lastF0 = (float) (sampleRate / (double) betterTau);
        lastConfident = (lastF0 >= (float) detMin && lastF0 <= rangeMax);
    }

    //==========================================================================
    // Map detected f0 -> nearest enabled note and compute the correction ratio.
    void updateTargets()
    {
        bool anyEnabled = false;
        for (bool b : notesEnabled) anyEnabled |= b;

        if (! lastConfident || lastF0 <= 0.0f || ! anyEnabled)
        {
            ratioTarget = 1.0f;
            detectedNote.store (-1);
            detectedCents.store (0.0f);
            pitchPresent.store (false);
            return;
        }

        // Keep the re-seat hop size locked to the current pitch period.
        periodSamp = (float) (sampleRate / (double) lastF0);

        const double noteFloat = 69.0 + 12.0 * std::log2 (lastF0 / (double) a4Hz);
        const int    base = (int) std::lround (noteFloat);

        int    bestMidi = -1;
        double bestDist = 1.0e9;
        for (int m = base - 7; m <= base + 7; ++m)
        {
            const int pc = ((m % 12) + 12) % 12; // 0 = C
            if (! notesEnabled[(size_t) pc]) continue;
            const double dist = std::abs (noteFloat - (double) m);
            if (dist < bestDist) { bestDist = dist; bestMidi = m; }
        }

        if (bestMidi < 0)
        {
            ratioTarget = 1.0f;
            detectedNote.store (-1); detectedCents.store (0.0f);
            pitchPresent.store (false);
            return;
        }

        const double targetF0 = (double) a4Hz * std::pow (2.0, (bestMidi - 69) / 12.0);
        const float centsToTarget = (float) (1200.0 * std::log2 (targetF0 / lastF0));

        // Humanize: keep a fraction of the fast (vibrato) part of the deviation.
        lpCents += (centsToTarget - lpCents) * 0.05f;
        const float vibrato = centsToTarget - lpCents;

        // Flex tune: 0 => snap EVERY detected note all the way to pitch (hard /
        // T-Pain); 100 => only correct notes that are already close, leaving wide
        // expressive gestures alone (natural). Crucially, inside the capture range
        // the pull is always FULL strength, so a note is never left half-corrected
        // the way a linear weight would. A short rolloff past the edge avoids clicks.
        const float absDev       = std::abs (centsToTarget);
        const float fInv         = 1.0f - flex;
        const float captureCents = 25.0f + fInv * fInv * 1175.0f;   // flex 0 -> 1200c, flex 1 -> 25c
        const float strength = (absDev <= captureCents)
                             ? 1.0f
                             : juce::jlimit (0.0f, 1.0f, 1.0f - (absDev - captureCents) / 150.0f);

        const float appliedCents = centsToTarget * strength - humanize * vibrato;
        float ratio = std::pow (2.0f, appliedCents / 1200.0f);
        ratioTarget = juce::jlimit (0.5f, 2.0f, ratio);

        detectedNote.store (((bestMidi % 12) + 12) % 12);
        // Meter shows how far the *input* sits from the snapped note.
        detectedCents.store (juce::jlimit (-100.0f, 100.0f, -centsToTarget));
        pitchPresent.store (true);
    }

    //==========================================================================
    double sampleRate = 44100.0;
    int    numChannels = 2;

    std::array<std::vector<float>, kMaxChannels> delay;
    int    writePos = 0;

    std::vector<float> analysis, yinBuffer, scratch;
    int    anWrite = 0;

    int    window = 1024;
    int    latencySamp = kLatencyLow;
    bool   hqMode = false;

    // params
    std::array<bool, 12> notesEnabled { { true, false, true, false, true, true,
                                           false, true, false, true, false, true } };
    float retune = 0.2f, humanize = 0.35f, flex = 0.35f;
    bool  modernMode = true;
    float a4Hz = 440.0f, rangeMin = 80.0f, rangeMax = 1100.0f;
    bool  powerOn = true;

    // running state
    float smoothedRatio = 1.0f, ratioTarget = 1.0f;
    float glideAlpha = 1.0f;
    float lpCents = 0.0f;
    int   hopCounter = 0;

    // PSOLA read pointer (gap behind the write head, in samples) + re-seat crossfade
    float readDelaySamp = (float) kLatencyLow;
    float xfadeDelaySamp = (float) kLatencyLow;
    int   xfadeRemain = 0, xfadeLen = 1;
    float periodSamp = (float) kLatencyLow;

    float lastF0 = 0.0f;
    bool  lastConfident = false;

    std::atomic<int>   detectedNote { -1 };
    std::atomic<float> detectedCents { 0.0f };
    std::atomic<bool>  pitchPresent { false };
};
