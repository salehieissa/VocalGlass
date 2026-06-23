#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>

//==============================================================================
// Band filter types (indices line up with the UI type selector).
//==============================================================================
enum class BandType { Bell = 0, LowShelf, HighShelf, LowCut, HighCut, Notch };
enum class BandChan { Stereo = 0, Mid, Side };

//==============================================================================
// Builds IIR coefficients for a band. Shared by the audio thread (to filter)
// and the editor (to draw the response curve) so they always agree.
//==============================================================================
inline juce::dsp::IIR::Coefficients<float>::Ptr
eqMakeCoeffs (BandType type, double sr, double freq, double q, double gainDb)
{
    freq = juce::jlimit (16.0, sr * 0.49, freq);
    q    = juce::jlimit (0.1, 60.0, q);
    const float gain = juce::Decibels::decibelsToGain ((float) gainDb);

    using C = juce::dsp::IIR::Coefficients<float>;
    switch (type)
    {
        case BandType::LowShelf:  return C::makeLowShelf  (sr, freq, (float) q, gain);
        case BandType::HighShelf: return C::makeHighShelf (sr, freq, (float) q, gain);
        case BandType::LowCut:    return C::makeHighPass   (sr, freq, (float) q);
        case BandType::HighCut:   return C::makeLowPass    (sr, freq, (float) q);
        case BandType::Notch:     return C::makeNotch      (sr, freq, (float) q);
        case BandType::Bell:
        default:                  return C::makePeakFilter (sr, freq, (float) q, gain);
    }
}

// Cuts are run as two cascaded biquads (24 dB/oct) so they're genuinely strong.
inline bool eqIsCut (BandType t) { return t == BandType::LowCut || t == BandType::HighCut; }

// Magnitude of a band at a frequency, accounting for the cut cascade. Used by
// both the audio engine and the curve display so they always agree.
inline double eqBandMagDb (BandType type, double sr, double freq, double q, double gainDb, double atFreq)
{
    auto c = eqMakeCoeffs (type, sr, freq, q, gainDb);
    double db = juce::Decibels::gainToDecibels (c->getMagnitudeForFrequency (atFreq, sr));
    if (eqIsCut (type)) db *= 2.0;   // two identical stages -> magnitude squared
    return db;
}

//==============================================================================
// One dynamic-EQ band: a static filter whose gain is modulated by an envelope
// follower (threshold / attack / release / range), processed on Stereo, Mid or
// Side. Publishes its live shape (atomics) for the curve display.
//==============================================================================
class EQBand
{
public:
    struct Params
    {
        bool  on = false;
        int   type = (int) BandType::Bell;
        float freq = 1000.0f, q = 1.0f, gain = 0.0f;
        float range = 0.0f, thresh = 0.0f, atkMs = 16.0f, relMs = 160.0f;
        int   chan = (int) BandChan::Stereo;
        int   scMode = 0; // 0 = split (band-focused detect), 1 = wide (broadband)
    };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sr = spec.sampleRate;
        for (auto& f : filt)  f.prepare (spec);
        for (auto& f : filtB) f.prepare (spec);
        detFilt.prepare (spec);
        reset();
    }

    void reset()
    {
        for (auto& f : filt)  f.reset();
        for (auto& f : filtB) f.reset();
        detFilt.reset();
        env = 0.0f;
        coeffsValid = false;   // force a coefficient rebuild on the next block
        detCoeffsValid = false;
    }

    void setParams (const Params& p) { params = p; }

    void process (juce::AudioBuffer<float>& buf)
    {
        const int n  = buf.getNumSamples();
        const int nc = buf.getNumChannels();
        if (n == 0) return;

        if (! params.on)
        {
            pubOn.store (false);
            return;
        }

        auto* L = buf.getWritePointer (0);
        auto* R = buf.getWritePointer (nc > 1 ? 1 : 0);

        const auto type = (BandType) params.type;
        const auto chan = (BandChan) params.chan;
        const double freq = params.freq;
        const double q    = juce::jmax (0.1f, params.q);
        const bool isCut   = eqIsCut (type);
        const bool isNotch = (type == BandType::Notch);
        // Cut / Notch filters have no gain term in their coeffs, so for them the
        // dynamic movement is expressed as how deeply the filter is blended in
        // (a dynamic cut/notch). Bell & shelf bands modulate their gain instead.
        const bool depthBand = isCut || isNotch;

        // ---- dynamic envelope of the detection signal ----
        float dynDb  = 0.0f;    // bell/shelf: gain modulation
        float dynMix = 1.0f;    // cut/notch: applied depth (1 = full filter)
        const bool dynamic = std::abs (params.range) > 0.01f;
        if (dynamic)
        {
            const float atk = std::exp (-1.0f / (juce::jmax (0.05f, params.atkMs) * 0.001f * (float) sr));
            const float rel = std::exp (-1.0f / (juce::jmax (1.0f,  params.relMs) * 0.001f * (float) sr));

            if (params.scMode == 0) // split: detect through a bandpass at the band
            {
                // Only rebuild the detector bandpass when its freq/q actually
                // changed — the coefficients are identical otherwise.
                const double detQ = juce::jmax (0.5, q);
                if (! detCoeffsValid || ! juce::exactlyEqual (freq, detFreqCached)
                    || ! juce::exactlyEqual (detQ, detQCached))
                {
                    *detFilt.coefficients = *juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, freq, (float) detQ);
                    detFreqCached = freq; detQCached = detQ; detCoeffsValid = true;
                }
            }

            float maxEnv = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float l = L[i], r = R[i];
                const float m = 0.5f * (l + r), s = 0.5f * (l - r);
                // Stereo follows the louder channel so detection responds to
                // either side, not just the mono (mid) sum.
                float det = chan == BandChan::Side ? s
                          : chan == BandChan::Mid  ? m
                          : (std::abs (l) >= std::abs (r) ? l : r);
                if (params.scMode == 0) det = detFilt.processSample (det);
                const float rect = std::abs (det);
                env = rect > env ? atk * (env - rect) + rect : rel * (env - rect) + rect;
                maxEnv = juce::jmax (maxEnv, env);
            }

            const float levelDb = juce::Decibels::gainToDecibels (maxEnv + 1.0e-9f);
            const float over    = levelDb - params.thresh;
            const float amt     = juce::jlimit (0.0f, 1.0f, over / 6.0f); // full move at +6 dB over

            if (depthBand)
                dynMix = juce::jlimit (0.0f, 1.0f, (std::abs (params.range) / 18.0f) * amt);
            else
                dynDb  = params.range * amt;
        }

        const float effGain = params.gain + dynDb;

        // ---- update filter coefficients ----
        // Recompute only when an input to eqMakeCoeffs changed. When dynamics are
        // active effGain moves each block (so we update); when range == 0 and the
        // static params are unchanged, the coefficients are bit-identical to last
        // block, so we skip the redundant rebuild (and its heap allocation).
        if (! coeffsValid || type != typeCached
            || ! juce::exactlyEqual (freq, freqCached)
            || ! juce::exactlyEqual (q, qCached)
            || ! juce::exactlyEqual (effGain, gainCached))
        {
            auto c = eqMakeCoeffs (type, sr, freq, q, effGain);
            for (auto& f : filt)  *f.coefficients = *c;
            if (isCut) for (auto& f : filtB) *f.coefficients = *c;
            typeCached = type; freqCached = freq; qCached = q; gainCached = effGain;
            coeffsValid = true;
        }

        // publish live shape for the UI
        pubOn.store (true);
        pubType.store (params.type);
        pubFreq.store (params.freq);
        pubQ.store ((float) q);
        pubGainEff.store (effGain);
        pubChan.store (params.chan);
        pubDepth.store (dynMix);

        // ---- apply (dynMix blends the filtered signal against dry; = 1 for
        //      bell/shelf so they pass through their gain-shaped output fully) ----
        if (chan == BandChan::Stereo)
        {
            for (int i = 0; i < n; ++i)
            {
                const float dry = L[i];
                float x = filt[0].processSample (dry);
                if (isCut) x = filtB[0].processSample (x);
                L[i] = dry + dynMix * (x - dry);
            }
            if (nc > 1)
                for (int i = 0; i < n; ++i)
                {
                    const float dry = R[i];
                    float x = filt[1].processSample (dry);
                    if (isCut) x = filtB[1].processSample (x);
                    R[i] = dry + dynMix * (x - dry);
                }
        }
        else // Mid or Side
        {
            const bool mid = (chan == BandChan::Mid);
            for (int i = 0; i < n; ++i)
            {
                const float l = L[i], r = (nc > 1 ? R[i] : L[i]);
                float m = 0.5f * (l + r), s = 0.5f * (l - r);
                const float dry = mid ? m : s;
                float x = filt[0].processSample (dry);
                if (isCut) x = filtB[0].processSample (x);
                const float wet = dry + dynMix * (x - dry);
                if (mid) m = wet; else s = wet;
                L[i] = m + s;
                if (nc > 1) R[i] = m - s;
            }
        }
    }

    double getMagnitudeDb (double freqHz) const
    {
        if (! pubOn.load()) return 0.0;
        double db = eqBandMagDb ((BandType) pubType.load(), sr,
                                 pubFreq.load(), juce::jmax (0.1f, pubQ.load()), pubGainEff.load(), freqHz);
        // Cut/notch are depth-blended, so scale their drawn magnitude by how much
        // of the filter is currently applied (bell/shelf keep depth = 1).
        return db * (double) pubDepth.load();
    }

    // Live shape, published for the editor (read on the message thread).
    std::atomic<bool>  pubOn { false };
    std::atomic<int>   pubType { 0 };
    std::atomic<float> pubFreq { 1000.0f };
    std::atomic<float> pubQ { 1.0f };
    std::atomic<float> pubGainEff { 0.0f };
    std::atomic<int>   pubChan { 0 };
    std::atomic<float> pubDepth { 1.0f };

private:
    double sr = 44100.0;
    Params params;
    std::array<juce::dsp::IIR::Filter<float>, 2> filt;
    std::array<juce::dsp::IIR::Filter<float>, 2> filtB;   // 2nd cascade stage for cuts
    juce::dsp::IIR::Filter<float> detFilt;
    float env = 0.0f;

    // Cache of the last inputs used to build the band coefficients, so identical
    // states skip the rebuild. Tracks exactly what is loaded into filt/filtB.
    bool     coeffsValid = false;
    BandType typeCached = BandType::Bell;
    double   freqCached = 0.0, qCached = 0.0;
    float    gainCached = 0.0f;

    // Same idea for the detector bandpass.
    bool   detCoeffsValid = false;
    double detFreqCached = 0.0, detQCached = 0.0;
};
