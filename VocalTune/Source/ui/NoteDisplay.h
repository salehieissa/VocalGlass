#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "../Scales.h"

//==============================================================================
// The big detected-note readout: a light recessed display face with a faint
// reference grid, and the detected note drawn as a glowing accent marker (a
// functional readout, so an accent glow is appropriate here).
//==============================================================================
class NoteDisplay : public juce::Component
{
public:
    void setNote (int pitchClass, bool active)
    {
        if (pitchClass != note || active != hasNote)
        {
            note = pitchClass;
            hasNote = active;
            repaint();
        }
    }

    // Plate mode: the dark smoked-glass face + gridlines are baked into the
    // chassis; only the glowing note readout is drawn here.
    bool plate = false;

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const float corner = 14.0f;

        // recessed display face
        if (! plate)
            theme::recess (g, b, corner);

        // faint reference grid (clipped to the rounded face)
        if (! plate)
        {
            juce::Path clip; clip.addRoundedRectangle (b, corner);
            g.saveState();
            g.reduceClipRegion (clip);
            g.setColour (theme::cardLine.withAlpha (0.6f));
            const int cols = 7;
            for (int i = 1; i < cols; ++i)
            {
                const float x = b.getX() + b.getWidth() * (float) i / (float) cols;
                g.fillRect (x - 0.5f, b.getY() + 8.0f, 1.0f, b.getHeight() - 16.0f);
            }
            const float midY = b.getCentreY();
            g.fillRect (b.getX() + 10.0f, midY - 0.5f, b.getWidth() - 20.0f, 1.0f);
            g.restoreState();
        }

        auto face = b.reduced (6.0f);

        if (! hasNote || ! juce::isPositiveAndBelow (note, 12))
        {
            g.setColour (plate ? juce::Colours::white.withAlpha (0.30f)
                               : theme::inkSoft.withAlpha (0.45f));
            g.setFont (theme::font (face.getHeight() * 0.62f, true));
            g.drawText (juce::CharPointer_UTF8 ("\xe2\x80\x93"), face, juce::Justification::centred);
            return;
        }

        const auto name = music::noteNames()[note];
        const juce::String letter = name.substring (0, 1);
        const bool sharp = name.length() > 1;

        auto bigF = theme::font (face.getHeight() * 0.92f, true);
        g.setFont (bigF);
        const float lw = juce::GlyphArrangement::getStringWidth (bigF, letter);

        const float sharpW = sharp ? face.getHeight() * 0.30f : 0.0f;
        const float total  = lw + (sharp ? sharpW * 0.8f : 0.0f);
        const float startX = face.getCentreX() - total * 0.5f;

        // soft accent glow behind the live readout
        theme::accentBloom (g, { startX + total * 0.5f, face.getCentreY() },
                            face.getHeight() * 0.55f, 0.18f);

        g.setColour (theme::accent);
        g.drawText (letter, juce::Rectangle<float> (startX, face.getY(), lw + 4.0f, face.getHeight()),
                    juce::Justification::centredLeft);

        if (sharp)
        {
            auto sharpF = theme::font (face.getHeight() * 0.34f, true);
            g.setFont (sharpF);
            g.drawText ("#",
                        juce::Rectangle<float> (startX + lw + 2.0f, face.getY() + face.getHeight() * 0.06f,
                                                sharpW + 10.0f, face.getHeight() * 0.4f),
                        juce::Justification::centredLeft);
        }
    }

private:
    int  note = -1;
    bool hasNote = false;
};
