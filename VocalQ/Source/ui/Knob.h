#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include <functional>

//==============================================================================
// Rotary knob with a name above, the live value in the middle, and the min/max
// range printed in the bottom corners (matching the mockup).
//==============================================================================
class Knob : public juce::Component,
             private juce::Slider::Listener
{
public:
    Knob()
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f, true);
        slider.addListener (this);
        addAndMakeVisible (slider);

        name.setJustificationType (juce::Justification::centred);
        name.setColour (juce::Label::textColourId, theme::inkSoft);
        name.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (name);
    }

    void setup (const juce::String& title, std::function<juce::String (double)> formatter, bool big = false)
    {
        name.setText (title.toUpperCase(), juce::dontSendNotification);
        fmt = std::move (formatter);
        isBig = big;
    }

    juce::Slider& getSlider() { return slider; }

    // Plate mode: caption, range marks and value capsule are baked into the
    // chassis — the component shrinks to just the dome square and paints
    // nothing itself (the editor owns the capsule value labels).
    void setPlateMode (bool p)
    {
        plateMode = p;
        name.setVisible (! p);
        resized();
        repaint();
    }

    juce::String valueText() const
    {
        return fmt ? fmt (slider.getValue()) : juce::String (slider.getValue(), 1);
    }

    void paint (juce::Graphics& g) override
    {
        if (plateMode) return;
        g.setColour (theme::inkSoft);
        g.setFont (theme::font (10.0f));
        auto bottom = getLocalBounds().removeFromBottom (14);
        g.drawText (rangeText (slider.getMinimum()), bottom.removeFromLeft (getWidth() / 2),
                    juce::Justification::centredLeft, false);
        g.drawText (rangeText (slider.getMaximum()), bottom,
                    juce::Justification::centredRight, false);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        if (plateMode) return;
        // drawn on top of the slider disc so the value is always visible
        g.setColour (theme::accent);
        g.setFont (theme::font (isBig ? 20.0f : 16.0f, true));
        g.drawText (fmt ? fmt (slider.getValue()) : juce::String (slider.getValue(), 1),
                    sliderArea.withTrimmedTop (sliderArea.getHeight() / 2 - (isBig ? 13 : 10))
                              .withHeight (isBig ? 26 : 20),
                    juce::Justification::centred, false);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        if (plateMode)
        {
            sliderArea = r;
            slider.setBounds (r);
            return;
        }
        name.setBounds (r.removeFromTop (16));
        r.removeFromBottom (14);
        sliderArea = r;
        slider.setBounds (r);
    }

private:
    void sliderValueChanged (juce::Slider*) override { repaint(); }

    juce::String rangeText (double v) const { return fmt ? fmt (v) : juce::String (v, 1); }

    juce::Slider slider;
    juce::Label  name;
    juce::Rectangle<int> sliderArea;
    std::function<juce::String (double)> fmt;
    bool isBig = false;
    bool plateMode = false;
};
