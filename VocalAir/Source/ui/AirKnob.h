#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "Icons.h"
#include "../../../common/ui/Skin.h"

//==============================================================================
// A large soft white neumorphic dome knob: top-lit domed face, a glowing pink
// value ring (no dots/ticks), a big centred value, the caption ("mid air" /
// "high air") beneath it and a small chain glyph under that.
//==============================================================================
class AirKnob : public juce::Slider
{
public:
    AirKnob()
    {
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setRotaryParameters (kStart, kEnd, true);
    }

    void setCaption (const juce::String& c) { caption = c; }

    // Plate mode: the baked chassis carries the ring groove and glow, so the
    // knob paints only the rotating brushed-steel dome sprite — no text.
    // Sweep is 6-to-6, once around.
    void setPlateMode (bool p)
    {
        plateMode = p;
        if (plateMode)
        {
            knobImg = skin::cropToDome (skin::image ("grit-knob-large@2x.png"),
                                        0.1999f, 0.3533f, 0.199f);
            setRotaryParameters (juce::MathConstants<float>::pi,
                                 juce::MathConstants<float>::pi * 3.0f, true);
        }
    }

    // Soft white domed face lit from the top — the core of the look. Shared by
    // the mini knob so both read as the same material at any scale.
    static void paintDome (juce::Graphics& g, juce::Rectangle<float> disc)
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

    void paint (juce::Graphics& g) override
    {
        if (plateMode)
        {
            if (knobImg.isValid())
            {
                const double range = getMaximum() - getMinimum();
                const float prop = range > 0.0 ? (float) ((getValue() - getMinimum()) / range) : 0.0f;
                const float angle = juce::MathConstants<float>::pi * (1.0f + 2.0f * prop);
                skin::drawKnobRotated (g, knobImg, getLocalBounds().toFloat(), angle);
            }
            return;
        }

        auto bounds = getLocalBounds().toFloat().reduced (10.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto  c = bounds.getCentre();
        const bool  hover = isMouseOverOrDragging (true);

        const double range = getMaximum() - getMinimum();
        const float prop = range > 0.0 ? (float) ((getValue() - getMinimum()) / range) : 0.0f;
        const float val  = kStart + prop * (kEnd - kStart);

        const float ringThick = juce::jmax (3.0f, radius * 0.085f);
        const float ringR     = radius - 8.0f;
        const float capR      = ringR - ringThick * 0.5f - radius * 0.06f;
        const float glowR     = radius * 0.12f;

        juce::Rectangle<float> disc (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);

        // ----- soft cast shadow (within bounds) -----
        {
            juce::Path sp; sp.addEllipse (disc.translated (0.0f, capR * 0.10f));
            juce::DropShadow (juce::Colours::black.withAlpha (0.18f), 18, { 0, 7 }).drawForPath (g, sp);
        }

        // ----- ring track + glowing value arc -----
        juce::Path base;
        base.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, kStart, kEnd, true);
        g.setColour (theme::ringTrack);
        g.strokePath (base, juce::PathStrokeType (ringThick, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        if (val > kStart + 0.001f)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, kStart, val, true);
            juce::Path arcStroke;
            juce::PathStrokeType (ringThick).createStrokedPath (arcStroke, arc);
            theme::glowPath (g, arcStroke, hover ? 0.8f : 0.45f, (int) (glowR * (hover ? 1.4f : 1.0f)));
            g.setColour (hover ? theme::accentHi : theme::accent);
            g.strokePath (arc, juce::PathStrokeType (ringThick, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        }

        // ----- white domed face -----
        paintDome (g, disc);

        // ----- big number -----
        const int value = juce::roundToInt (getValue());
        auto bigF = theme::font (radius * 0.46f, false);
        g.setColour (theme::ink);
        g.setFont (bigF);
        g.drawText (juce::String (value),
                    juce::Rectangle<float> (c.x - radius, c.y - radius * 0.42f,
                                            radius * 2.0f, radius * 0.6f),
                    juce::Justification::centred);

        // ----- caption -----
        g.setColour (theme::inkSoft);
        g.setFont (theme::font (radius * 0.16f, false));
        g.drawText (caption,
                    juce::Rectangle<float> (c.x - radius, c.y + radius * 0.18f,
                                            radius * 2.0f, radius * 0.28f),
                    juce::Justification::centred);

        // ----- chain glyph -----
        const float gw = radius * 0.22f;
        icons::chain (g, juce::Rectangle<float> (c.x - gw * 0.5f, c.y + radius * 0.46f,
                                                 gw, gw * 0.55f),
                      theme::inkSoft, 1.3f);
    }

private:
    static constexpr float kStart = juce::MathConstants<float>::pi * 1.25f;
    static constexpr float kEnd   = juce::MathConstants<float>::pi * 2.75f;

    juce::String caption;
    bool plateMode = false;
    juce::Image knobImg;
};

//==============================================================================
// The small "trim" knob: a soft white dome with a glowing pink value ring and
// the value centred. No indicator dots.
//==============================================================================
class MiniKnob : public juce::Slider
{
public:
    MiniKnob()
    {
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setRotaryParameters (kStart, kEnd, true);
    }

    void setPlateMode (bool p)
    {
        plateMode = p;
        if (plateMode)
        {
            knobImg = skin::cropToDome (skin::image ("grit-knob-small@2x.png"),
                                        0.4993f, 0.4648f, 0.615f);
            setRotaryParameters (juce::MathConstants<float>::pi,
                                 juce::MathConstants<float>::pi * 3.0f, true);
        }
    }

    void paint (juce::Graphics& g) override
    {
        if (plateMode)
        {
            if (knobImg.isValid())
            {
                const double range = getMaximum() - getMinimum();
                const float prop = range > 0.0 ? (float) ((getValue() - getMinimum()) / range) : 0.0f;
                const float angle = juce::MathConstants<float>::pi * (1.0f + 2.0f * prop);
                skin::drawKnobRotated (g, knobImg, getLocalBounds().toFloat(), angle);
            }
            return;
        }

        auto bounds = getLocalBounds().toFloat().reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto  c = bounds.getCentre();
        const bool  hover = isMouseOverOrDragging (true);

        const double range = getMaximum() - getMinimum();
        const float prop = range > 0.0 ? (float) ((getValue() - getMinimum()) / range) : 0.0f;
        const float val  = kStart + prop * (kEnd - kStart);

        const float ringThick = juce::jmax (2.5f, radius * 0.11f);
        const float ringR     = radius - 3.0f;
        const float capR      = ringR - ringThick * 0.5f - radius * 0.07f;
        const float glowR     = radius * 0.14f;

        juce::Rectangle<float> disc (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);

        {
            juce::Path sp; sp.addEllipse (disc.translated (0.0f, capR * 0.12f));
            juce::DropShadow (juce::Colours::black.withAlpha (0.18f),
                              (int) juce::jlimit (4.0f, 12.0f, capR * 0.4f), { 0, 4 }).drawForPath (g, sp);
        }

        juce::Path base;
        base.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, kStart, kEnd, true);
        g.setColour (theme::ringTrack);
        g.strokePath (base, juce::PathStrokeType (ringThick, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        if (val > kStart + 0.001f)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, kStart, val, true);
            juce::Path arcStroke;
            juce::PathStrokeType (ringThick).createStrokedPath (arcStroke, arc);
            theme::glowPath (g, arcStroke, hover ? 0.8f : 0.45f, (int) (glowR * (hover ? 1.4f : 1.0f)));
            g.setColour (hover ? theme::accentHi : theme::accent);
            g.strokePath (arc, juce::PathStrokeType (ringThick, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        }

        AirKnob::paintDome (g, disc);

        const int value = juce::roundToInt (getValue());
        g.setColour (theme::ink);
        g.setFont (theme::font (capR * 0.7f, false));
        g.drawText (juce::String (value),
                    juce::Rectangle<float> (c.x - radius, c.y - radius * 0.55f,
                                            radius * 2.0f, radius * 1.1f),
                    juce::Justification::centred);
    }

private:
    static constexpr float kStart = juce::MathConstants<float>::pi * 1.25f;
    static constexpr float kEnd   = juce::MathConstants<float>::pi * 2.75f;

    bool plateMode = false;
    juce::Image knobImg;
};
