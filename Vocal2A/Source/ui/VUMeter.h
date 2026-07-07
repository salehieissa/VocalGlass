#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "../../../common/ui/Skin.h"
#include <array>
#include <cmath>

//==============================================================================
// VUMeter — a classic analog VU arc with a swinging needle. The dark left
// portion of the scale is the negative range (-20..-1), the pink right portion
// is 0..+3. Arc line, ticks, labels and needle all share one pivot/geometry.
//
// setLevel() takes a value on the printed scale (-20..+3); the needle smooths
// toward it for an authentic, slightly lagging ballistic motion.
//
// If the photoreal skin is present (2a-vu-face + 2a-vu-needle), the glass face
// and needle are drawn from the bundled images and the needle is swung in code
// to line up with the printed scale; otherwise everything is drawn as vectors.
//==============================================================================
class VUMeter : public juce::Component,
                private juce::Timer
{
public:
    VUMeter()
    {
        faceImg   = skin::image ("2a-vu-face@2x.png");
        needleImg = skin::image ("2a-vu-needle@2x.png");
        // Needle-only mode: when the baked chassis is present it carries the VU
        // face, so we swing just the needle over it (ignore any stale face art).
        const bool chassisBaked = skin::has ("2a-chassis@2x.png");
        needleOnly = needleImg.isValid() && (chassisBaked || ! faceImg.isValid());
        useSkin   = ! needleOnly && faceImg.isValid() && needleImg.isValid();
        startTimerHz (30);
    }

    // value expressed on the printed VU scale (-20 .. +3)
    void setLevel (float scaleValue) { target = juce::jlimit (-20.0f, 3.5f, scaleValue); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();

        if (needleOnly)
        {
            // Swing just the needle over the baked VU face. Pivot lands on the
            // hub printed on the plate; length reaches the printed scale.
            const juce::Point<float> pivot { b.getX() + pivotFx * b.getWidth(),
                                             b.getY() + pivotFy * b.getHeight() };
            const float lenToTip = tipLenFx * b.getWidth();
            const float ang = juce::degreesToRadians (angleDegForValue (pos));
            skin::drawNeedle (g, needleImg, pivot, 0.500f, 0.851f, 0.089f, lenToTip, ang);
            return;
        }

        if (useSkin) { paintSkin (g, b); return; }

        // premium small-caps title (vector fallback keeps its own title)
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
    //==========================================================================
    // Photoreal path: draw the glass face image, then swing the image needle so
    // its tip tracks the printed scale (angles measured off the generated art).
    void paintSkin (juce::Graphics& g, juce::Rectangle<float> area)
    {
        // Fill the width; transparent top/bottom margins are clipped by the host.
        const auto face = skin::widthRect (faceImg, area);
        skin::drawInRect (g, faceImg, face);

        // Hub anchored inside the visible glass (fractions of the drawn face
        // rect). The printed arc's true centre is far below the art, so we place
        // a visible hub at ~0.86 down and map each tick to the angle it subtends
        // from there — a natural ~84 deg sweep that tracks the printed numbers.
        const juce::Point<float> pivot { face.getX() + 0.510f * face.getWidth(),
                                         face.getY() + 0.800f * face.getHeight() };
        const float lenToTip = 0.450f * face.getWidth();

        const float deg = angleDegForValue (pos);
        const float ang = juce::degreesToRadians (deg);

        // needle image: hub at (0.503, 0.81), tip at y 0.093 of its own bounds
        skin::drawNeedle (g, needleImg, pivot, 0.503f, 0.81f, 0.093f, lenToTip, ang);
    }

    // Angle (deg, clockwise, 0 = up) for a printed scale value. Measured off
    // the glass/chrome plate: circle-fit pivot through the printed numbers,
    // then the angle each number subtends from it. Interpolated between marks.
    static float angleDegForValue (float v)
    {
        struct A { float value, deg; };
        static const std::array<A, 7> k = {{
            { -20.0f, -27.8f }, { -10.0f, -17.5f }, { -7.0f, -10.7f },
            { -5.0f, -5.3f }, { -3.0f, 1.1f }, { 0.0f, 13.8f }, { 3.0f, 29.6f }
        }};
        if (v <= k.front().value) return k.front().deg;
        if (v >= k.back().value)  return k.back().deg;
        for (size_t i = 1; i < k.size(); ++i)
            if (v <= k[i].value)
            {
                const float t = (v - k[i - 1].value) / (k[i].value - k[i - 1].value);
                return k[i - 1].deg + t * (k[i].deg - k[i - 1].deg);
            }
        return k.back().deg;
    }

    juce::Image faceImg, needleImg;
    bool useSkin = false;
    bool needleOnly = false;
public:
    // pivot + length as fractions of the component bounds (tuned to the plate)
    float pivotFx = 0.500f, pivotFy = 0.560f, tipLenFx = 0.360f;
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
