#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

//==============================================================================
// Vertical gain-reduction meter: a recessed capsule channel that fills
// downward from the top as the gate attenuates (0 .. range dB), with a dB
// ruler alongside. The channel sits on the left of the component; the ruler
// occupies the remaining width.
//==============================================================================
class GRMeter : public juce::Component
{
public:
    // Current attenuation (positive dB) and the full-scale range it maps onto.
    void setGr (float attenuationDb, float rangeDb)
    {
        maxDb = juce::jmax (1.0f, rangeDb);
        const float t = juce::jlimit (0.0f, 1.0f, attenuationDb / maxDb);
        // light smoothing for a calmer meter (snap the tail to zero so the
        // repaint loop stops once the fill is invisible)
        float next = t > smoothed ? t : smoothed * 0.82f + t * 0.18f;
        if (next < 1.0e-4f) next = 0.0f;
        if (next == smoothed) return;
        smoothed = next;
        repaint();
    }

    float getGrDb() const noexcept { return smoothed * maxDb; }

    // 0..1 fill fraction for plate mode, where the editor masks the lit meter
    // groove from the ON plate instead of this component painting anything.
    float getFraction() const noexcept { return smoothed; }

    bool plate = false;

    void paint (juce::Graphics& g) override
    {
        if (plate) return;

        auto r = getLocalBounds().toFloat();
        const float chanW = 18.0f;
        auto chan = r.removeFromLeft (chanW);
        chan.reduce (0.0f, 8.0f);   // room for the end ruler labels
        const float radius = chanW * 0.5f;

        // recessed gradient well
        theme::recess (g, chan, radius);

        // fill downward from the top (gain-reduction style)
        if (smoothed > 0.004f)
        {
            auto fill = chan.withHeight (chan.getHeight() * smoothed);
            juce::Path fp; fp.addRoundedRectangle (fill, radius);
            theme::glowPath (g, fp, 0.22f, 8);
            juce::ColourGradient ag (theme::accentHi, fill.getX(), fill.getY(),
                                     theme::accentLo, fill.getX(), fill.getBottom(), false);
            g.setGradientFill (ag);
            g.fillRoundedRectangle (fill, radius);
            g.setColour (juce::Colours::white.withAlpha (0.28f));
            g.drawLine (fill.getX() + radius, fill.getY() + 0.8f,
                        fill.getRight() - radius, fill.getY() + 0.8f, 1.0f);
        }

        // dB ruler: 0 at the top down to -range at the bottom
        r.removeFromLeft (6.0f);
        for (int i = 0; i <= 4; ++i)
        {
            const float frac = (float) i / 4.0f;
            const float y = chan.getY() + frac * chan.getHeight();
            g.setColour (theme::cardLine);
            g.fillRect (r.getX(), y - 0.5f, 7.0f, 1.0f);

            const int db = juce::roundToInt (frac * maxDb);
            g.setColour (theme::inkSoft);
            g.setFont (theme::font (10.0f, false));
            g.drawText (db == 0 ? juce::String ("0") : "-" + juce::String (db),
                        juce::Rectangle<float> (r.getX() + 10.0f, y - 7.0f,
                                                r.getWidth() - 10.0f, 14.0f),
                        juce::Justification::centredLeft);
        }
    }

private:
    float smoothed = 0.0f;
    float maxDb = 60.0f;
};
