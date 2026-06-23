#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "KnobLookAndFeel.h"

//==============================================================================
// The "Effect Only" toggle pill: a clean white pill (no dot, centred label)
// that fills with an accent gradient when on. A juce::Button so a
// ButtonAttachment can bind it to the APVTS "effectOnly" parameter.
//==============================================================================
class EffectOnlyButton : public juce::Button
{
public:
    EffectOnlyButton() : juce::Button ("Effect Only")
    {
        setClickingTogglesState (true);
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool /*down*/) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f);
        const float radius = r.getHeight() * 0.5f;
        const bool on = getToggleState();

        KnobLookAndFeel::paintPill (g, r, radius, on, highlighted);

        g.setColour (on ? juce::Colours::white : theme::ink);
        g.setFont (theme::font (juce::jmin (15.0f, r.getHeight() * 0.45f), false));
        g.drawText ("Effect Only", r, juce::Justification::centred);
    }
};
