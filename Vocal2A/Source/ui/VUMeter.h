#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include <array>
#include <cmath>

//==============================================================================
// VUMeter — a classic analog VU arc with a swinging needle. The dark left
// portion of the scale is the negative range (-20..-1), the pink right portion
// is 0..+3. Arc line, ticks, labels and needle all share one pivot/geometry.
//
// setLevel() takes a value on the printed scale (-20..+3); the needle smooths
// toward it for an authentic, slightly lagging ballistic motion.
//==============================================================================
class VUMeter : public juce::Component,
                private juce::Timer
{
public:
    VUMeter() { startTimerHz (30); }

    // value expressed on the printed VU scale (-20 .. +3)
    void setLevel (float scaleValue) { target = juce::jlimit (-20.0f, 3.5f, scaleValue); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();

        // premium small-caps title
        theme::spacedText (g, "gain reduction", b.removeFromTop (30.0f),
                           theme::ink, 12.5f, 2.4f, true, juce::Justification::centred);

        // recessed glass face the needle swings across
        auto face = b.reduced (2.0f);
        theme::recess (g, face, 16.0f);

        // Pivot sits just below the face; the scale is a true arc centred on it,
        // with everything (arc line, ticks, labels, needle) sharing one geometry
        // so they line up the way a real VU meter does.
        const float cx     = face.getCentreX();
        const float pivotY = face.getY() + face.getHeight() * 0.86f;
        const float radius = juce::jmin (face.getWidth() * 0.42f, face.getHeight() * 0.70f);

        const float leftAng  = juce::degreesToRadians (-50.0f);
        const float rightAng = juce::degreesToRadians ( 50.0f);

        auto angleForFrac = [leftAng, rightAng] (float f)
        { return leftAng + juce::jlimit (0.0f, 1.0f, f) * (rightAng - leftAng); };

        auto polar = [cx, pivotY] (float ang, float r)
        {
            return juce::Point<float> (cx + r * std::sin (ang),
                                       pivotY - r * std::cos (ang));
        };

        const float zeroAng = angleForFrac (fracForValue (0.0f));

        // ---- scale arc: dark across the negative range, pink across 0..+3 ----
        juce::Path darkArc, pinkArc;
        darkArc.addCentredArc (cx, pivotY, radius, radius, 0.0f, leftAng, zeroAng, true);
        pinkArc.addCentredArc (cx, pivotY, radius, radius, 0.0f, zeroAng, rightAng, true);
        g.setColour (theme::ink);    g.strokePath (darkArc, juce::PathStrokeType (2.2f));
        g.setColour (theme::accent); g.strokePath (pinkArc, juce::PathStrokeType (2.4f));

        // ---- scale marks + labels (just outside the arc) ----
        for (const auto& m : kMarks)
        {
            const float ang = angleForFrac (m.frac);
            const bool pink = m.value >= 0.0f;
            g.setColour (pink ? theme::accent : theme::ink);

            const auto p0 = polar (ang, radius - 7.0f);
            const auto p1 = polar (ang, radius);
            g.drawLine (p0.x, p0.y, p1.x, p1.y, pink ? 1.8f : 1.6f);

            const auto pt = polar (ang, radius + 13.0f);
            g.setFont (theme::font (pink ? 12.0f : 11.0f, false));
            g.drawText (m.value == 0.0f ? juce::String ("0") : juce::String ((int) m.value),
                        juce::Rectangle<float> (pt.x - 14.0f, pt.y - 9.0f, 28.0f, 18.0f),
                        juce::Justification::centred, false);
        }

        // minor ticks between marks for a denser analog look
        for (size_t i = 0; i + 1 < kMarks.size(); ++i)
        {
            const float fmid = (kMarks[i].frac + kMarks[i + 1].frac) * 0.5f;
            const float ang  = angleForFrac (fmid);
            const bool pink  = kMarks[i].value >= 0.0f && kMarks[i + 1].value >= 0.0f;
            g.setColour ((pink ? theme::accent : theme::ink).withAlpha (0.5f));
            const auto p0 = polar (ang, radius - 4.0f);
            const auto p1 = polar (ang, radius);
            g.drawLine (p0.x, p0.y, p1.x, p1.y, 1.0f);
        }

        // ---- needle + hub ----
        const float ang  = angleForFrac (fracForValue (pos));
        const auto  tip  = polar (ang, radius - 6.0f);
        const auto  tail = polar (ang + juce::MathConstants<float>::pi, radius * 0.10f);
        g.setColour (theme::ink);
        g.drawLine (tail.x, tail.y, tip.x, tip.y, 2.4f);
        g.fillEllipse (cx - 6.0f, pivotY - 6.0f, 12.0f, 12.0f);
        g.setColour (theme::accent);
        g.fillEllipse (cx - 2.5f, pivotY - 2.5f, 5.0f, 5.0f);
    }

private:
    struct Mark { float value; float frac; };

    // values & their fractional position along the arc (left 0 .. right 1)
    static inline const std::array<Mark, 11> kMarks = {{
        { -20.0f, 0.00f }, { -10.0f, 0.18f }, { -7.0f, 0.32f }, { -5.0f, 0.43f },
        { -3.0f, 0.55f }, { -2.0f, 0.62f }, { -1.0f, 0.68f }, { 0.0f, 0.74f },
        { 1.0f, 0.84f }, { 2.0f, 0.92f }, { 3.0f, 1.00f }
    }};

    static float fracForValue (float v)
    {
        if (v <= kMarks.front().value) return kMarks.front().frac;
        if (v >= kMarks.back().value)  return kMarks.back().frac;
        for (size_t i = 1; i < kMarks.size(); ++i)
        {
            if (v <= kMarks[i].value)
            {
                const auto& a = kMarks[i - 1];
                const auto& b = kMarks[i];
                const float t = (v - a.value) / (b.value - a.value);
                return a.frac + t * (b.frac - a.frac);
            }
        }
        return kMarks.back().frac;
    }

    void timerCallback() override
    {
        pos += (target - pos) * 0.25f;
        if (std::abs (target - pos) > 0.01f) repaint();
    }

    float pos = -20.0f, target = -20.0f;
};
