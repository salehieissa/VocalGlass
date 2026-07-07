#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

//==============================================================================
// Vertical level meter (IN / OUT pair in the output card): a recessed capsule
// channel filling upward from the bottom, -60..0 dBFS, with a peak-hold tick.
// Fed dB values by the editor's timer.
//==============================================================================
class VMeter : public juce::Component
{
public:
    void setDb (float db)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (db - kMinDb) / (0.0f - kMinDb));
        float next = t > smoothed ? t : smoothed * 0.86f + t * 0.14f;
        if (next < 1.0e-3f) next = 0.0f;

        if (next >= peak) { peak = next; peakAge = 0; }
        else if (++peakAge > 22) peak = juce::jmax (0.0f, peak - 0.012f);

        if (next == smoothed && peakAge > 40) return;
        smoothed = next;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float radius = juce::jmin (r.getWidth() * 0.5f, 7.0f);

        theme::recess (g, r, radius);

        if (smoothed > 0.004f)
        {
            auto fill = r.withTrimmedTop (r.getHeight() * (1.0f - smoothed));
            juce::ColourGradient ag (theme::accentHi, fill.getX(), fill.getY(),
                                     theme::accentLo, fill.getX(), fill.getBottom(), false);
            g.setGradientFill (ag);
            g.fillRoundedRectangle (fill, radius);
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.drawLine (fill.getX() + radius, fill.getY() + 0.8f,
                        fill.getRight() - radius, fill.getY() + 0.8f, 1.0f);
        }

        if (peak > 0.01f)
        {
            const float y = r.getY() + r.getHeight() * (1.0f - peak);
            g.setColour (theme::accent.withAlpha (0.85f));
            g.fillRect (r.getX() + 1.5f, y - 1.0f, r.getWidth() - 3.0f, 2.0f);
        }
    }

private:
    static constexpr float kMinDb = -60.0f;
    float smoothed = 0.0f;
    float peak = 0.0f;
    int   peakAge = 0;
};
