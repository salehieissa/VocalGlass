#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

//==============================================================================
// The big circular "VOCALKNOB" hero dial: a large soft white neumorphic dome
// with a glowing pink value ring, a spaced title, a dark-ink percentage readout
// with an accent unit, and the current mode word beneath. Drag to change.
//==============================================================================
class KnobDial : public juce::Slider,
                 private juce::Timer
{
public:
    KnobDial()
    {
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setRotaryParameters (kStart, kEnd, true);
        startTimerHz (30);
    }

    void setCaption (const juce::String& c) { caption = c; }

    // Soft white domed face — the core of the family look at hero scale.
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
        auto bounds = getLocalBounds().toFloat().reduced (24.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();

        const double range = getMaximum() - getMinimum();
        const float prop = range > 0.0 ? (float) ((getValue() - getMinimum()) / range) : 0.0f;
        const float val = kStart + prop * (kEnd - kStart);

        const float ringThick = juce::jmax (10.0f, radius * 0.11f);
        const float ringR = radius * 0.86f;
        const float capR  = ringR - ringThick * 0.5f - radius * 0.06f;
        const float pulse = 0.5f + 0.5f * std::sin (phase);

        juce::Rectangle<float> disc (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);

        // ---- soft cast shadow (within bounds)
        {
            juce::Path sp; sp.addEllipse (disc.translated (0.0f, capR * 0.10f));
            juce::DropShadow (juce::Colours::black.withAlpha (0.18f), 18, { 0, 7 }).drawForPath (g, sp);
        }

        // ---- ring track + glowing value arc
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
            theme::glowPath (g, arcStroke, 0.35f + 0.2f * pulse, 16);
            g.setColour (theme::accent);
            g.strokePath (arc, juce::PathStrokeType (ringThick, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        }

        // ---- white domed face
        KnobDial::paintDome (g, disc);

        // ---- spaced "VOCALKNOB" label
        theme::spacedText (g, "VOCALKNOB",
                           juce::Rectangle<float> (c.x - radius * 0.6f, c.y - radius * 0.34f,
                                                   radius * 1.2f, radius * 0.14f),
                           theme::inkSoft, radius * 0.085f, 3.0f, true,
                           juce::Justification::centred);

        // ---- big number + accent %
        const int pct = juce::roundToInt (prop * 100.0f);
        auto bigF = theme::font (radius * 0.52f, true);
        auto pctF = theme::font (radius * 0.21f, false);
        const juce::String num (pct);
        const float nw = juce::GlyphArrangement::getStringWidth (bigF, num);
        const float pw = juce::GlyphArrangement::getStringWidth (pctF, "%");
        const float gap = 5.0f;
        const float total = nw + gap + pw;
        const float startX = c.x - total * 0.5f;
        const float bh = bigF.getHeight();

        g.setColour (theme::ink);
        g.setFont (bigF);
        g.drawText (num, juce::Rectangle<float> (startX, c.y - bh * 0.55f, nw + 2.0f, bh * 1.1f),
                    juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.setFont (pctF);
        g.drawText ("%", juce::Rectangle<float> (startX + nw + gap, c.y - bh * 0.1f, pw + 8.0f, bh * 0.45f),
                    juce::Justification::centredLeft);

        // ---- mode caption
        g.setColour (theme::inkSoft);
        g.setFont (theme::font (radius * 0.13f, false));
        g.drawText (caption, juce::Rectangle<float> (c.x - radius * 0.7f, c.y + radius * 0.42f,
                                                     radius * 1.4f, radius * 0.22f),
                    juce::Justification::centred);
    }

private:
    void timerCallback() override
    {
        phase += 0.13f;
        if (phase > juce::MathConstants<float>::twoPi) phase -= juce::MathConstants<float>::twoPi;
        repaint();
    }

    static constexpr float kStart = juce::MathConstants<float>::pi * 1.2f;
    static constexpr float kEnd   = juce::MathConstants<float>::pi * 2.8f;

    juce::String caption;
    float phase = 0.0f;
};
