#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

//==============================================================================
// The big circular "GRIT" hero dial: a large dark glossy disc with a glowing
// accent ring and a white percentage readout. Drag to change (rotary).
//==============================================================================
class GritDial : public juce::Slider,
                 private juce::Timer
{
public:
    GritDial()
    {
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                             juce::MathConstants<float>::pi * 2.8f, true);
        startTimerHz (30);
    }

    void setCaption (const juce::String& c) { caption = c; }

    // Soft white domed face (matches the small knobs' look at hero scale).
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
        auto bounds = getLocalBounds().toFloat().reduced (26.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();

        const double range = getMaximum() - getMinimum();
        const float prop = range > 0.0 ? (float) ((getValue() - getMinimum()) / range) : 0.0f;

        const float start = juce::MathConstants<float>::pi * 1.2f;
        const float end   = juce::MathConstants<float>::pi * 2.8f;
        const float val   = start + prop * (end - start);
        const float ringThick = 14.0f;
        const float ringR = radius * 0.86f;
        const float capR  = ringR - ringThick * 0.5f - 8.0f;
        const float pulse = 0.5f + 0.5f * std::sin (phase);

        juce::Rectangle<float> disc (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);

        // ---- soft cast shadow (within bounds)
        {
            juce::Path sp; sp.addEllipse (disc.translated (0.0f, capR * 0.10f));
            juce::DropShadow (juce::Colours::black.withAlpha (0.18f), 18, { 0, 7 }).drawForPath (g, sp);
        }

        // ---- ring track + glowing value arc
        juce::Path base;
        base.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, start, end, true);
        g.setColour (theme::ringTrack);
        g.strokePath (base, juce::PathStrokeType (ringThick, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        if (val > start + 0.001f)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, start, val, true);
            juce::Path arcStroke;
            juce::PathStrokeType (ringThick).createStrokedPath (arcStroke, arc);
            theme::glowPath (g, arcStroke, 0.35f + 0.2f * pulse, 16);
            g.setColour (theme::accent);
            g.strokePath (arc, juce::PathStrokeType (ringThick, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        }

        // ---- white domed face
        GritDial::paintDome (g, disc);

        // ---- "GRIT" label
        theme::spacedText (g, "GRIT",
                           juce::Rectangle<float> (c.x - 80.0f, c.y - radius * 0.34f, 160.0f, 20.0f),
                           theme::inkSoft, 12.5f, 3.0f, true, juce::Justification::centred);

        // ---- big number + %
        const int pct = juce::roundToInt (prop * 100.0f);
        auto bigF = theme::font (52.0f, true);
        auto pctF = theme::font (22.0f, false);
        const juce::String num (pct);
        const float nw = juce::GlyphArrangement::getStringWidth (bigF, num);
        const float pw = juce::GlyphArrangement::getStringWidth (pctF, "%");
        const float gap = 5.0f;
        const float total = nw + gap + pw;
        const float startX = c.x - total * 0.5f;

        g.setColour (theme::ink);
        g.setFont (bigF);
        g.drawText (num, juce::Rectangle<float> (startX, c.y - 34.0f, nw + 2.0f, 64.0f),
                    juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.setFont (pctF);
        g.drawText ("%", juce::Rectangle<float> (startX + nw + gap, c.y - 6.0f, pw + 6.0f, 30.0f),
                    juce::Justification::centredLeft);

        // ---- caption
        g.setColour (theme::inkSoft);
        g.setFont (theme::font (11.5f, false));
        g.drawFittedText (caption,
                          juce::Rectangle<float> (c.x - capR * 0.84f, c.y + radius * 0.24f,
                                                  capR * 1.68f, 30.0f).toNearestInt(),
                          juce::Justification::centredTop, 2);
    }

private:
    void timerCallback() override
    {
        phase += 0.13f;
        if (phase > juce::MathConstants<float>::twoPi) phase -= juce::MathConstants<float>::twoPi;
        repaint();
    }

    juce::String caption;
    float phase = 0.0f;
};
