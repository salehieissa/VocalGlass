#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

//==============================================================================
// VocalKnob engine — a one-knob multiband maximizer in the spirit of FL's
// Soundgoodizer (a simplified Maximus). The signal is split into three bands
// (low / mid / high); each band is driven, compressed toward a target, softly
// saturated and made up; the bands recombine into a master soft-clip limiter.
//
// A single "amount" (0..1) scales the intensity of everything, and four voicings
// (clean / warm / dirty / blown) reshape the per-band drive / saturation / tone.
//==============================================================================
class Maximizer
{
public:
    enum Mode { Clean = 0, Warm, Dirty, Blown };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sr = spec.sampleRate;

        auto setup = [&spec] (juce::dsp::LinkwitzRileyFilter<float>& f,
                              juce::dsp::LinkwitzRileyFilterType type, float fc)
        {
            f.prepare (spec);
            f.setType (type);
            f.setCutoffFrequency (fc);
        };

        // Split: low/high pair at fLo, then the high path split again at fHi.
        // The low band is run through an allpass at fHi to keep phase aligned.
        setup (lpLo, juce::dsp::LinkwitzRileyFilterType::lowpass,  fLo);
        setup (hpLo, juce::dsp::LinkwitzRileyFilterType::highpass, fLo);
        setup (lpHi, juce::dsp::LinkwitzRileyFilterType::lowpass,  fHi);
        setup (hpHi, juce::dsp::LinkwitzRileyFilterType::highpass, fHi);
        setup (apLo, juce::dsp::LinkwitzRileyFilterType::allpass,  fHi);

        reset();
    }

    void reset()
    {
        lpLo.reset(); hpLo.reset(); lpHi.reset(); hpHi.reset(); apLo.reset();
        for (auto& e : env) for (auto& v : e) v = 0.0f;
    }

    void setParams (float amount01, int mode)
    {
        amount  = juce::jlimit (0.0f, 1.0f, amount01);
        modeIdx = juce::jlimit (0, 3, mode);
    }

    void process (juce::AudioBuffer<float>& buf)
    {
        const int n  = buf.getNumSamples();
        const int nc = juce::jmin (buf.getNumChannels(), 2);
        if (n == 0 || nc == 0) return;

        const Voicing v = voicing (modeIdx, amount);

        const float aL = coef (8.0f),  rL = coef (140.0f);
        const float aM = coef (4.0f),  rM = coef (90.0f);
        const float aH = coef (1.5f),  rH = coef (60.0f);

        for (int ch = 0; ch < nc; ++ch)
        {
            auto* d = buf.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
            {
                const float x = d[i];

                float low      = lpLo.processSample (ch, x);
                float highRest = hpLo.processSample (ch, x);
                float mid      = lpHi.processSample (ch, highRest);
                float high     = hpHi.processSample (ch, highRest);
                low            = apLo.processSample (ch, low);

                low  = band (low,  (size_t) ch, 0, v.driveLo,  v.threshLo,  v.ratioLo,  v.satLo,  v.makeLo,  aL, rL);
                mid  = band (mid,  (size_t) ch, 1, v.driveMid, v.threshMid, v.ratioMid, v.satMid, v.makeMid, aM, rM);
                high = band (high, (size_t) ch, 2, v.driveHi,  v.threshHi,  v.ratioHi,  v.satHi,  v.makeHi,  aH, rH);

                float out = softClip ((low + mid + high) * v.outGain);

                d[i] = x + (out - x) * v.wet;   // 0% amount == bypass
            }
        }
    }

private:
    struct Voicing
    {
        float driveLo, driveMid, driveHi;
        float threshLo, threshMid, threshHi;
        float ratioLo, ratioMid, ratioHi;
        float satLo, satMid, satHi;
        float makeLo, makeMid, makeHi;
        float outGain, wet;
    };

    static float lin (float a, float b, float t) { return a + (b - a) * t; }

    Voicing voicing (int mode, float amt) const
    {
        Voicing v {};
        const float a = amt;
        v.wet     = juce::jmin (1.0f, a * 1.6f);
        v.outGain = juce::Decibels::decibelsToGain (lin (0.0f, 2.5f, a));

        v.threshLo  = lin (-6.0f, -22.0f, a);
        v.threshMid = lin (-6.0f, -24.0f, a);
        v.threshHi  = lin (-6.0f, -22.0f, a);
        v.ratioLo   = lin (1.0f, 4.0f, a);
        v.ratioMid  = lin (1.0f, 4.5f, a);
        v.ratioHi   = lin (1.0f, 3.5f, a);

        switch (mode)
        {
            case Clean:
                v.driveLo = lin (0.f, 2.f, a);  v.driveMid = lin (0.f, 2.f, a);  v.driveHi = lin (0.f, 4.f, a);
                v.satLo = lin (0.f, 0.10f, a);  v.satMid = lin (0.f, 0.12f, a);  v.satHi = lin (0.f, 0.18f, a);
                v.makeLo = 0.5f; v.makeMid = 0.5f; v.makeHi = 1.5f;
                break;
            case Warm:
                v.driveLo = lin (0.f, 5.f, a);  v.driveMid = lin (0.f, 4.f, a);  v.driveHi = lin (0.f, 2.f, a);
                v.satLo = lin (0.f, 0.30f, a);  v.satMid = lin (0.f, 0.32f, a);  v.satHi = lin (0.f, 0.12f, a);
                v.makeLo = 1.5f; v.makeMid = 1.0f; v.makeHi = -1.0f;
                break;
            case Dirty:
                v.driveLo = lin (0.f, 4.f, a);  v.driveMid = lin (0.f, 8.f, a);  v.driveHi = lin (0.f, 6.f, a);
                v.satLo = lin (0.f, 0.45f, a);  v.satMid = lin (0.f, 0.70f, a);  v.satHi = lin (0.f, 0.55f, a);
                v.makeLo = 0.5f; v.makeMid = 1.5f; v.makeHi = 1.0f;
                break;
            case Blown:
            default:
                v.driveLo = lin (0.f, 8.f, a);  v.driveMid = lin (0.f, 12.f, a); v.driveHi = lin (0.f, 10.f, a);
                v.satLo = lin (0.f, 0.75f, a);  v.satMid = lin (0.f, 0.95f, a);  v.satHi = lin (0.f, 0.85f, a);
                v.makeLo = 1.0f; v.makeMid = 2.0f; v.makeHi = 1.5f;
                v.ratioMid = lin (1.0f, 8.0f, a);
                break;
        }
        return v;
    }

    float coef (float ms) const
    {
        return std::exp (-1.0f / (juce::jmax (0.05f, ms) * 0.001f * (float) sr));
    }

    float band (float x, size_t ch, int b, float driveDb, float threshDb,
                float ratio, float sat, float makeDb, float atk, float rel)
    {
        x *= juce::Decibels::decibelsToGain (driveDb);

        float& e = env[(size_t) b][ch];
        const float r = std::abs (x);
        e = r > e ? atk * (e - r) + r : rel * (e - r) + r;
        const float overDb = juce::Decibels::gainToDecibels (e + 1.0e-9f) - threshDb;
        if (overDb > 0.0f && ratio > 1.0f)
            x *= juce::Decibels::decibelsToGain (-overDb * (1.0f - 1.0f / ratio));

        if (sat > 0.0001f)
        {
            const float k = 1.0f + sat * 6.0f;
            const float shaped = std::tanh (x * k) / std::tanh (k);
            x = x + (shaped - x) * sat;
        }

        return x * juce::Decibels::decibelsToGain (makeDb);
    }

    static float softClip (float x)
    {
        const float a = std::abs (x);
        if (a <= 0.7f) return x;
        const float s = (x < 0.0f ? -1.0f : 1.0f);
        return s * (0.7f + 0.3f * std::tanh ((a - 0.7f) / 0.3f));
    }

    double sr = 44100.0;
    float amount = 0.0f;
    int   modeIdx = Dirty;

    static constexpr float fLo = 220.0f;
    static constexpr float fHi = 3200.0f;

    juce::dsp::LinkwitzRileyFilter<float> lpLo, hpLo, lpHi, hpHi, apLo;
    std::array<std::array<float, 2>, 3> env {}; // [band][channel]
};
