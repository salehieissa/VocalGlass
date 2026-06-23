#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/EQBand.h"

//==============================================================================
// Small filter-shape glyph used on band tabs, the type selector, and nodes.
// Drawn as a little response curve so each type reads at a glance.
//==============================================================================
inline void drawBandGlyph (juce::Graphics& g, BandType t, juce::Point<float> c,
                           juce::Colour col, float s = 7.0f)
{
    const float h = s * 0.72f;
    juce::Path p;

    switch (t)
    {
        case BandType::Bell: // symmetric hump
            p.startNewSubPath (c.x - s, c.y + h);
            p.cubicTo (c.x - s * 0.45f, c.y + h, c.x - s * 0.45f, c.y - h, c.x, c.y - h);
            p.cubicTo (c.x + s * 0.45f, c.y - h, c.x + s * 0.45f, c.y + h, c.x + s, c.y + h);
            break;

        case BandType::LowShelf: // high on the left, step down to the right
            p.startNewSubPath (c.x - s, c.y - h);
            p.lineTo (c.x - s * 0.2f, c.y - h);
            p.cubicTo (c.x + s * 0.1f, c.y - h, c.x + s * 0.1f, c.y + h, c.x + s * 0.45f, c.y + h);
            p.lineTo (c.x + s, c.y + h);
            break;

        case BandType::HighShelf: // low on the left, step up to the right
            p.startNewSubPath (c.x - s, c.y + h);
            p.lineTo (c.x - s * 0.45f, c.y + h);
            p.cubicTo (c.x - s * 0.1f, c.y + h, c.x - s * 0.1f, c.y - h, c.x + s * 0.2f, c.y - h);
            p.lineTo (c.x + s, c.y - h);
            break;

        case BandType::LowCut: // cut lows: rise from bottom-left up to flat
            p.startNewSubPath (c.x - s, c.y + h);
            p.cubicTo (c.x - s * 0.3f, c.y + h, c.x - s * 0.15f, c.y - h, c.x + s * 0.25f, c.y - h);
            p.lineTo (c.x + s, c.y - h);
            break;

        case BandType::HighCut: // cut highs: flat then fall to bottom-right
            p.startNewSubPath (c.x - s, c.y - h);
            p.lineTo (c.x - s * 0.25f, c.y - h);
            p.cubicTo (c.x + s * 0.15f, c.y - h, c.x + s * 0.3f, c.y + h, c.x + s, c.y + h);
            break;

        case BandType::Notch: // sharp dip
            p.startNewSubPath (c.x - s, c.y - h);
            p.lineTo (c.x - s * 0.28f, c.y - h);
            p.lineTo (c.x, c.y + h);
            p.lineTo (c.x + s * 0.28f, c.y - h);
            p.lineTo (c.x + s, c.y - h);
            break;
    }

    g.setColour (col);
    g.strokePath (p, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
