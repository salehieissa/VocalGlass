#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

//==============================================================================
// Two-segment selector inside one rounded outline (e.g. "Low Latency | HQ").
// index 0 = left segment, 1 = right. Reports changes via onChange.
//==============================================================================
class SegmentedControl : public juce::Component
{
public:
    SegmentedControl (juce::String left, juce::String right)
        : labels { std::move (left), std::move (right) } {}

    std::function<void (int)> onChange;

    void setIndex (int i, juce::NotificationType n = juce::dontSendNotification)
    {
        i = juce::jlimit (0, 1, i);
        if (i != index)
        {
            index = i;
            repaint();
            if (n != juce::dontSendNotification && onChange) onChange (index);
        }
    }

    int getIndex() const { return index; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        setIndex (e.x < getWidth() / 2 ? 0 : 1, juce::sendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const float radius = 9.0f;

        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (r, radius);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (r, radius, 1.2f);

        auto half = r.withWidth (r.getWidth() * 0.5f);
        auto active = (index == 0) ? half : half.withX (r.getCentreX());
        g.setColour (theme::accentSoft);
        g.fillRoundedRectangle (active.reduced (2.5f), radius - 2.0f);

        g.setColour (theme::cardLine);
        g.fillRect (r.getCentreX() - 0.5f, r.getY() + 6.0f, 1.0f, r.getHeight() - 12.0f);

        g.setFont (theme::font (13.0f, false));
        for (int i = 0; i < 2; ++i)
        {
            auto seg = r.withWidth (r.getWidth() * 0.5f);
            if (i == 1) seg.setX (r.getCentreX());
            g.setColour (i == index ? theme::ink : theme::inkSoft);
            g.drawText (labels[(size_t) i], seg, juce::Justification::centred);
        }
    }

private:
    std::array<juce::String, 2> labels;
    int index = 0;
};
