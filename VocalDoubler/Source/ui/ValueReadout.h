#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

//==============================================================================
// A large left-aligned readout: a small soft-ink caption above a big ink value
// with a small "%". Vertical drag (or mouse-wheel) edits the value 0..100, so
// the two readouts the central node controls are also directly editable.
//==============================================================================
class ValueReadout : public juce::Component
{
public:
    std::function<void (float v01)> onChange;

    explicit ValueReadout (juce::String captionText) : caption (std::move (captionText)) {}

    // Plate mode: the caption is baked into the chassis, so only the big live
    // number + accent "%" are drawn, filling the whole bounds.
    bool plate = false;

    void setValue01 (float v) { value01 = juce::jlimit (0.0f, 1.0f, v); repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();

        if (plate)
        {
            const int pct = juce::roundToInt (value01 * 100.0f);
            auto bigF = theme::font (r.getHeight() * 0.98f, true);
            auto pctF = theme::font (r.getHeight() * 0.38f, true);

            const juce::String num (pct);
            const float nw = juce::GlyphArrangement::getStringWidth (bigF, num);

            g.setColour (theme::ink);
            g.setFont (bigF);
            g.drawText (num, r.withWidth (nw + 6.0f), juce::Justification::centredLeft);

            g.setColour (theme::accent);
            g.setFont (pctF);
            g.drawText ("%", r.withTrimmedLeft (nw + 10.0f).withTrimmedTop (r.getHeight() * 0.10f),
                        juce::Justification::topLeft);
            return;
        }

        auto capArea = r.removeFromTop (r.getHeight() * 0.30f);
        theme::spacedText (g, caption, capArea, theme::inkSoft,
                           juce::jmin (12.0f, capArea.getHeight()), 2.2f, true,
                           juce::Justification::centredLeft);

        const int pct = juce::roundToInt (value01 * 100.0f);
        auto bigF = theme::font (r.getHeight() * 0.92f, true);
        auto pctF = theme::font (r.getHeight() * 0.34f, false);

        const juce::String num (pct);
        const float nw = juce::GlyphArrangement::getStringWidth (bigF, num);

        g.setColour (theme::ink);
        g.setFont (bigF);
        g.drawText (num, r.withWidth (nw + 6.0f), juce::Justification::centredLeft);

        g.setColour (theme::accent);
        g.setFont (pctF);
        g.drawText ("%", r.withTrimmedLeft (nw + 8.0f).withTrimmedTop (r.getHeight() * 0.18f),
                    juce::Justification::topLeft);
    }

    void mouseDown (const juce::MouseEvent&) override { dragStartValue = value01; }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float delta = (float) -e.getDistanceFromDragStartY() / 220.0f;
        setValue01 (dragStartValue + delta);
        if (onChange) onChange (value01);
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        setValue01 (value01 + w.deltaY * 0.5f);
        if (onChange) onChange (value01);
    }

private:
    juce::String caption;
    float value01 = 0.0f;
    float dragStartValue = 0.0f;
};
