#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

//==============================================================================
// Small round module power toggle: a circular button with the standard power
// glyph (arc + stem). ON = accent-filled disc with a soft pink glow and white
// glyph; OFF = white disc, hairline outline, grey glyph. Toggle state comes
// from an APVTS ButtonAttachment.
//==============================================================================
class PowerButton : public juce::Button
{
public:
    PowerButton() : juce::Button ({})
    {
        setClickingTogglesState (true);
    }

    bool plate = false;   // baked into the chassis art; keep only the hit area

    void paintButton (juce::Graphics& g, bool highlighted, bool) override
    {
        if (plate) return;

        auto b = getLocalBounds().toFloat();
        const float d = juce::jmin (b.getWidth(), b.getHeight()) - 4.0f;
        auto disc = juce::Rectangle<float> (d, d).withCentre (b.getCentre());
        const bool on = getToggleState();

        if (on)
        {
            juce::Path p; p.addEllipse (disc);
            theme::glowPath (g, p, 0.55f, (int) (d * 0.30f));
            juce::ColourGradient ag (theme::accentHi, disc.getX(), disc.getY(),
                                     theme::accentLo, disc.getX(), disc.getBottom(), false);
            g.setGradientFill (ag);
            g.fillEllipse (disc);
            g.setColour (theme::accentLo.withAlpha (0.55f));
            g.drawEllipse (disc, 1.0f);
        }
        else
        {
            juce::ColourGradient wg (juce::Colours::white, disc.getX(), disc.getY(),
                                     juce::Colour (0xfff0f1f5), disc.getX(), disc.getBottom(), false);
            g.setGradientFill (wg);
            g.fillEllipse (disc);
            g.setColour (highlighted ? theme::accent.withAlpha (0.6f) : theme::cardLine);
            g.drawEllipse (disc, 1.2f);
        }

        // power glyph: open arc + vertical stem through the gap
        const auto c = disc.getCentre();
        const float r = d * 0.26f;
        g.setColour (on ? juce::Colours::white
                        : (highlighted ? theme::accent : theme::inkSoft));
        juce::Path glyph;
        glyph.addCentredArc (c.x, c.y + d * 0.02f, r, r, 0.0f,
                             0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
        g.strokePath (glyph, juce::PathStrokeType (juce::jmax (1.5f, d * 0.09f),
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        g.drawLine (c.x, c.y - r - d * 0.10f, c.x, c.y - r * 0.15f,
                    juce::jmax (1.5f, d * 0.09f));
    }
};
