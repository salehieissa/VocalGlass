#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include <atomic>

//==============================================================================
// Vertical L/R output meters with a dB scale (0,-3,-6,-12,-24,-36,-48).
// Reads two atomic linear-peak values owned by the processor.
//==============================================================================
class Meter : public juce::Component,
              private juce::Timer
{
public:
    Meter (std::atomic<float>& l, std::atomic<float>& r) : srcL (l), srcR (r)
    {
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();

        // scale numbers down the centre
        const float scaleW = 22.0f;
        auto scaleArea = r.removeFromRight (scaleW);
        r.removeFromRight (4.0f);

        const float barGap = 8.0f;
        const float barW = (r.getWidth() - barGap) * 0.5f;
        auto barL = juce::Rectangle<float> (r.getX(), r.getY(), barW, r.getHeight() - 16.0f);
        auto barR = juce::Rectangle<float> (r.getX() + barW + barGap, r.getY(), barW, r.getHeight() - 16.0f);

        drawBar (g, barL, dispL);
        drawBar (g, barR, dispR);

        // L / R captions
        g.setColour (theme::inkSoft);
        g.setFont (theme::font (11.0f, false));
        g.drawText ("L", (int) barL.getX(), (int) (barL.getBottom() + 2.0f),
                    (int) barL.getWidth(), 14, juce::Justification::centred);
        g.drawText ("R", (int) barR.getX(), (int) (barR.getBottom() + 2.0f),
                    (int) barR.getWidth(), 14, juce::Justification::centred);

        // dB scale
        g.setFont (theme::font (9.5f, false));
        const int marks[] = { 0, -3, -6, -12, -24, -36, -48 };
        for (int m : marks)
        {
            const float prop = dbToProp ((float) m);
            const float yy = barL.getBottom() - prop * barL.getHeight();
            g.setColour (theme::inkSoft);
            g.drawText (juce::String (m), (int) scaleArea.getX(), (int) (yy - 7.0f),
                        (int) scaleW, 14, juce::Justification::centredLeft);
        }
    }

private:
    void drawBar (juce::Graphics& g, juce::Rectangle<float> bar, float prop)
    {
        const float corner = bar.getWidth() * 0.5f;

        // recessed well
        theme::recess (g, bar, corner);

        if (prop > 0.001f)
        {
            auto fill = bar.withTop (bar.getBottom() - prop * bar.getHeight());
            juce::Path fp; fp.addRoundedRectangle (fill, corner);
            theme::glowPath (g, fp, 0.22f, 8);

            // green (quiet, bottom) -> orange -> accent (hot, top), mapped over
            // the full bar so the colour at a given level stays stable.
            juce::ColourGradient ag (theme::accent, bar.getCentreX(), bar.getY(),
                                     juce::Colour (0xff27d0a8), bar.getCentreX(), bar.getBottom(), false);
            ag.addColour (0.55, juce::Colour (0xfff0a83a));
            g.setGradientFill (ag);
            g.fillRoundedRectangle (fill, corner);

            g.setColour (juce::Colours::white.withAlpha (0.28f));
            g.drawLine (fill.getX() + corner, fill.getY() + 0.8f,
                        fill.getRight() - corner, fill.getY() + 0.8f, 1.0f);
        }
    }

    static float dbToProp (float db)
    {
        // map -54..0 dB onto 0..1
        return juce::jlimit (0.0f, 1.0f, (db + 54.0f) / 54.0f);
    }

    void timerCallback() override
    {
        auto step = [] (float& disp, std::atomic<float>& src)
        {
            const float lin = src.load();
            const float db = juce::Decibels::gainToDecibels (lin, -60.0f);
            const float target = dbToProp (db);
            disp = target > disp ? target : disp + (target - disp) * 0.25f; // fast attack, slow release
        };
        step (dispL, srcL);
        step (dispR, srcR);
        repaint();
    }

    std::atomic<float>& srcL;
    std::atomic<float>& srcR;
    float dispL = 0.0f, dispR = 0.0f;
};
