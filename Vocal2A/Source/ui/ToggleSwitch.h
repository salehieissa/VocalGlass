#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

//==============================================================================
// ToggleSwitch — a rounded iOS-style switch. Track turns hot pink when on and
// the white thumb slides across. Stays a juce::Button so an APVTS
// ButtonAttachment can drive it.
//==============================================================================
class ToggleSwitch : public juce::Button,
                     private juce::Timer
{
public:
    ToggleSwitch() : juce::Button ("toggle")
    {
        setClickingTogglesState (true);
    }

    void paintButton (juce::Graphics& g, bool, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const float radius = r.getHeight() * 0.5f;

        const float target = getToggleState() ? 1.0f : 0.0f;
        if (std::abs (target - pos) > 0.001f && ! isTimerRunning())
            startTimerHz (60);

        // track
        g.setColour (getToggleState() ? theme::accent
                                      : theme::track.interpolatedWith (theme::accent, pos));
        g.fillRoundedRectangle (r, radius);

        // thumb
        const float d = r.getHeight() - 6.0f;
        const float x = r.getX() + 3.0f + pos * (r.getWidth() - d - 6.0f);
        juce::Path thumb;
        thumb.addEllipse (x, r.getY() + 3.0f, d, d);
        juce::DropShadow (juce::Colours::black.withAlpha (0.18f), 5, { 0, 1 }).drawForPath (g, thumb);
        g.setColour (juce::Colours::white);
        g.fillPath (thumb);
    }

private:
    void timerCallback() override
    {
        const float target = getToggleState() ? 1.0f : 0.0f;
        pos += (target - pos) * 0.30f;
        if (std::abs (target - pos) < 0.005f) { pos = target; stopTimer(); }
        repaint();
    }

    float pos = 0.0f;
};
