#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

//==============================================================================
// Small circular icon button (undo / redo / reset / settings / power / dots).
//==============================================================================
class IconButton : public juce::Button
{
public:
    enum class Icon { Undo, Redo, Reset, Gear, Power, Dots, Prev, Next };

    explicit IconButton (Icon ic, bool framed = true)
        : juce::Button ({}), icon (ic), drawFrame (framed) {}

    void setActiveTint (bool on) { tinted = on; repaint(); }

    // Plate mode: the button circle + icon are baked into the chassis; lit
    // states are masked from the ON plate by the editor.
    bool plate = false;

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

        const auto col = tinted ? theme::accent : theme::ink;
        g.setColour (col);
        auto c = circle.getCentre();
        const float s = d * 0.26f; // icon half-size
        juce::PathStrokeType stroke (1.6f, juce::PathStrokeType::curved,
                                     juce::PathStrokeType::rounded);

        switch (icon)
        {
            case Icon::Undo:
            case Icon::Redo:
            {
                const bool redo = (icon == Icon::Redo);
                juce::Path p;
                const float r = s * 0.9f;
                p.addCentredArc (c.x, c.y, r, r, 0.0f,
                                 juce::degreesToRadians (redo ? 40.0f : -40.0f),
                                 juce::degreesToRadians (redo ? 300.0f : -300.0f), true);
                g.strokePath (p, stroke);
                // arrow head
                const float ax = c.x + (redo ? r : -r) * std::sin (juce::degreesToRadians (40.0f));
                const float ay = c.y - r * std::cos (juce::degreesToRadians (40.0f));
                juce::Path head;
                const float dir = redo ? 1.0f : -1.0f;
                head.startNewSubPath (ax, ay - 4.0f);
                head.lineTo (ax, ay);
                head.lineTo (ax - dir * 4.0f, ay);
                g.strokePath (head, stroke);
                break;
            }
            case Icon::Reset:
            {
                juce::Path p;
                p.addCentredArc (c.x, c.y, s, s, 0.0f,
                                 juce::degreesToRadians (60.0f),
                                 juce::degreesToRadians (330.0f), true);
                g.strokePath (p, stroke);
                const float ax = c.x + s * std::sin (juce::degreesToRadians (60.0f));
                const float ay = c.y - s * std::cos (juce::degreesToRadians (60.0f));
                juce::Path head;
                head.startNewSubPath (ax - 4.0f, ay - 2.0f);
                head.lineTo (ax, ay);
                head.lineTo (ax + 1.0f, ay - 5.0f);
                g.strokePath (head, stroke);
                break;
            }
            case Icon::Gear:
            {
                juce::Path p;
                const int teeth = 8;
                for (int i = 0; i < teeth; ++i)
                {
                    const float a = juce::MathConstants<float>::twoPi * (float) i / (float) teeth;
                    const float x = c.x + s * std::cos (a);
                    const float y = c.y + s * std::sin (a);
                    if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                    const float a2 = a + juce::MathConstants<float>::twoPi / (teeth * 2.0f);
                    p.lineTo (c.x + s * 1.35f * std::cos (a2), c.y + s * 1.35f * std::sin (a2));
                }
                p.closeSubPath();
                g.strokePath (p, juce::PathStrokeType (1.4f));
                g.drawEllipse (c.x - s * 0.4f, c.y - s * 0.4f, s * 0.8f, s * 0.8f, 1.4f);
                break;
            }
            case Icon::Power:
            {
                juce::Path p;
                p.addCentredArc (c.x, c.y + 1.0f, s, s, 0.0f,
                                 juce::degreesToRadians (40.0f),
                                 juce::degreesToRadians (320.0f), true);
                g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
                g.drawLine (c.x, c.y - s - 1.0f, c.x, c.y + 1.0f, 1.8f);
                break;
            }
            case Icon::Dots:
            {
                for (int i = -1; i <= 1; ++i)
                    g.fillEllipse (c.x - 1.6f, c.y + (float) i * 6.0f - 1.6f, 3.2f, 3.2f);
                break;
            }
            case Icon::Prev:
            case Icon::Next:
            {
                const float dir = (icon == Icon::Next) ? 1.0f : -1.0f;
                juce::Path p;
                p.startNewSubPath (c.x + dir * 3.0f, c.y - 5.0f);
                p.lineTo (c.x - dir * 3.0f, c.y);
                p.lineTo (c.x + dir * 3.0f, c.y + 5.0f);
                g.strokePath (p, stroke);
                break;
            }
        }
    }

private:
    Icon icon;
    bool drawFrame;
    bool tinted = false;
};
