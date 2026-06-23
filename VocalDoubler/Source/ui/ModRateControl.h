#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "Bounce.h"
#include "TopBarButton.h"
#include "KnobLookAndFeel.h"

//==============================================================================
// A small white pill that shows the current tempo division (e.g. "1/4") and
// cycles to the next one when clicked. Bound manually to the "modDiv" choice
// parameter by the editor. Greyed out when sync is off.
//==============================================================================
class DivisionPill : public juce::Button
{
public:
    DivisionPill() : juce::Button ("division") {}

    void setText (const juce::String& t) { if (t != text) { text = t; repaint(); } }

    void paintButton (juce::Graphics& g, bool highlighted, bool /*down*/) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f);
        const float radius = r.getHeight() * 0.5f;
        const bool en = isEnabled();

        KnobLookAndFeel::paintPill (g, r, radius, false, highlighted && en);
        if (! en)
        {
            g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.fillRoundedRectangle (r, radius);
        }

        // subtle chevrons hinting the pill cycles through values
        const juce::Colour chev = en ? theme::accent : theme::inkSoft.withAlpha (0.35f);
        const float cy = r.getCentreY(), ch = r.getHeight() * 0.18f;
        g.setColour (chev);
        {
            const float lx = r.getX() + r.getHeight() * 0.55f;
            juce::Path l; l.startNewSubPath (lx + 3.0f, cy - ch); l.lineTo (lx, cy); l.lineTo (lx + 3.0f, cy + ch);
            g.strokePath (l, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            const float rx = r.getRight() - r.getHeight() * 0.55f;
            juce::Path rp; rp.startNewSubPath (rx - 3.0f, cy - ch); rp.lineTo (rx, cy); rp.lineTo (rx - 3.0f, cy + ch);
            g.strokePath (rp, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        g.setColour (en ? theme::ink : theme::inkSoft.withAlpha (0.5f));
        g.setFont (theme::font (juce::jmin (15.0f, r.getHeight() * 0.5f), false));
        g.drawText (text, r, juce::Justification::centred);
    }

private:
    juce::String text { "1/4" };
};

//==============================================================================
// Compact modulation-rate control: a flat rotary "rate" knob, a "SYNC" toggle
// pill and a small division selector. Shows the live value (Hz, or the note
// division when synced) in the caption row. Child controls are public so the
// editor can bind them to the APVTS.
//==============================================================================
class ModRateControl : public juce::Component
{
public:
    juce::Slider              rateKnob;
    Bouncy<TopBarButton>      syncBtn { "SYNC", TopBarButton::Kind::Plain };
    Bouncy<DivisionPill>      divBtn;

    ModRateControl()
    {
        rateKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        rateKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        rateKnob.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                      juce::MathConstants<float>::pi * 2.8f, true);
        addAndMakeVisible (rateKnob);
        addAndMakeVisible (syncBtn);
        addAndMakeVisible (divBtn);
    }

    // Update the value displayed in the caption row.
    void setReadout (bool syncedOn, float hz, juce::String divText)
    {
        synced = syncedOn;
        rateHz = hz;
        division = std::move (divText);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        theme::spacedText (g, "MOD RATE", capArea.toFloat(), theme::inkSoft,
                           11.0f, 2.2f, true, juce::Justification::centredLeft);

        const juce::String value = synced ? division
                                          : juce::String (rateHz, 2) + " Hz";
        g.setColour (theme::accent);
        g.setFont (theme::font (13.0f, true));
        g.drawText (value, capArea, juce::Justification::centredRight);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        capArea = r.removeFromTop (20);
        r.removeFromTop (6);

        auto left = r.removeFromLeft (76);
        rateKnob.setBounds (left.withSizeKeepingCentre (66, 66));

        r.removeFromLeft (10);

        const int pillH = 32, gap = 8;
        const int totalH = pillH * 2 + gap;
        const int py = r.getCentreY() - totalH / 2;
        syncBtn.setBounds (r.getX(), py, r.getWidth(), pillH);
        divBtn.setBounds  (r.getX(), py + pillH + gap, r.getWidth(), pillH);
    }

private:
    juce::Rectangle<int> capArea;
    bool   synced  = false;
    float  rateHz  = 0.2f;
    juce::String division { "1/4" };
};
