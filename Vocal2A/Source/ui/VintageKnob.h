#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include <cmath>

//==============================================================================
// VintageKnob — a soft white neumorphic dome knob with a glowing hot-pink value
// ring. No indicator dots or pointer: the lit ring shows position. The Large
// style optionally shows a big value number above the dome.
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
    }

    void setBigValueVisible (bool v) { showBigValue = v; }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();

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

        drawDome (g, c, radius, kStart, kEnd, angle, isMouseOverOrDragging (true));
    }

private:
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

    // 300-degree sweep, symmetric around 12 o'clock (value 50 -> straight up).
    static constexpr float kStart = -juce::MathConstants<float>::pi * 5.0f / 6.0f; // -150 deg
    static constexpr float kEnd   =  juce::MathConstants<float>::pi * 5.0f / 6.0f; // +150 deg

    Style style;
    bool showBigValue = true;
};
