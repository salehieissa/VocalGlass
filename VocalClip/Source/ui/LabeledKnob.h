#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

//==============================================================================
// A rotary knob with a value read-out and a name caption beneath it. The value
// text is updated by the editor (so it can show "1/8D", "40%", "120 Hz", ...).
//==============================================================================
class LabeledKnob : public juce::Component
{
public:
    LabeledKnob (const juce::String& nm, bool large = false)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (kStart, kEnd, true);
        addAndMakeVisible (slider);

        value.setJustificationType (juce::Justification::centred);
        value.setColour (juce::Label::textColourId, theme::ink);
        value.setFont (theme::font (large ? 15.0f : 12.5f, false));
        addAndMakeVisible (value);

        name.setText (nm, juce::dontSendNotification);
        name.setJustificationType (juce::Justification::centred);
        name.setColour (juce::Label::textColourId, large ? theme::ink : theme::inkSoft);
        name.setFont (theme::font (large ? 21.0f : 13.0f, false));
        addAndMakeVisible (name);

        isLarge = large;
    }

    void setValueText (const juce::String& t) { value.setText (t, juce::dontSendNotification); }

    // Plate mode: caption + value are baked/painted by the editor; this
    // component becomes just the dome sprite with a full 360-degree sweep
    // starting and ending at 6 o'clock.
    void setPlate (bool p, const juce::String& domeID)
    {
        plate = p;
        name.setVisible (! p);
        value.setVisible (! p);
        if (p)
        {
            slider.setComponentID (domeID);
            slider.setRotaryParameters (juce::MathConstants<float>::pi,
                                        juce::MathConstants<float>::pi * 3.0f, true);
        }
        resized();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        if (plate)
        {
            const int d = juce::jmin (r.getWidth(), r.getHeight());
            slider.setBounds (r.withSizeKeepingCentre (d, d));
            return;
        }
        const int nameH  = isLarge ? 26 : 18;
        const int valueH = isLarge ? 22 : 17;
        name.setBounds (r.removeFromBottom (nameH));
        value.setBounds (r.removeFromBottom (valueH));
        // keep the knob square and centred in what remains
        const int d = juce::jmin (r.getWidth(), r.getHeight());
        slider.setBounds (r.withSizeKeepingCentre (d, d));
    }

    juce::Slider slider;
    juce::Label  value, name;

private:
    bool plate   = false;
    bool isLarge = false;
    static constexpr float kStart = juce::MathConstants<float>::pi * 1.25f;
    static constexpr float kEnd   = juce::MathConstants<float>::pi * 2.75f;
};
