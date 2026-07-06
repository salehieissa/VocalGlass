#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

//==============================================================================
// Small circular icon button (preset prev / next steppers). Same style as the
// suite's round icon buttons (VocalTune).
//==============================================================================
class IconButton : public juce::Button
{
public:
    enum class Icon { Prev, Next };

    explicit IconButton (Icon ic, bool framed = true)
        : juce::Button ({}), icon (ic), drawFrame (framed) {}

    bool plate = false;   // baked into the chassis art; keep only the hit area

    void paintButton (juce::Graphics& g, bool highlighted, bool) override
    {
        if (plate) return;

        auto b = getLocalBounds().toFloat().reduced (2.0f);
        const float d = juce::jmin (b.getWidth(), b.getHeight());
        auto circle = juce::Rectangle<float> (b.getCentreX() - d * 0.5f,
                                              b.getCentreY() - d * 0.5f, d, d);

        if (drawFrame)
        {
            g.setColour (juce::Colours::white);
            g.fillEllipse (circle);
            g.setColour (highlighted ? theme::accent : theme::cardLine);
            g.drawEllipse (circle, 1.2f);
        }

        g.setColour (theme::ink);
        auto c = circle.getCentre();
        juce::PathStrokeType stroke (1.6f, juce::PathStrokeType::curved,
                                     juce::PathStrokeType::rounded);

        const float dir = (icon == Icon::Prev) ? 1.0f : -1.0f;   // apex points outward: '<' prev, '>' next
        juce::Path p;
        p.startNewSubPath (c.x + dir * 3.0f, c.y - 5.0f);
        p.lineTo (c.x - dir * 3.0f, c.y);
        p.lineTo (c.x + dir * 3.0f, c.y + 5.0f);
        g.strokePath (p, stroke);
    }

private:
    Icon icon;
    bool drawFrame;
};
