#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "Theme.h"

//==============================================================================
// A titled rotary control: small caption on top, a flat white knob in the
// middle (drawn by KnobLookAndFeel) and a formatted value underneath. Set
// `large` for the big decay knob.
//==============================================================================
class Knob : public juce::Component
{
public:
    Knob (const juce::String& titleText, bool isLarge = false)
        : large (isLarge)
    {
        title.setText (titleText, juce::dontSendNotification);
        title.setJustificationType (juce::Justification::centred);
        title.setFont (theme::font (large ? 17.0f : 12.5f, large));
        title.setColour (juce::Label::textColourId, large ? theme::ink : theme::inkSoft);
        addAndMakeVisible (title);

        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (kStart, kEnd, true);
        slider.getProperties().set ("large", large);
        addAndMakeVisible (slider);

        value.setJustificationType (juce::Justification::centred);
        value.setFont (theme::font (large ? 26.0f : 13.0f, large));
        value.setColour (juce::Label::textColourId, theme::ink);
        addAndMakeVisible (value);

        slider.onValueChange = [this] { refresh(); };
    }

    void setFormatter (std::function<juce::String (double)> f)
    {
        formatter = std::move (f);
        refresh();
    }

    void refresh()
    {
        if (overrideText.isNotEmpty())
            value.setText (overrideText, juce::dontSendNotification);
        else if (formatter)
            value.setText (formatter (slider.getValue()), juce::dontSendNotification);
    }

    // When non-empty, the value readout shows this text instead of the
    // formatted slider value (used to display the tempo division when synced).
    void setOverrideText (const juce::String& t)
    {
        if (t == overrideText) return;
        overrideText = t;
        refresh();
    }

    juce::Slider& getSlider() { return slider; }

    // Plate mode: caption + ring seat are baked into the chassis and the value
    // text is drawn by the editor; the slider draws a rotating chrome dome
    // sprite (per domeID) sweeping the full 360 from 6 o'clock (matching the
    // wedge revealed from the ON plate).
    void setPlate (bool p, const juce::String& domeID = "dome-small")
    {
        plate = p;
        title.setVisible (! p);
        value.setVisible (! p);
        if (p)
        {
            slider.setComponentID (domeID);
            slider.setRotaryParameters (juce::MathConstants<float>::pi,
                                        juce::MathConstants<float>::pi * 3.0f, true);
        }
        resized();
    }

    juce::String getValueText() const { return value.getText(); }

    void resized() override
    {
        auto r = getLocalBounds();
        if (plate)
        {
            slider.setBounds (r);
            return;
        }
        const int titleH = large ? 26 : 18;
        const int valueH = large ? 40 : 18;
        title.setBounds (r.removeFromTop (titleH));
        value.setBounds (r.removeFromBottom (valueH));
        slider.setBounds (r.reduced (large ? 0 : 2));
    }

private:
    static constexpr float kStart = juce::MathConstants<float>::pi * 1.25f;
    static constexpr float kEnd   = juce::MathConstants<float>::pi * 2.75f;

    bool large;
    bool plate = false;
    juce::Label title, value;
    juce::Slider slider;
    std::function<juce::String (double)> formatter;
    juce::String overrideText;
};
