#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

//==============================================================================
// Scale-note key. A clean white pill/key with a soft top sheen and a crisp
// hairline when off; fills with the accent gradient (white text) when enabled.
// No status dots, no clipped shadows. Toggle state is driven by an APVTS
// ButtonAttachment.
//==============================================================================
class NoteButton : public juce::Button
{
public:
    explicit NoteButton (const juce::String& label) : juce::Button (label)
    {
        setClickingTogglesState (true);
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.0f);
        const float d = juce::jmin (b.getWidth(), b.getHeight());
        auto circle = juce::Rectangle<float> (b.getCentreX() - d * 0.5f,
                                              b.getCentreY() - d * 0.5f, d, d);
        const bool on = getToggleState();

        if (on)
        {
            juce::ColourGradient ag (theme::accentHi, circle.getX(), circle.getY(),
                                     theme::accentLo, circle.getX(), circle.getBottom(), false);
            g.setGradientFill (ag);
            g.fillEllipse (circle);
            // top highlight catch
            g.setColour (juce::Colours::white.withAlpha (0.30f));
            g.drawEllipse (circle.reduced (1.0f).translated (0.0f, -0.4f), 1.0f);
            g.setColour (theme::accentLo.withAlpha (0.55f));
            g.drawEllipse (circle, 1.0f);
        }
        else
        {
            juce::ColourGradient wg (juce::Colours::white, circle.getX(), circle.getY(),
                                     juce::Colour (0xfff4f5f8), circle.getX(), circle.getBottom(), false);
            g.setGradientFill (wg);
            g.fillEllipse (circle);
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawEllipse (circle.reduced (1.0f).translated (0.0f, -0.4f), 1.0f);
            g.setColour (highlighted ? theme::accent.withAlpha (0.55f) : theme::cardLine);
            g.drawEllipse (circle, 1.2f);
        }

        g.setColour (on ? juce::Colours::white : theme::ink);
        g.setFont (theme::font (d * 0.34f, false));
        g.drawText (getButtonText(), circle, juce::Justification::centred);
    }
};
