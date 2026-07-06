#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include <array>
#include <cmath>

//==============================================================================
// The wide "air display" arc meter. A shallow dome of tick marks with dB
// labels; the upper region (above ~-12 dB) is hot pink, the lower part gray.
// A pink dot rides along the arc at the current output level.
//==============================================================================
class ArcMeter : public juce::Component
{
public:
    ArcMeter() = default;

    // level is a linear magnitude (0..~1+) published by the engine.
    void setLevel (float linear)
    {
        const float db = juce::Decibels::gainToDecibels (linear + 1.0e-6f);
        target = dbToT (db);
        smoothed += (target - smoothed) * (target > smoothed ? 0.5f : 0.12f);
        repaint();
    }

    // Smoothed 0..1 meter position — read by the plate renderer, which reveals
    // the baked lit tick arc up to this point instead of vector-drawing.
    float getT() const noexcept { return juce::jlimit (0.0f, 1.0f, smoothed); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (14.0f);

        // Dome geometry: a large circle whose top sits near the card top.
        const float span   = juce::degreesToRadians (62.0f); // half-angle of the arc
        const float halfW  = r.getWidth() * 0.46f;
        const float radius = halfW / std::sin (span);
        const float cx     = r.getCentreX();
        const float cy     = r.getY() + radius + r.getHeight() * 0.06f;

        auto pointAt = [cx, cy, radius] (float ang)
        {
            return juce::Point<float> (cx + radius * std::sin (ang),
                                       cy - radius * std::cos (ang));
        };

        const float a0 = -span;        // left end
        const float a1 =  span;        // right end
        const float aThresh = a0 + (a1 - a0) * kThreshT; // -12 dB boundary

        // ----- tick marks -----
        const int ticks = 72;
        for (int i = 0; i <= ticks; ++i)
        {
            const float t   = (float) i / (float) ticks;
            const float ang = a0 + t * (a1 - a0);
            const bool  hot = ang >= aThresh;
            const bool  major = (i % 9 == 0);

            const float len = major ? 13.0f : 8.0f;
            const auto outer = pointAt (ang);
            const auto inner = juce::Point<float> (cx + (radius - len) * std::sin (ang),
                                                   cy - (radius - len) * std::cos (ang));
            g.setColour (hot ? theme::accent : theme::inkSoft.withAlpha (0.55f));
            g.drawLine ({ inner, outer }, major ? 1.8f : 1.1f);
        }

        // ----- labels -----
        g.setFont (theme::font (13.0f, false));
        for (const auto& lab : labels())
        {
            const float ang = a0 + lab.t * (a1 - a0);
            const bool  hot = ang >= aThresh;
            const float lr  = radius - 30.0f;
            const auto p = juce::Point<float> (cx + lr * std::sin (ang),
                                               cy - lr * std::cos (ang));
            g.setColour (hot ? theme::accent : theme::inkSoft);
            g.drawText (lab.text,
                        juce::Rectangle<float> (p.x - 26.0f, p.y - 11.0f, 52.0f, 22.0f),
                        juce::Justification::centred);
        }

        // ----- level dot -----
        const float t   = juce::jlimit (0.0f, 1.0f, smoothed);
        const float ang = a0 + t * (a1 - a0);
        const auto dot  = pointAt (ang);

        juce::Path glow;
        glow.addEllipse (dot.x - 9.0f, dot.y - 9.0f, 18.0f, 18.0f);
        theme::glowPath (g, glow, 0.5f, 18);

        g.setColour (theme::accent);
        g.fillEllipse (dot.x - 6.0f, dot.y - 6.0f, 12.0f, 12.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (dot.x - 2.2f, dot.y - 2.2f, 4.4f, 4.4f);
    }

private:
    struct Label { juce::String text; float t; };

    // Non-linear meter scale, matching the mockup left -> right.
    static constexpr float kThreshT = 0.60f; // the -12 dB boundary

    static const std::array<Label, 8>& labels()
    {
        static const std::array<Label, 8> l { {
            { "-inf", 0.00f }, { "-96", 0.13f }, { "-48", 0.30f }, { "-24", 0.46f },
            { "-12", 0.60f },  { "-6", 0.72f },  { "-3", 0.85f },  { "+", 1.00f }
        } };
        return l;
    }

    // Map a dB value onto the meter's non-linear 0..1 position.
    static float dbToT (float db)
    {
        struct BP { float db, t; };
        static const BP bps[] = {
            { -120.0f, 0.00f }, { -96.0f, 0.13f }, { -48.0f, 0.30f }, { -24.0f, 0.46f },
            { -12.0f, 0.60f },  { -6.0f, 0.72f },  { -3.0f, 0.85f },  { 0.0f, 1.00f }
        };
        if (db <= bps[0].db) return 0.0f;
        if (db >= bps[7].db) return 1.0f;
        for (int i = 1; i < 8; ++i)
            if (db <= bps[i].db)
            {
                const float f = (db - bps[i - 1].db) / (bps[i].db - bps[i - 1].db);
                return bps[i - 1].t + f * (bps[i].t - bps[i - 1].t);
            }
        return 1.0f;
    }

    float target = 0.0f, smoothed = 0.0f;
};
