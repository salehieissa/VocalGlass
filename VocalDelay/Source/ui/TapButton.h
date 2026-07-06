#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "Bounce.h"
#include <cmath>

//==============================================================================
// Big square tap-tempo button: a white card with a pink waveform glyph and a
// "TAP" caption. Bounces on press.
//==============================================================================
class TapButton : public Bouncy<juce::Button>
{
public:
    TapButton() : Bouncy<juce::Button> ("TAP") {}

    bool plate = false;   // baked into the chassis; editor masks the lit state

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        if (plate) return;

        auto r = getLocalBounds().toFloat().reduced (2.0f);

        // clean white card: subtle top-down sheen + crisp hairline, no shadow
        juce::ColourGradient wg (juce::Colours::white, r.getX(), r.getY(),
                                 juce::Colour (0xfff4f5f8), r.getX(), r.getBottom(), false);
        g.setGradientFill (wg);
        g.fillRoundedRectangle (r, 16.0f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawLine (r.getX() + 16.0f, r.getY() + 1.2f, r.getRight() - 16.0f, r.getY() + 1.2f, 1.2f);
        g.setColour ((highlighted || down) ? theme::accent : theme::cardLine);
        g.drawRoundedRectangle (r, 16.0f, 1.4f);

        // pink waveform glyph
        auto wave = r.reduced (r.getWidth() * 0.22f, r.getHeight() * 0.32f);
        const float midY = wave.getCentreY();
        juce::Path p;
        const int steps = 48;
        for (int i = 0; i <= steps; ++i)
        {
            const float t = (float) i / (float) steps;
            const float x = wave.getX() + t * wave.getWidth();
            const float env = std::sin (t * juce::MathConstants<float>::pi); // taper ends
            const float y = midY - std::sin (t * juce::MathConstants<float>::pi * 6.0f)
                                   * wave.getHeight() * 0.5f * env;
            if (i == 0) p.startNewSubPath (x, y);
            else        p.lineTo (x, y);
        }
        g.setColour (theme::accent);
        g.strokePath (p, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }
};
