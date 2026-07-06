#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "Bounce.h"

//==============================================================================
// Small circular chain-link toggle that sits between the two filter knobs.
//==============================================================================
class LinkButton : public Bouncy<juce::Button>
{
public:
    LinkButton() : Bouncy<juce::Button> ("LINK") { setClickingTogglesState (true); }

    bool plate = false;   // baked into the chassis; editor masks the lit state

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        if (plate) return;

        auto r = getLocalBounds().toFloat().reduced (2.0f);
        const float d = juce::jmin (r.getWidth(), r.getHeight());
        auto circle = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
        const bool on = getToggleState();

        if (on)
        {
            g.setColour (theme::accentSoft);
            g.fillEllipse (circle.expanded (2.0f));
            g.setColour (theme::accent);
            g.fillEllipse (circle);
        }
        else
        {
            g.setColour (juce::Colours::white);
            g.fillEllipse (circle);
            g.setColour ((highlighted || down) ? theme::accent : theme::cardLine);
            g.drawEllipse (circle, 1.4f);
        }

        // chain glyph: two interlocking rounded links
        const auto c = circle.getCentre();
        const float lw = d * 0.22f, lh = d * 0.16f;
        g.setColour (on ? juce::Colours::white : theme::accent);
        juce::Rectangle<float> a (lw, lh); a.setCentre (c.x - lw * 0.42f, c.y - lh * 0.42f);
        juce::Rectangle<float> b (lw, lh); b.setCentre (c.x + lw * 0.42f, c.y + lh * 0.42f);
        g.drawRoundedRectangle (a, lh * 0.5f, 1.8f);
        g.drawRoundedRectangle (b, lh * 0.5f, 1.8f);
        g.drawLine (c.x - lw * 0.1f, c.y - lh * 0.1f, c.x + lw * 0.1f, c.y + lh * 0.1f, 1.8f);
    }
};
