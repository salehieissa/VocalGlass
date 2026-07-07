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
    enum class Icon { Prev, Next, Save };

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

        if (icon == Icon::Save)
        {
            // floppy-disk glyph: body with a clipped corner, shutter + label
            const float s = 5.5f;
            juce::Path p;
            p.startNewSubPath (c.x - s, c.y - s);
            p.lineTo (c.x + s - 3.0f, c.y - s);
            p.lineTo (c.x + s, c.y - s + 3.0f);
            p.lineTo (c.x + s, c.y + s);
            p.lineTo (c.x - s, c.y + s);
            p.closeSubPath();
            g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::mitered,
                                                   juce::PathStrokeType::rounded));
            g.drawRect (juce::Rectangle<float> (c.x - 2.0f, c.y - s, 4.0f, 3.2f), 1.2f);
            g.fillRoundedRectangle (c.x - 3.2f, c.y + 0.6f, 6.4f, 3.8f, 1.0f);
            return;
        }

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
