#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

//==============================================================================
// Soft white neumorphic dome knob with a glowing pink value ring (matches the
// VocalGrit look). No indicator dots — the lit ring shows position. The ring
// brightens and the glow grows on hover / drag. Geometry and rotary range are
// unchanged; only the rendering is the premium dome style.
//==============================================================================
class RingKnob : public juce::Slider
{
public:
    RingKnob (float arcThickness = 3.0f, bool ticks = false)
        : thickness (arcThickness), drawTicks (ticks)
    {
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setRotaryParameters (kStart, kEnd, true);
    }

    // When set, the lit ring fills in reverse: the minimum value draws a FULL arc
    // (ending on the right) and the maximum draws empty. Used for "retune speed"
    // where 0 = hardest/fastest should read as a full ring on the right.
    void setInvertedFill (bool shouldInvert) { invertFill = shouldInvert; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (5.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
        const auto  c = bounds.getCentre();

        const double range = getMaximum() - getMinimum();
        float prop = range > 0.0 ? (float) ((getValue() - getMinimum()) / range) : 0.0f;
        if (invertFill) prop = 1.0f - prop;
        const float angle = kStart + prop * (kEnd - kStart);
        const bool hover = isMouseOverOrDragging (true);

        const float ringThick = juce::jmax (thickness, radius * 0.10f);
        const float ringR     = radius * 0.80f;
        const float capR      = ringR - ringThick * 0.5f - radius * 0.07f;
        const float glowR     = radius * 0.12f;

        juce::Rectangle<float> disc (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);

        // ---- soft cast shadow (kept well within bounds)
        {
            const int blur = (int) juce::jlimit (4.0f, 16.0f, capR * 0.30f);
            juce::Path sp; sp.addEllipse (disc.translated (0.0f, capR * 0.12f));
            juce::DropShadow (juce::Colours::black.withAlpha (0.20f), blur,
                              { 0, (int) (capR * 0.10f) }).drawForPath (g, sp);
        }

        // ---- ring track + glowing value arc
        juce::Path base;
        base.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, kStart, kEnd, true);
        g.setColour (theme::ringTrack);
        g.strokePath (base, juce::PathStrokeType (ringThick, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        if (angle > kStart + 0.001f)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, kStart, angle, true);
            juce::Path arcStroke;
            juce::PathStrokeType (ringThick).createStrokedPath (arcStroke, arc);
            theme::glowPath (g, arcStroke, hover ? 0.8f : 0.45f, (int) (glowR * (hover ? 1.4f : 1.0f)));
            g.setColour (hover ? theme::accentHi : theme::accent);
            g.strokePath (arc, juce::PathStrokeType (ringThick, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        }

        paintWhiteDome (g, disc);
        juce::ignoreUnused (drawTicks);
    }

private:
    // A soft, domed white knob face lit from the top — the core of the look.
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

    static constexpr float kStart = juce::MathConstants<float>::pi * 1.25f;
    static constexpr float kEnd   = juce::MathConstants<float>::pi * 2.75f;

    float thickness;
    bool  drawTicks;
    bool  invertFill = false;
};
