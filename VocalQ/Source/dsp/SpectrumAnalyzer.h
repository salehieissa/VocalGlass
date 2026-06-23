#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>

//==============================================================================
// SpectrumAnalyzer — a lightweight real-time FFT used to draw the live
// frequency curve behind the EQ. The audio thread pushes samples and runs the
// transform; the UI thread reads smoothed per-bin dB values (lock-free atomics).
//==============================================================================
class SpectrumAnalyzer
{
public:
    static constexpr int fftOrder = 11;            // 2048-point
    static constexpr int fftSize  = 1 << fftOrder;
    static constexpr int numBins  = fftSize / 2;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        fifoIndex = 0;
        fifo.fill (0.0f);
        for (auto& b : bins) b.store (-120.0f);
    }

    void pushBlock (const juce::AudioBuffer<float>& buffer) noexcept
    {
        const int n  = buffer.getNumSamples();
        const int nc = buffer.getNumChannels();
        const float* L = buffer.getReadPointer (0);
        const float* R = nc > 1 ? buffer.getReadPointer (1) : L;
        for (int i = 0; i < n; ++i)
            pushSample (0.5f * (L[i] + R[i]));
    }

    // Smoothed magnitude (dBFS-ish) at a frequency, for the UI.
    float levelDb (double freqHz) const noexcept
    {
        const double bin = freqHz / (sr * 0.5) * (double) numBins;
        const int i = juce::jlimit (0, numBins - 1, (int) bin);
        return bins[(size_t) i].load();
    }

private:
    void pushSample (float s) noexcept
    {
        fifo[(size_t) fifoIndex] = s;
        if (++fifoIndex >= fftSize)
        {
            processFrame();
            fifoIndex = 0;
        }
    }

    void processFrame() noexcept
    {
        std::copy (fifo.begin(), fifo.end(), fftData.begin());
        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
        window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        // Full-scale sine -> ~0 dB with a Hann window (coherent gain 0.5).
        const float norm = 1.0f / ((float) fftSize * 0.25f);
        for (int k = 0; k < numBins; ++k)
        {
            const float db = juce::Decibels::gainToDecibels (fftData[(size_t) k] * norm + 1.0e-9f);
            const float prev = bins[(size_t) k].load();
            // fast attack, slow release for the classic analyzer feel
            const float v = db > prev ? db : prev + (db - prev) * 0.28f;
            bins[(size_t) k].store (v);
        }
    }

    double sr = 44100.0;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize,
                                                 juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize>     fifo {};
    std::array<float, fftSize * 2> fftData {};
    int fifoIndex = 0;
    std::array<std::atomic<float>, numBins> bins;
};
