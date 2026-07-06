#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "../../../common/ui/Skin.h"

//==============================================================================
// Rounded sliding switch. ON (e.g. Modern) = pink track, thumb to the left;
// OFF (Classic) = grey track, thumb to the right. Toggle state via attachment.
//==============================================================================
class ToggleSwitch : public juce::Button,
                     private juce::Timer
{
public:
    ToggleSwitch() : juce::Button ({})
    {
        setClickingTogglesState (true);
        onStateChange = [this] { startTimerHz (60); };
    }

    // Plate mode: the track is baked into the chassis (silver off, pink masked
    // from the ON plate by the editor); only the sliding thumb is drawn here,
    // as a chrome dome sprite.
    void setPlateThumb (juce::Image dome) { thumbSprite = dome; repaint(); }

    void paintButton (juce::Graphics& g, bool, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const float radius = r.getHeight() * 0.5f;

        if (thumbSprite.isValid())
        {
            const float pad = 2.0f;
            const float d = r.getHeight() - pad * 2.0f;
            const float x = juce::jmap (pos, r.getX() + pad, r.getRight() - pad - d);
            skin::drawKnobRotated (g, thumbSprite,
                                   { x, r.getY() + pad, d, d }, 0.0f);
            return;
        }

        g.setColour (getToggleState() ? theme::accent : theme::track);
        g.fillRoundedRectangle (r, radius);

        const float pad = 3.0f;
        const float d = r.getHeight() - pad * 2.0f;
        const float leftX  = r.getX() + pad;
        const float rightX = r.getRight() - pad - d;
        const float x = juce::jmap (pos, leftX, rightX);

        juce::Rectangle<float> thumb (x, r.getY() + pad, d, d);
        juce::DropShadow (juce::Colours::black.withAlpha (0.18f), 4, { 0, 1 }).drawForRectangle (g, thumb.toNearestInt());
        g.setColour (juce::Colours::white);
        g.fillEllipse (thumb);
    }

private:
    void timerCallback() override
    {
        const float target = getToggleState() ? 0.0f : 1.0f; // ON -> thumb left
        pos += (target - pos) * 0.35f;
        if (std::abs (target - pos) < 0.005f) { pos = target; stopTimer(); }
        repaint();
    }

    float pos = 0.0f;
    juce::Image thumbSprite;
};
