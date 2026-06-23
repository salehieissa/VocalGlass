#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

//==============================================================================
// A horizontal level meter: a recessed capsule track with a smooth gradient
// fill, a floating peak cap, and a dB readout.
//==============================================================================
class LevelMeter : public juce::Component,
                   private juce::Timer
{
public:
    explicit LevelMeter (std::atomic<float>& sourceLevel)
        : source (sourceLevel)
    {
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto textArea = r.removeFromRight (60.0f);
        r = r.withSizeKeepingCentre (r.getWidth(), juce::jmin (r.getHeight(), 12.0f));
        const float corner = r.getHeight() * 0.5f;

        // recessed well
        theme::recess (g, r, corner);

        const float db   = juce::Decibels::gainToDecibels (smoothed, -60.0f);
        const float prop = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        const float pkDb = juce::Decibels::gainToDecibels (peak, -60.0f);
        const float pkP  = juce::jlimit (0.0f, 1.0f, (pkDb + 60.0f) / 60.0f);

        if (prop > 0.005f)
        {
            juce::Rectangle<float> fill (r.getX(), r.getY(), r.getWidth() * prop, r.getHeight());
            juce::Path fp; fp.addRoundedRectangle (fill, corner);
            theme::glowPath (g, fp, 0.22f, 8);
            // green->accent so loud reads hot, like a real meter
            juce::ColourGradient ag (juce::Colour (0xff27d0a8), r.getX(), 0.0f,
                                     theme::accent, r.getRight(), 0.0f, false);
            ag.addColour (0.7, juce::Colour (0xfff0a83a));
            g.setGradientFill (ag);
            g.fillRoundedRectangle (fill, corner);
            g.setColour (juce::Colours::white.withAlpha (0.28f));
            g.drawLine (fill.getX() + corner, fill.getY() + 0.8f,
                        fill.getRight() - corner, fill.getY() + 0.8f, 1.0f);
        }

        // floating peak cap
        if (pkP > 0.01f)
        {
            const float px = r.getX() + r.getWidth() * pkP;
            g.setColour (theme::accent.withAlpha (0.9f));
            g.fillRoundedRectangle (px - 1.5f, r.getY(), 3.0f, r.getHeight(), 1.5f);
        }

        g.setColour (theme::inkSoft);
        g.setFont (theme::font (12.0f, false));
        juce::String txt = smoothed > 0.0001f ? juce::String (db, 1) + " dB" : juce::String ("-inf");
        g.drawText (txt, textArea, juce::Justification::centredRight);
    }

private:
    void timerCallback() override
    {
        const float target = source.load();
        if (target > smoothed) smoothed = target;
        else                   smoothed += (target - smoothed) * 0.2f;

        if (target >= peak) { peak = target; peakHold = 18; }
        else if (--peakHold <= 0) peak += (target - peak) * 0.08f;

        repaint();
    }

    std::atomic<float>& source;
    float smoothed = 0.0f, peak = 0.0f;
    int   peakHold = 0;
};
