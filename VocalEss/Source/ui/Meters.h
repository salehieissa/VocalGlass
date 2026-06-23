#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

//==============================================================================
// Floating value bubble (threshold readout). Paints its own rounded background
// so it can sit ON TOP of the meter bars without being clipped behind them.
//==============================================================================
class Bubble : public juce::Component
{
public:
    void setText (const juce::String& t) { text = t; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const float radius = r.getHeight() * 0.5f;
        juce::DropShadow (juce::Colours::black.withAlpha (0.16f), 8, { 0, 2 })
            .drawForPath (g, [&] { juce::Path p; p.addRoundedRectangle (r, radius); return p; }());
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (r, radius);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (r, radius, 1.0f);
        g.setColour (theme::accent);
        g.setFont (theme::font (13.0f, true));
        g.drawText (text, getLocalBounds(), juce::Justification::centred);
    }

private:
    juce::String text;
};

//==============================================================================
// Vertical capsule meter. Maps a dB value into a 0..1 fill over [minDb, maxDb].
// fromTop = true fills downward from the top (gain-reduction style); otherwise
// it fills upward from the bottom (level style).
//==============================================================================
class Bar : public juce::Component
{
public:
    Bar (float minDb_, float maxDb_, bool fromTop_)
        : minDb (minDb_), maxDb (maxDb_), fromTop (fromTop_) {}

    void setLevelDb (float db)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
        // light smoothing for a calmer meter
        smoothed = t > smoothed ? t : smoothed * 0.82f + t * 0.18f;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float radius = r.getWidth() * 0.5f;

        // recessed gradient well (matches the LevelMeter look)
        theme::recess (g, r, radius);

        if (smoothed <= 0.001f)
            return;

        const float h = r.getHeight() * smoothed;
        auto fill = fromTop ? r.withHeight (h)
                            : r.withTop (r.getBottom() - h);

        juce::Path fp; fp.addRoundedRectangle (fill, radius);
        theme::glowPath (g, fp, 0.22f, 8);

        // green (quiet, bottom) -> accent (loud, top) so level reads hot
        juce::ColourGradient ag (theme::accent, r.getX(), r.getY(),
                                 juce::Colour (0xff27d0a8), r.getX(), r.getBottom(), false);
        ag.addColour (0.3, juce::Colour (0xfff0a83a));
        g.setGradientFill (ag);
        g.fillRoundedRectangle (fill, radius);

        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.drawLine (fill.getX() + radius, fill.getY() + 0.8f,
                    fill.getRight() - radius, fill.getY() + 0.8f, 1.0f);
    }

private:
    float minDb, maxDb;
    bool  fromTop;
    float smoothed = 0.0f;
};
