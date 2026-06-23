#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

//==============================================================================
// A thin vertical level meter bar. Light track with an active fill rising from
// the bottom — dark at the top fading to hot pink at the bottom — driven by a
// level in dB (-60 .. 0).
//==============================================================================
class MeterBar : public juce::Component
{
public:
    void setLevelDb (float db)
    {
        const float target = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        if (std::abs (target - level) > 0.001f)
        {
            level = target;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float radius = r.getWidth() * 0.5f;

        // recessed well
        theme::recess (g, r, radius);

        if (level <= 0.001f) return;

        const float h = r.getHeight() * level;
        auto fill = r.withTop (r.getBottom() - h);

        juce::Path fp; fp.addRoundedRectangle (fill, radius);
        theme::glowPath (g, fp, 0.22f, 6);

        // green (quiet, bottom) -> orange -> accent (loud, top) like a real meter
        juce::ColourGradient grad (juce::Colour (0xff27d0a8), r.getCentreX(), r.getBottom(),
                                   theme::accent, r.getCentreX(), r.getY(), false);
        grad.addColour (0.7, juce::Colour (0xfff0a83a));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, radius);

        g.setColour (juce::Colours::white.withAlpha (0.30f));
        g.drawLine (fill.getCentreX() - radius * 0.4f, fill.getY() + 0.8f,
                    fill.getCentreX() + radius * 0.4f, fill.getY() + 0.8f, 1.0f);
    }

private:
    float level = 0.0f;
};
