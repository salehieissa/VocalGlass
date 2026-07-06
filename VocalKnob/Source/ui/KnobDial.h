#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "../../../common/ui/Skin.h"

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

    // Plate mode: the baked chassis carries the ring groove and glow, so the
    // dial paints only the rotating brushed-steel dome sprite (shared with
    // grit) — no text or numbers on the dome. Sweep is 6-to-6, once around.
    void setPlateMode (bool p)
    {
        plateMode = p;
        if (plateMode)
        {
            knobImg = skin::cropToDome (skin::image ("grit-knob-large@2x.png"),
                                        0.1999f, 0.3533f, 0.199f);
            setRotaryParameters (juce::MathConstants<float>::pi,
                                 juce::MathConstants<float>::pi * 3.0f, true);
            stopTimer();
        }
    }

    // Soft white domed face — the core of the family look at hero scale.
    static void paintDome (juce::Graphics& g, juce::Rectangle<float> disc)
    {
        const float capR = disc.getWidth() * 0.5f;

        // Porcelain white: brightest high-centre, only a thin cool shade hugging
        // the very bottom edge so the cap stays luminous rather than greying out.
        juce::ColourGradient body (juce::Colours::white,
                                   disc.getCentreX(), disc.getY() + capR * 0.62f,
                                   theme::capLo, disc.getCentreX(), disc.getBottom(), true);
        body.addColour (0.74, juce::Colour (0xfff8f9fc));
        body.addColour (0.93, juce::Colour (0xffeaebf0));
        g.setGradientFill (body);
        g.fillEllipse (disc);

        // a very soft lower crescent so it reads as gently convex (kept subtle)
        juce::ColourGradient lower (juce::Colours::transparentBlack, disc.getCentreX(), disc.getCentreY(),
                                    juce::Colours::black.withAlpha (0.05f), disc.getCentreX(), disc.getBottom(), false);
        g.setGradientFill (lower);
        g.fillEllipse (disc);

        // faint pink bounce: the glowing arc (lower-left in this design) reflects
        // a touch of colour onto the white cap. This is a big part of the render.
        {
            const juce::Point<float> pr (disc.getX() + capR * 0.40f, disc.getBottom() - capR * 0.40f);
            juce::ColourGradient pg (theme::accent.withAlpha (0.11f), pr.x, pr.y,
                                     juce::Colours::transparentBlack, pr.x, pr.y - capR * 0.95f, true);
            g.setGradientFill (pg);
            g.fillEllipse (disc);
        }

        // top specular catch
        auto hi = juce::Rectangle<float> (disc.getWidth() * 0.7f, disc.getHeight() * 0.42f)
                      .withCentre ({ disc.getCentreX(), disc.getY() + disc.getHeight() * 0.24f });
        juce::ColourGradient sg (juce::Colours::white.withAlpha (0.9f), hi.getCentreX(), hi.getY(),
                                 juce::Colours::transparentWhite, hi.getCentreX(), hi.getBottom(), false);
        g.setGradientFill (sg);
        g.fillEllipse (hi);

        // rim: only a soft bright top hairline — no hard outline, so the surface
        // stays porcelain-smooth and is separated from the panel purely by shadow.
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawEllipse (disc.reduced (0.6f).translated (0.0f, -0.4f), 1.0f);
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

        auto bounds = getLocalBounds().toFloat().reduced (24.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();

        const double range = getMaximum() - getMinimum();
        const float prop = range > 0.0 ? (float) ((getValue() - getMinimum()) / range) : 0.0f;
        const float val = kStart + prop * (kEnd - kStart);

        const float ringThick = juce::jmax (12.0f, radius * 0.13f);
        const float ringR = radius * 0.86f;
        const float capR  = ringR - ringThick * 0.5f - radius * 0.06f;
        const float pulse = 0.5f + 0.5f * std::sin (phase);

        juce::Rectangle<float> disc (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);

        // ---- layered cast shadow: a wide ambient bloom plus a tighter, darker
        // contact shadow, both thrown down-right so the dome reads as a solid
        // object lifting off the panel (matches the photoreal reference).
        {
            const int ambBlur = (int) juce::jlimit (24.0f, 46.0f, capR * 0.50f);
            const int conBlur = (int) juce::jlimit (10.0f, 22.0f, capR * 0.22f);
            const juce::Colour sh (0xff1c2030);
            juce::Path sp; sp.addEllipse (disc);
            juce::DropShadow (sh.withAlpha (0.11f), ambBlur,
                              { (int) (capR * 0.06f), (int) (capR * 0.15f) }).drawForPath (g, sp);
            juce::DropShadow (sh.withAlpha (0.15f), conBlur,
                              { (int) (capR * 0.03f), (int) (capR * 0.08f) }).drawForPath (g, sp);
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
            // two-stage glow: a wide soft halo that bleeds onto the panel, plus a
            // tighter bright bloom hugging the arc — the signature look.
            theme::glowPath (g, arcStroke, 0.22f, (int) juce::jmax (30.0f, radius * 0.36f));
            theme::glowPath (g, arcStroke, 0.55f + 0.25f * pulse, 22);
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
    bool plateMode = false;
    juce::Image knobImg;
};
