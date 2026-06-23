#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "KnobLookAndFeel.h"

//==============================================================================
// A pill button for the top bar. Three flavours:
//   ExternalLink — "Vocal Tips" with an external-link glyph on the left
//   Plain        — "Bypass" (acts as a toggle; pink when engaged)
//   Menu         — a circular 3-dot overflow button
//==============================================================================
class TopBarButton : public juce::Button
{
public:
    enum class Kind { ExternalLink, Plain, Menu };

    TopBarButton (juce::String labelText, Kind k)
        : juce::Button (labelText), label (std::move (labelText)), kind (k)
    {
        if (kind == Kind::Plain) setClickingTogglesState (true);
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool /*down*/) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f);
        const float radius = (kind == Kind::Menu) ? r.getWidth() * 0.5f : r.getHeight() * 0.5f;
        const bool on = getToggleState();

        KnobLookAndFeel::paintPill (g, r, radius, on, highlighted);

        const juce::Colour fg = on ? juce::Colours::white : theme::ink;

        if (kind == Kind::Menu)
        {
            g.setColour (fg);
            const float d = 3.0f;
            const float cx = r.getCentreX();
            for (int i = -1; i <= 1; ++i)
                g.fillEllipse (cx - d * 0.5f, r.getCentreY() + (float) i * 7.0f - d * 0.5f, d, d);
            return;
        }

        auto content = r.reduced (radius * 0.55f, 0.0f);

        if (kind == Kind::ExternalLink)
        {
            auto iconArea = content.removeFromLeft (16.0f);
            drawExternalLink (g, iconArea.withSizeKeepingCentre (13.0f, 13.0f), fg);
            content.removeFromLeft (6.0f);
        }

        g.setColour (fg);
        g.setFont (theme::font (juce::jmin (15.0f, r.getHeight() * 0.42f), false));
        g.drawText (label, content, juce::Justification::centred);
    }

private:
    static void drawExternalLink (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour col)
    {
        g.setColour (col);
        const float t = 1.4f;

        // box (open top-right corner)
        juce::Path box;
        const float x = b.getX(), y = b.getY(), w = b.getWidth(), h = b.getHeight();
        box.startNewSubPath (x + w * 0.55f, y);
        box.lineTo (x, y);
        box.lineTo (x, y + h);
        box.lineTo (x + w, y + h);
        box.lineTo (x + w, y + h * 0.45f);
        g.strokePath (box, juce::PathStrokeType (t, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        // arrow out of the top-right
        g.drawLine (x + w * 0.5f, y + h * 0.5f, x + w + 1.0f, y - 1.0f, t);
        juce::Path head;
        head.startNewSubPath (x + w * 0.6f, y - 1.0f);
        head.lineTo (x + w + 1.0f, y - 1.0f);
        head.lineTo (x + w + 1.0f, y + h * 0.4f);
        g.strokePath (head, juce::PathStrokeType (t, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    juce::String label;
    Kind kind;
};
