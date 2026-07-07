#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

//==============================================================================
// Premium light look shared with the rest of the suite: soft white neumorphic
// dome knobs with a glowing accent value ring, and clean white pill buttons
// that fill with the accent gradient when active.
//==============================================================================
class RackLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Plate mode: buttons whose art is baked into the chassis become invisible
    // hit areas (componentID "hit"); the editor masks their lit state from the
    // ON plate.
    bool plate = false;

    RackLookAndFeel()
    {
       #if VG_HAS_BUNDLED_FONT
        setDefaultSansSerifTypeface (theme::bundledTypeface (false));
       #else
        setDefaultSansSerifTypefaceName (theme::fontFamily);
       #endif

        setColour (juce::ResizableWindow::backgroundColourId, theme::bg);
        setColour (juce::Label::textColourId, theme::ink);
        setColour (juce::TextButton::textColourOffId, theme::ink);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colours::white);
        setColour (juce::PopupMenu::textColourId, theme::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::accent);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::buttonColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::textColourId, theme::ink);
        setColour (juce::ComboBox::arrowColourId, theme::inkSoft);
    }

    //==========================================================================
    // The preset selector combo lives inside the painted header capsule, so it
    // draws no chrome of its own — just centred text.
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int,
                       juce::ComboBox&) override {}

    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return theme::font (15.0f, false);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (1, 1, box.getWidth() - 2, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (juce::Justification::centred);
    }

    juce::Font getLabelFont (juce::Label& label) override
    {
        const auto f = label.getFont();
        return theme::font (f.getHeight(), f.isBold());
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return theme::font (juce::jmin (13.0f, (float) buttonHeight * 0.5f), false);
    }

    juce::Font getPopupMenuFont() override { return theme::font (14.0f, false); }

    //==========================================================================
    // Clean white pill: filled with the accent gradient when active, crisp
    // hairline outline when not.
    void drawButtonBackground (juce::Graphics& g, juce::Button& b,
                               const juce::Colour&, bool highlighted, bool /*down*/) override
    {
        if (plate && b.getComponentID() == "hit") return;

        auto r = b.getLocalBounds().toFloat().reduced (1.5f);
        const float radius = r.getHeight() * 0.5f;
        paintPill (g, r, radius, b.getToggleState(), highlighted);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b,
                         bool highlighted, bool down) override
    {
        if (plate && b.getComponentID() == "hit") return;
        LookAndFeel_V4::drawButtonText (g, b, highlighted, down);
    }

private:
    static void paintPill (juce::Graphics& g, juce::Rectangle<float> r, float radius,
                           bool on, bool highlighted)
    {
        if (on)
        {
            juce::ColourGradient ag (theme::accentHi, r.getX(), r.getY(),
                                     theme::accentLo, r.getX(), r.getBottom(), false);
            g.setGradientFill (ag);
            g.fillRoundedRectangle (r, radius);
            g.setColour (juce::Colours::white.withAlpha (0.30f));
            g.drawLine (r.getX() + radius, r.getY() + 1.2f, r.getRight() - radius, r.getY() + 1.2f, 1.2f);
            g.setColour (theme::accentLo.withAlpha (0.55f));
            g.drawRoundedRectangle (r, radius, 1.0f);
        }
        else
        {
            juce::ColourGradient wg (juce::Colours::white, r.getX(), r.getY(),
                                     juce::Colour (0xfff4f5f8), r.getX(), r.getBottom(), false);
            g.setGradientFill (wg);
            g.fillRoundedRectangle (r, radius);
            g.setColour (juce::Colours::white);
            g.drawLine (r.getX() + radius, r.getY() + 1.0f, r.getRight() - radius, r.getY() + 1.0f, 1.0f);
            g.setColour (highlighted ? theme::accent.withAlpha (0.55f) : theme::cardLine);
            g.drawRoundedRectangle (r, radius, 1.2f);
        }
    }
};
