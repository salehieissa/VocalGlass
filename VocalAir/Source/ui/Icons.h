#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

//==============================================================================
// Small vector glyphs drawn as stroked paths, matched to the flat UI style.
//==============================================================================
namespace icons
{
    inline void chain (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float thickness = 1.4f)
    {
        // Two interlocking rounded links drawn on a slight diagonal.
        auto area = r.reduced (r.getWidth() * 0.08f, r.getHeight() * 0.22f);
        const float w = area.getWidth() * 0.62f;
        const float h = area.getHeight();
        const float corner = h * 0.5f;

        juce::Rectangle<float> a (area.getX(), area.getCentreY() - h * 0.5f, w, h);
        juce::Rectangle<float> b (area.getRight() - w, area.getCentreY() - h * 0.5f, w, h);

        g.setColour (c);
        g.drawRoundedRectangle (a, corner, thickness);
        g.drawRoundedRectangle (b, corner, thickness);
    }

    inline void undo (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float thickness = 1.6f)
    {
        auto a = r.reduced (r.getWidth() * 0.18f);
        juce::Path p;
        // arc curving anticlockwise with an arrow head on the left
        p.addCentredArc (a.getCentreX() + a.getWidth() * 0.08f, a.getCentreY(),
                         a.getWidth() * 0.36f, a.getHeight() * 0.36f,
                         0.0f, juce::MathConstants<float>::pi * 0.35f,
                         juce::MathConstants<float>::pi * 1.75f, true);
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

        const float ax = a.getCentreX() - a.getWidth() * 0.28f;
        const float ay = a.getCentreY() - a.getHeight() * 0.18f;
        juce::Path head;
        head.startNewSubPath (ax, ay);
        head.lineTo (ax - a.getWidth() * 0.02f, ay + a.getHeight() * 0.30f);
        head.lineTo (ax + a.getWidth() * 0.26f, ay + a.getHeight() * 0.24f);
        g.strokePath (head, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    inline void redo (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float thickness = 1.6f)
    {
        // Mirror of undo.
        juce::Graphics::ScopedSaveState ss (g);
        g.addTransform (juce::AffineTransform::scale (-1.0f, 1.0f, r.getCentreX(), r.getCentreY()));
        undo (g, r, c, thickness);
    }

    inline void save (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float thickness = 1.5f)
    {
        auto a = r.reduced (r.getWidth() * 0.2f);
        juce::Path body;
        const float cut = a.getWidth() * 0.26f;
        body.startNewSubPath (a.getX(), a.getY());
        body.lineTo (a.getRight() - cut, a.getY());
        body.lineTo (a.getRight(), a.getY() + cut);
        body.lineTo (a.getRight(), a.getBottom());
        body.lineTo (a.getX(), a.getBottom());
        body.closeSubPath();
        g.setColour (c);
        g.strokePath (body, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        // inner label slot
        auto slot = juce::Rectangle<float> (a.getX() + a.getWidth() * 0.24f, a.getY(),
                                            a.getWidth() * 0.42f, a.getHeight() * 0.34f);
        g.drawRect (slot, thickness);
    }

    inline void hamburger (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float thickness = 1.6f)
    {
        auto a = r.reduced (r.getWidth() * 0.22f, r.getHeight() * 0.3f);
        g.setColour (c);
        for (int i = 0; i < 3; ++i)
        {
            const float y = a.getY() + a.getHeight() * 0.5f * (float) i;
            g.drawLine (a.getX(), y, a.getRight(), y, thickness);
        }
    }

    inline void power (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float thickness = 1.8f)
    {
        auto a = r.reduced (r.getWidth() * 0.28f);
        juce::Path ring;
        ring.addCentredArc (a.getCentreX(), a.getCentreY(),
                            a.getWidth() * 0.5f, a.getHeight() * 0.5f,
                            0.0f, juce::degreesToRadians (40.0f),
                            juce::degreesToRadians (320.0f), true);
        g.setColour (c);
        g.strokePath (ring, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
        g.drawLine (a.getCentreX(), a.getY() - a.getHeight() * 0.12f,
                    a.getCentreX(), a.getCentreY(), thickness);
    }

    inline void chevron (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float thickness = 1.5f)
    {
        auto a = r.reduced (r.getWidth() * 0.34f, r.getHeight() * 0.3f);
        juce::Path p;
        p.startNewSubPath (a.getX(), a.getY());
        p.lineTo (a.getRight(), a.getCentreY());
        p.lineTo (a.getX(), a.getBottom());
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }
}
