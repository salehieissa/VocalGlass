#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

//==============================================================================
// Tiny horizontal activity meter used inside module rows (gain reduction for
// gate / de-ess / comp, clip amount for the clipper). A recessed capsule that
// fills left-to-right with the accent gradient. Fed dB values by the editor's
// timer; smoothing keeps it calm.
//==============================================================================
class ActivityBar : public juce::Component
{
public:
    // fullScaleDb: the dB value that maps to a 100% filled bar.
    explicit ActivityBar (float fullScaleDb = 12.0f) : maxDb (fullScaleDb) {}

    void setDb (float db)
    {
        const float t = juce::jlimit (0.0f, 1.0f, db / maxDb);
        float next = t > smoothed ? t : smoothed * 0.80f + t * 0.20f;
        if (next < 1.0e-3f) next = 0.0f;
        if (next == smoothed) return;
        smoothed = next;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float radius = r.getHeight() * 0.5f;

        theme::recess (g, r, radius);

        if (smoothed > 0.004f)
        {
            auto fill = r.withWidth (juce::jmax (r.getHeight(), r.getWidth() * smoothed));
            juce::Path fp; fp.addRoundedRectangle (fill, radius);
            theme::glowPath (g, fp, 0.20f, 6);
            juce::ColourGradient ag (theme::accentHi, fill.getX(), fill.getY(),
                                     theme::accentLo, fill.getX(), fill.getBottom(), false);
            g.setGradientFill (ag);
            g.fillRoundedRectangle (fill, radius);
        }
    }

private:
    float smoothed = 0.0f;
    float maxDb;
};
