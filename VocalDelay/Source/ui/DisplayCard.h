#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

//==============================================================================
// The centre read-out card: a big tempo number with a unit beneath, "L"/"R"
// tags in the corners and animated dotted delay-tap visualisers on each side.
//==============================================================================
class DisplayCard : public juce::Component,
                    private juce::Timer
{
public:
    DisplayCard() { startTimerHz (24); }

    void setBigText  (const juce::String& t) { bigText = t; }
    void setUnitText (const juce::String& t) { unitText = t; }
    void setTaps     (float fb)              { decay = juce::jlimit (0.2f, 0.97f, 0.4f + fb * 0.55f); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();

        // Floating-card surface (the soft cast shadow is drawn by the editor
        // behind this component so it never clips against our own bounds).
        g.setColour (theme::card);
        g.fillRoundedRectangle (r, 14.0f);
        theme::topHighlight (g, r, 14.0f);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (r, 14.0f, 1.0f);

        // L / R corner tags
        g.setColour (theme::accent);
        g.setFont (theme::font (15.0f, true));
        g.drawText ("L", (int) r.getX() + 14, (int) r.getY() + 10, 24, 18, juce::Justification::centredLeft);
        g.drawText ("R", (int) r.getRight() - 38, (int) r.getY() + 10, 24, 18, juce::Justification::centredRight);

        // dotted visualisers flanking the number
        const float cy = r.getCentreY();
        drawTaps (g, r.getX() + 14.0f, cy, true);
        drawTaps (g, r.getRight() - 14.0f, cy, false);

        // big number + unit
        g.setColour (theme::ink);
        g.setFont (theme::font (juce::jmin (66.0f, r.getHeight() * 0.46f), true));
        g.drawText (bigText, r.withTrimmedBottom (r.getHeight() * 0.22f),
                    juce::Justification::centred);

        g.setColour (theme::inkSoft);
        g.setFont (theme::font (15.0f, false));
        g.drawText (unitText, r.withTrimmedTop (r.getHeight() * 0.70f),
                    juce::Justification::centred);
    }

private:
    void drawTaps (juce::Graphics& g, float originX, float cy, bool toRight)
    {
        const int cols = 6, rows = 5;
        const float dot = 2.4f;
        const float dx = 9.0f * (toRight ? 1.0f : -1.0f);
        const float dy = 8.0f;

        for (int cIdx = 0; cIdx < cols; ++cIdx)
        {
            const float colHeight = std::pow (decay, (float) cIdx); // echoes decay outward
            const int visibleRows = juce::jmax (1, (int) std::round (colHeight * rows));
            const float pulse = 0.45f + 0.55f * std::pow (decay, (float) ((cIdx + tick) % cols));

            for (int rIdx = 0; rIdx < visibleRows; ++rIdx)
            {
                const float x = originX + dx * (float) cIdx;
                const float yUp = cy - dy * (float) rIdx;
                const float yDn = cy + dy * (float) rIdx;
                g.setColour (theme::inkSoft.withAlpha (0.18f + 0.30f * pulse * colHeight));
                g.fillEllipse (juce::Rectangle<float> (dot, dot).withCentre ({ x, yUp }));
                if (rIdx > 0)
                    g.fillEllipse (juce::Rectangle<float> (dot, dot).withCentre ({ x, yDn }));
            }
        }
    }

    void timerCallback() override { tick = (tick + 1) % 6; repaint(); }

    juce::String bigText { "120" }, unitText { "BPM" };
    float decay = 0.7f;
    int   tick = 0;
};
