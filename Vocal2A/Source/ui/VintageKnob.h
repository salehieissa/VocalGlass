#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "../../../common/ui/Skin.h"
#include <cmath>

//==============================================================================
// VintageKnob — a soft white neumorphic dome knob with a glowing hot-pink value
// ring. No indicator dots or pointer: the lit ring shows position. The Large
// style optionally shows a big value number above the dome.
//
// If the photoreal skin is present (knob@2x.png) the glossy chrome dome is drawn
// from the bundled image and rotated so its pink notch tracks the value, with
// the glowing value ring still drawn in code around it. Falls back to the
// all-vector dome when the asset is missing.
//==============================================================================
class VintageKnob : public juce::Slider
{
public:
    enum class Style { Large, Small };

    explicit VintageKnob (Style s = Style::Large) : style (s)
    {
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setRotaryParameters (kStart, kEnd, true);
        // Keep the whole knob family chrome so big + small match. Prefer the
        // size-specific art, fall back to the shared knob.
        knobImg = skin::image (s == Style::Large ? "knob-large@2x.png" : "knob-small@2x.png");
        if (! knobImg.isValid())
            knobImg = skin::image ("knob@2x.png");

        // Baked "divoted ring": an unlit machined groove (ring-off) that the
        // pink light fills (ring-on). We draw the groove always and reveal the
        // lit copy up to the value with a wedge mask + a code hot-tip. Falls
        // back to the single glow asset, then to the fully-drawn arc.
        const char* offName = (s == Style::Large) ? "ring-off-large@2x.png" : "ring-off-small@2x.png";
        const char* onName  = (s == Style::Large) ? "ring-on-large@2x.png"  : "ring-on-small@2x.png";
        ringOffImg = skin::image (offName); if (! ringOffImg.isValid()) ringOffImg = skin::image ("ring-off@2x.png");
        ringOnImg  = skin::image (onName);  if (! ringOnImg.isValid())  ringOnImg  = skin::image ("ring-on@2x.png");

        glowImg = skin::image (s == Style::Large ? "ring-glow-large@2x.png" : "ring-glow-small@2x.png");
        if (! glowImg.isValid())
            glowImg = skin::image ("ring-glow@2x.png");
    }

    void setBigValueVisible (bool v) { showBigValue = v; }

    // Plate mode: the baked chassis carries the ring seat + halo, so the knob
    // component paints ONLY the rotated brushed-metal dome that seats into it.
    void setPlateMode (bool p) { plateMode = p; showBigValue = false; }

    // Value angle so the editor can align the plate halo to the dome pointer.
    float valueAngle() const
    {
        const double range = getMaximum() - getMinimum();
        const double prop  = range > 0.0 ? (getValue() - getMinimum()) / range : 0.0;
        return (float) (kStart + prop * (kEnd - kStart));
    }
    static constexpr float startAngle() { return kStart; }
    static constexpr float endAngle()   { return kEnd; }

    // Measured geometry of the current dome sprites (hard-alpha bbox): where
    // the dome centre sits in the canvas and how much of the canvas width the
    // visible dome occupies. Re-measure whenever the sprite art changes.
    float domePivotFx() const { return style == Style::Large ? 0.4936f : 0.4992f; }
    float domePivotFy() const { return style == Style::Large ? 0.4936f : 0.4681f; }
    float domeDiaFrac() const { return style == Style::Large ? 0.765f  : 0.518f; }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();

        if (plateMode)
        {
            const double range = getMaximum() - getMinimum();
            const double prop  = range > 0.0 ? (getValue() - getMinimum()) / range : 0.0;
            const float angle  = (float) (kStart + prop * (kEnd - kStart));
            if (knobImg.isValid())
                skin::drawKnobRotated (g, knobImg, area, angle, domePivotFx(), domePivotFy());
            return;
        }

        // reserve top strip for the big number on the large style
        if (showBigValue)
        {
            auto top = area.removeFromTop (style == Style::Large ? 34.0f : 0.0f);
            const int v = juce::roundToInt (getValue());
            g.setColour (theme::ink);
            g.setFont (theme::font (28.0f, true));
            g.drawText (juce::String (v), top, juce::Justification::centred, false);
        }

        const float pad = (style == Style::Large) ? 12.0f : 4.0f;
        area.reduce (pad, pad);
        const float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
        const auto c = area.getCentre();

        const double range = getMaximum() - getMinimum();
        const double prop  = range > 0.0 ? (getValue() - getMinimum()) / range : 0.0;
        const float angle  = (float) (kStart + prop * (kEnd - kStart));

        if (knobImg.isValid())
            drawSkinnedDome (g, c, radius, kStart, kEnd, angle, isMouseOverOrDragging (true));
        else
            drawDome (g, c, radius, kStart, kEnd, angle, isMouseOverOrDragging (true));
    }

private:
    // Photoreal path: a soft blooming neon value-halo (all code) that hugs the
    // brushed-metal dome image. The halo — not a hard ring — is the indicator,
    // matching the mockup: it lights from the start angle to the value with a
    // bright hot tip and a wide additive bloom.
    void drawSkinnedDome (juce::Graphics& g, juce::Point<float> centre, float radius,
                          float startAngle, float endAngle, float angle, bool hover)
    {
        const float ringR = radius * 0.90f;   // halo radius, just outside the dome
        const float discR = radius * 0.74f;   // brushed-metal knob face

        drawSoftHalo (g, centre, ringR, radius, startAngle, endAngle, angle, hover);

        // brushed-metal dome on top (covers any inward bloom bleed). The art's
        // visible knob is ~0.74 of the image; scale up so it fills the disc.
        juce::Rectangle<float> dest (centre.x - discR, centre.y - discR, discR * 2.0f, discR * 2.0f);
        const float over = 1.0f / 0.74f;
        skin::drawKnobRotated (g, knobImg,
                               dest.withSizeKeepingCentre (dest.getWidth() * over,
                                                           dest.getHeight() * over),
                               angle);
    }

    // Soft neon value-halo: a faint sunken track for the whole sweep, then a
    // multi-layer additive pink glow from start to the value with a hot tip.
    static void drawSoftHalo (juce::Graphics& g, juce::Point<float> centre, float ringR,
                              float radius, float startAngle, float endAngle,
                              float angle, bool hover)
    {
        const auto curved  = juce::PathStrokeType::curved;
        const auto rounded = juce::PathStrokeType::rounded;
        const float thick  = juce::jmax (3.0f, radius * 0.085f);

        auto arc = [&] (float a0, float a1)
        {
            juce::Path p;
            p.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, a0, a1, true);
            return p;
        };

        // very faint recessed track across the full sweep (unlit channel)
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        g.strokePath (arc (startAngle, endAngle), juce::PathStrokeType (thick * 0.85f, curved, rounded));

        if (angle <= startAngle + 0.001f) return;

        const juce::Path lit = arc (startAngle, angle);

        // wide soft bloom (the signature glow)
        {
            juce::Path litStroke;
            juce::PathStrokeType (thick).createStrokedPath (litStroke, lit);
            theme::glowPath (g, litStroke, hover ? 1.0f : 0.8f,
                             (int) (thick * (hover ? 5.4f : 4.2f)));
        }
        // broad body
        g.setColour (theme::accent.withAlpha (0.38f));
        g.strokePath (lit, juce::PathStrokeType (thick * 1.9f, curved, rounded));
        // saturated core
        g.setColour (theme::accent);
        g.strokePath (lit, juce::PathStrokeType (thick, curved, rounded));
        // bright inner filament
        g.setColour (theme::accentHi.brighter (0.35f).withAlpha (0.95f));
        g.strokePath (lit, juce::PathStrokeType (thick * 0.42f, curved, rounded));

        // hot tip at the live value position
        const juce::Point<float> tip (centre.x + std::sin (angle) * ringR,
                                      centre.y - std::cos (angle) * ringR);
        const float tr = thick * (hover ? 2.4f : 1.9f);
        juce::ColourGradient hot (juce::Colours::white.withAlpha (hover ? 1.0f : 0.92f), tip.x, tip.y,
                                  juce::Colours::transparentWhite, tip.x, tip.y + tr, true);
        hot.addColour (0.4, theme::accentHi.withAlpha (0.9f));
        g.setGradientFill (hot);
        g.fillEllipse (tip.x - tr, tip.y - tr, tr * 2.0f, tr * 2.0f);
    }

    // A recessed machined groove that fills with layered pink neon up to the
    // value, with a moving hot tip. All code — perfectly aligned + dynamic.
    static void drawDivotedRing (juce::Graphics& g, juce::Point<float> centre, float ringR,
                                 float thick, float startAngle, float endAngle,
                                 float angle, bool hover)
    {
        auto arc = [&] (float a0, float a1)
        {
            juce::Path p;
            p.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, a0, a1, true);
            return p;
        };
        const auto rounded = juce::PathStrokeType::rounded;
        const auto curved  = juce::PathStrokeType::curved;

        const juce::Path track = arc (startAngle, endAngle);

        // ---- recessed groove (the divot) ----
        // valley floor
        g.setColour (juce::Colour (0xffc4c5cf));
        g.strokePath (track, juce::PathStrokeType (thick, curved, rounded));
        // inner shadow on the upper wall (reads as sunken)
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.strokePath (arc (startAngle, endAngle), juce::PathStrokeType (thick * 0.82f, curved, rounded));
        g.setColour (theme::trackDeep);
        g.strokePath (track, juce::PathStrokeType (thick * 0.5f, curved, rounded));
        // bright lower lip where light catches the edge
        {
            juce::Path lip = arc (startAngle, endAngle);
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.strokePath (lip.createPathWithRoundedCorners (0.0f),
                          juce::PathStrokeType (thick, curved, rounded));
            g.setColour (juce::Colour (0xffc4c5cf));
            g.strokePath (track, juce::PathStrokeType (thick * 0.86f, curved, rounded));
        }

        if (angle <= startAngle + 0.001f) return;

        // ---- lit fill inside the groove ----
        const juce::Path lit = arc (startAngle, angle);

        // outer bloom (soft halo)
        {
            juce::Path litStroke;
            juce::PathStrokeType (thick).createStrokedPath (litStroke, lit);
            theme::glowPath (g, litStroke, hover ? 0.95f : 0.65f,
                             (int) (thick * (hover ? 3.4f : 2.6f)));
        }
        // wide soft body
        g.setColour (theme::accent.withAlpha (0.40f));
        g.strokePath (lit, juce::PathStrokeType (thick * 1.35f, curved, rounded));
        // saturated core
        g.setColour (theme::accent);
        g.strokePath (lit, juce::PathStrokeType (thick * 0.78f, curved, rounded));
        // bright hot inner line
        g.setColour (theme::accentHi.brighter (0.25f).withAlpha (0.95f));
        g.strokePath (lit, juce::PathStrokeType (thick * 0.34f, curved, rounded));

        // ---- hot tip at the live value position ----
        const juce::Point<float> tip (centre.x + std::sin (angle) * ringR,
                                      centre.y - std::cos (angle) * ringR);
        const float tr = thick * (hover ? 2.2f : 1.7f);
        juce::ColourGradient hot (juce::Colours::white.withAlpha (hover ? 1.0f : 0.9f), tip.x, tip.y,
                                  juce::Colours::transparentWhite, tip.x, tip.y + tr, true);
        hot.addColour (0.4, theme::accentHi.withAlpha (0.85f));
        g.setGradientFill (hot);
        g.fillEllipse (tip.x - tr, tip.y - tr, tr * 2.0f, tr * 2.0f);
    }

    // Soft white neumorphic dome with a glowing accent value ring. Shadows are
    // sized to fit within the caller's bounds so nothing clips.
    static void drawDome (juce::Graphics& g, juce::Point<float> centre, float radius,
                          float startAngle, float endAngle, float angle, bool hover)
    {
        const float ringThick = juce::jmax (2.5f, radius * 0.10f);
        const float ringR     = radius * 0.80f;
        const float capR      = ringR - ringThick * 0.5f - radius * 0.07f;
        const float glowR     = radius * 0.12f;

        juce::Rectangle<float> disc (centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f);

        // soft cast shadow (within bounds)
        {
            const int blur = (int) juce::jlimit (4.0f, 16.0f, capR * 0.30f);
            juce::Path sp; sp.addEllipse (disc.translated (0.0f, capR * 0.12f));
            juce::DropShadow (juce::Colours::black.withAlpha (0.20f), blur,
                              { 0, (int) (capR * 0.10f) }).drawForPath (g, sp);
        }

        // ring track + glowing value arc
        juce::Path base;
        base.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, startAngle, endAngle, true);
        g.setColour (theme::ringTrack);
        g.strokePath (base, juce::PathStrokeType (ringThick, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        if (angle > startAngle + 0.001f)
        {
            juce::Path arc;
            arc.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, startAngle, angle, true);
            juce::Path arcStroke;
            juce::PathStrokeType (ringThick).createStrokedPath (arcStroke, arc);
            theme::glowPath (g, arcStroke, hover ? 0.8f : 0.45f, (int) (glowR * (hover ? 1.4f : 1.0f)));
            g.setColour (hover ? theme::accentHi : theme::accent);
            g.strokePath (arc, juce::PathStrokeType (ringThick, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        }

        paintWhiteDome (g, disc);
    }

    static void paintWhiteDome (juce::Graphics& g, juce::Rectangle<float> disc)
    {
        const float capR = disc.getWidth() * 0.5f;

        juce::ColourGradient body (juce::Colours::white,
                                   disc.getCentreX(), disc.getY() + capR * 0.62f,
                                   theme::capLo, disc.getCentreX(), disc.getBottom(), true);
        body.addColour (0.6, theme::capMid);
        body.addColour (0.85, juce::Colour (0xffd9dbe4));
        g.setGradientFill (body);
        g.fillEllipse (disc);

        juce::ColourGradient lower (juce::Colours::transparentBlack, disc.getCentreX(), disc.getCentreY(),
                                    juce::Colours::black.withAlpha (0.10f), disc.getCentreX(), disc.getBottom(), false);
        g.setGradientFill (lower);
        g.fillEllipse (disc);

        auto hi = juce::Rectangle<float> (disc.getWidth() * 0.7f, disc.getHeight() * 0.42f)
                      .withCentre ({ disc.getCentreX(), disc.getY() + disc.getHeight() * 0.24f });
        juce::ColourGradient sg (juce::Colours::white.withAlpha (0.9f), hi.getCentreX(), hi.getY(),
                                 juce::Colours::transparentWhite, hi.getCentreX(), hi.getBottom(), false);
        g.setGradientFill (sg);
        g.fillEllipse (hi);

        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawEllipse (disc.reduced (0.7f).translated (0.0f, -0.5f), 1.1f);
        g.setColour (theme::cardLine);
        g.drawEllipse (disc, 1.0f);
    }

    // Full 360-degree sweep: starts at 6 o'clock and ends at 6 o'clock
    // (value 50 -> straight up at 12).
    static constexpr float kStart = juce::MathConstants<float>::pi;         // 6 o'clock
    static constexpr float kEnd   = juce::MathConstants<float>::pi * 3.0f;  // 6 o'clock, once around

    Style style;
    bool showBigValue = true;
    bool plateMode = false;
    juce::Image knobImg;
    juce::Image ringOffImg, ringOnImg, glowImg;
};
