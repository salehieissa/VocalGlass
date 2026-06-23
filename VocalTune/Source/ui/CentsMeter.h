#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

//==============================================================================
// Horizontal cents meter: a -100..+100 track with a tick scale beneath and a
// pink block that slides to show the current cents deviation.
//==============================================================================
class CentsMeter : public juce::Component
{
public:
    void setCents (float c, bool active)
    {
        const float clamped = juce::jlimit (-100.0f, 100.0f, c);
        smoothed += (clamped - smoothed) * 0.35f;
        if (active != hasPitch) { hasPitch = active; }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const float scaleH = 22.0f;
        auto barArea = b.removeFromTop (b.getHeight() - scaleH - 6.0f);

        // recessed track well
        const float radius = barArea.getHeight() * 0.5f;
        theme::recess (g, barArea, radius);

        // faint internal dividers near +/-50
        g.setColour (theme::cardLine);
        for (float t : { 0.25f, 0.75f })
        {
            const float x = barArea.getX() + barArea.getWidth() * t;
            g.fillRect (x - 1.0f, barArea.getY() + 4.0f, 2.0f, barArea.getHeight() - 8.0f);
        }

        // pink deviation block (glowing accent gradient when tracking pitch)
        const float blockW = barArea.getWidth() * 0.14f;
        const float usable = barArea.getWidth() - blockW;
        const float norm   = (smoothed + 100.0f) / 200.0f; // 0..1
        const float bx = barArea.getX() + norm * usable;
        auto block = juce::Rectangle<float> (bx, barArea.getY() + 2.0f,
                                             blockW, barArea.getHeight() - 4.0f);
        if (hasPitch)
        {
            juce::Path bp; bp.addRoundedRectangle (block, radius - 2.0f);
            theme::glowPath (g, bp, 0.35f, 10);
            juce::ColourGradient ag (theme::accentHi, block.getX(), block.getY(),
                                     theme::accentLo, block.getX(), block.getBottom(), false);
            g.setGradientFill (ag);
            g.fillRoundedRectangle (block, radius - 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.30f));
            g.drawLine (block.getX() + 2.0f, block.getY() + 1.0f,
                        block.getRight() - 2.0f, block.getY() + 1.0f, 1.0f);
        }
        else
        {
            g.setColour (theme::inkSoft.withAlpha (0.35f));
            g.fillRoundedRectangle (block, radius - 2.0f);
        }

        // scale ticks + labels
        auto scale = b;
        const char* labels[] = { "-100", "-50", "0", "+50", "+100" };
        g.setFont (theme::font (11.5f, false));
        for (int i = 0; i < 5; ++i)
        {
            const float t = (float) i / 4.0f;
            const float x = barArea.getX() + barArea.getWidth() * t;
            g.setColour (theme::inkSoft);
            g.fillRect (x - 0.6f, scale.getY(), 1.2f, 7.0f);
            g.drawText (labels[i],
                        juce::Rectangle<float> (x - 28.0f, scale.getY() + 7.0f, 56.0f, scaleH - 7.0f),
                        juce::Justification::centred);
        }
        // minor ticks
        g.setColour (theme::cardLine);
        for (int i = 0; i <= 20; ++i)
        {
            if (i % 5 == 0) continue;
            const float x = barArea.getX() + barArea.getWidth() * ((float) i / 20.0f);
            g.fillRect (x - 0.4f, scale.getY(), 0.8f, 4.0f);
        }
    }

private:
    float smoothed = 0.0f;
    bool  hasPitch = false;
};
