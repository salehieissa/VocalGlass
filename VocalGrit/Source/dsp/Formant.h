#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <complex>
#include <cmath>

//==============================================================================
// Formant — a TRUE spectral formant shifter (STFT / phase-vocoder style).
//
// The previous version moved a few resonant EQ peaks around, which just reads
// as a moving high/low-pass tilt. A real formant shift has to separate the
// vocal's *spectral envelope* (the formants / vocal-tract resonances that make
// the "ahh/eee" colour and perceived head size) from the *fine harmonic
// structure* (the pitch), then move ONLY the envelope and leave the pitch where
// it is.
//
// Per STFT frame we:
//   1. take the magnitude spectrum,
//   2. estimate a smooth spectral envelope (box-smooth across bins),
//   3. resample that envelope by `factor` (= 2^shift) to move the formants
//      up or down WITHOUT touching the harmonics,
//   4. multiply each bin by warpedEnv / env so the harmonics stay put but sit
//      under the shifted formant shape.
//
// Result: pitch is preserved, the voice gets "smaller/brighter" (shift up) or
// "bigger/deeper" (shift down) — an actual formant shift, not an EQ.
//
// Cost: an STFT has inherent latency of one FFT block. We report it to the host
// so everything stays phase-aligned. fftSize 1024 ~= 21 ms @ 48 kHz.
//==============================================================================
class Formant
{
public:
    static constexpr int fftOrder = 10;            // 2^10 = 1024
    static constexpr int fftSize  = 1 << fftOrder; // 1024
    static constexpr int hop      = fftSize / 4;   // 256  (75% overlap)
    static constexpr int numBins  = fftSize / 2 + 1;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate  = spec.sampleRate;
        numChannels = (int) spec.numChannels;

        // Periodic Hann window (used for both analysis and synthesis).
        window.resize (fftSize);
        double wss = 0.0;
        for (int i = 0; i < fftSize; ++i)
        {
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                         * (float) i / (float) fftSize);
            wss += (double) window[(size_t) i] * window[(size_t) i];
        }
        // COLA normalisation for window^2 overlap (Hann @ 75% -> ~1.5).
        norm = (float) ((double) hop / juce::jmax (1.0e-9, wss));

        fftData.assign ((size_t) (2 * fftSize), 0.0f);
        mag.assign     ((size_t) numBins, 0.0f);
        env.assign     ((size_t) numBins, 0.0f);
        cep1.assign    ((size_t) fftSize, {});
        cep2.assign    ((size_t) fftSize, {});

        // Cepstral lifter cutoff: keep quefrencies broader than ~700 Hz of
        // spectral detail (the formants) and drop the finer pitch ripple.
        liftOrder = juce::jlimit (16, fftSize / 8, (int) (sampleRate / 700.0));

        in.assign  ((size_t) juce::jmax (1, numChannels), std::vector<float> ((size_t) fftSize, 0.0f));
        out.assign ((size_t) juce::jmax (1, numChannels), std::vector<float> ((size_t) fftSize, 0.0f));
        pos.assign  ((size_t) juce::jmax (1, numChannels), 0);
        count.assign((size_t) juce::jmax (1, numChannels), 0);

        reset();
    }

    void reset()
    {
        for (auto& v : in)  std::fill (v.begin(), v.end(), 0.0f);
        for (auto& v : out) std::fill (v.begin(), v.end(), 0.0f);
        std::fill (pos.begin(),   pos.end(),   0);
        std::fill (count.begin(), count.end(), 0);
    }

    int getLatencySamples() const noexcept { return fftSize; }

    // shift in -1..+1 (0 = neutral). +1 = +1 octave formants, -1 = -1 octave.
    void setShift (float s) noexcept
    {
        factor = std::pow (2.0f, juce::jlimit (-1.0f, 1.0f, s));
    }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int chans = juce::jmin (buffer.getNumChannels(), (int) in.size());
        const int n     = buffer.getNumSamples();

        for (int ch = 0; ch < chans; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
                data[i] = processSample (ch, data[i]);
        }
    }

private:
    inline float processSample (int ch, float x) noexcept
    {
        const auto c = (size_t) ch;
        const int  p = pos[c];

        const float y = out[c][(size_t) p];
        out[c][(size_t) p] = 0.0f;          // consumed -> clear for future overlap-add
        in[c][(size_t) p]  = x;

        pos[c] = (p + 1) & (fftSize - 1);

        if (++count[c] >= hop)
        {
            count[c] = 0;
            processFrame (ch);
        }
        return y;
    }

    void processFrame (int ch) noexcept
    {
        const auto c   = (size_t) ch;
        const int  start = pos[c];          // oldest sample of the most-recent fftSize

        // --- analysis: windowed frame (time ordered) ---
        for (int i = 0; i < fftSize; ++i)
            fftData[(size_t) i] = in[c][(size_t) ((start + i) & (fftSize - 1))] * window[(size_t) i];
        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);

        fft.performRealOnlyForwardTransform (fftData.data());

        // --- magnitudes ---
        float maxMag = 1.0e-9f;
        for (int k = 0; k < numBins; ++k)
        {
            const float re = fftData[(size_t) (2 * k)];
            const float im = fftData[(size_t) (2 * k + 1)];
            const float m  = std::sqrt (re * re + im * im);
            mag[(size_t) k] = m;
            maxMag = juce::jmax (maxMag, m);
        }

        // --- spectral envelope via cepstral liftering ---
        // Smooth the LOG magnitude (cepstrum -> keep low quefrencies -> back),
        // which separates the formant shape from the harmonic/pitch ripple far
        // better than averaging linear magnitude. This is what makes the shift
        // sound like a real formant move instead of a tilt.
        const float logFloor = maxMag * 1.0e-5f + 1.0e-9f;
        for (int k = 0; k < fftSize; ++k)
        {
            const float m = (k < numBins) ? mag[(size_t) k]
                                          : mag[(size_t) (fftSize - k)];
            cep1[(size_t) k] = std::complex<float> (std::log (m + logFloor), 0.0f);
        }
        fft.perform (cep1.data(), cep2.data(), true);    // -> real cepstrum (1/N normalised)
        for (int n = 0; n < fftSize; ++n)
            if (n > liftOrder && n < fftSize - liftOrder)
                cep2[(size_t) n] = {};
        fft.perform (cep2.data(), cep1.data(), false);   // -> smoothed log spectrum
        for (int k = 0; k < numBins; ++k)
            env[(size_t) k] = std::exp (cep1[(size_t) k].real());

        const float floorE = maxMag * 1.0e-4f;  // keep quiet bins from blowing up

        // --- warp envelope by factor and re-shape each bin ---
        const float f = factor;
        for (int k = 0; k < numBins; ++k)
        {
            const float e  = juce::jmax (env[(size_t) k], floorE);
            const float w  = juce::jmax (sampleEnv ((float) k / f), floorE);
            float r = w / e;
            r = juce::jlimit (0.05f, 20.0f, r);
            fftData[(size_t) (2 * k)]     *= r;
            fftData[(size_t) (2 * k + 1)] *= r;
        }

        fft.performRealOnlyInverseTransform (fftData.data());

        // --- synthesis: windowed overlap-add ---
        for (int i = 0; i < fftSize; ++i)
            out[c][(size_t) ((start + i) & (fftSize - 1))]
                += fftData[(size_t) i] * window[(size_t) i] * norm;
    }

    inline float sampleEnv (float idx) const noexcept
    {
        idx = juce::jlimit (0.0f, (float) (numBins - 1), idx);
        const int i0 = (int) idx;
        const int i1 = juce::jmin (i0 + 1, numBins - 1);
        const float fr = idx - (float) i0;
        return env[(size_t) i0] * (1.0f - fr) + env[(size_t) i1] * fr;
    }

    double sampleRate  = 44100.0;
    int    numChannels = 2;
    float  norm        = 1.0f;
    float  factor      = 1.0f;

    int liftOrder = 64;

    juce::dsp::FFT fft { fftOrder };
    std::vector<float> window, fftData, mag, env;
    std::vector<juce::dsp::Complex<float>> cep1, cep2;
    std::vector<std::vector<float>> in, out;
    std::vector<int> pos, count;
};
