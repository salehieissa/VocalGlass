#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "KnobLookAndFeel.h"
#include <cmath>

//==============================================================================
// The large circular interactive "double" display:
//   - concentric guide circles
//   - a draggable pink glowing node on a radius line from the centre
//   - the node's ANGLE (clockwise from top) sets Separation (0..1)
//   - the node's RADIUS sets Variation (0..1)
//   - a solid pink arc traces from the top clockwise to the node; the rest of
//     the node's circle is a faint pink dashed track
//   - faint wavy concentric ring lines around the outside form a gently
//     animated visualization whose amplitude reflects amount + variation
//==============================================================================
class DoubleDisplay : public juce::Component,
                      private juce::Timer
{
public:
    std::function<void (float sep01, float var01)> onChange;

    DoubleDisplay() { startTimerHz (30); }

    // Plate mode: the recessed dark disc, "DOUBLE" title, guide circle and the
    // decorative wavy rings are baked into the chassis. Only the dynamic layer
    // is drawn: dashed track, value arc, radius line, centre anchor and the
    // draggable node.
    bool plate = false;

    void setValues (float sep01, float var01)
    {
        sep = juce::jlimit (0.0f, 1.0f, sep01);
        var = juce::jlimit (0.0f, 1.0f, var01);
    }

    // 0..1 overall energy used to drive the wavy ring amplitude.
    void setEnergy (float e) { energy = juce::jlimit (0.0f, 1.0f, e); }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto c = bounds.getCentre();
        const float R = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 6.0f;

        if (! plate)
        {
        drawWavyRings (g, c, R);

        // recessed light face: gentle inner shadow at the top, light catch at the
        // bottom, with a crisp hairline so it reads as carved into the panel.
        {
            juce::Rectangle<float> face (c.x - R, c.y - R, R * 2.0f, R * 2.0f);
            g.setColour (theme::card);
            g.fillEllipse (face);
            juce::ColourGradient ig (juce::Colours::black.withAlpha (0.10f), c.x, c.y - R,
                                     juce::Colours::transparentBlack, c.x, c.y + R * 0.25f, false);
            g.setGradientFill (ig);
            g.fillEllipse (face);
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.drawEllipse (face.reduced (0.6f).translated (0.0f, 0.6f), 1.0f);
            g.setColour (theme::cardLine);
            g.drawEllipse (face, 1.4f);
        }

        // a faint inner concentric guide circle
        const float guideR = R * 0.5f;
        g.setColour (theme::cardLine.withAlpha (0.6f));
        g.drawEllipse (c.x - guideR, c.y - guideR, guideR * 2.0f, guideR * 2.0f, 1.0f);

        // title
        theme::spacedText (g, "double",
                           juce::Rectangle<float> (c.x - R, c.y - R * 0.66f, R * 2.0f, R * 0.2f),
                           theme::inkSoft, R * 0.085f, 2.6f, true, juce::Justification::centred);
        } // ! plate

        // node geometry
        const float nodeR = juce::jmap (var, 0.0f, 1.0f, R * 0.16f, R * 0.74f);
        const float theta = sep * juce::MathConstants<float>::twoPi; // clockwise from top
        const juce::Point<float> node (c.x + nodeR * std::sin (theta),
                                       c.y - nodeR * std::cos (theta));

        // faint pink dashed track around the node's full circle
        {
            juce::Path full;
            full.addCentredArc (c.x, c.y, nodeR, nodeR, 0.0f,
                                0.0f, juce::MathConstants<float>::twoPi, true);
            juce::PathStrokeType dashStroke (1.4f);
            const float dashes[] = { 3.0f, 5.0f };
            juce::Path dashed;
            dashStroke.createDashedStroke (dashed, full, dashes, 2);
            g.setColour (theme::accent.withAlpha (0.35f));
            g.fillPath (dashed);
        }

        // solid pink value arc from top clockwise to the node
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, nodeR, nodeR, 0.0f, 0.0f, theta, true);

            juce::Path arcStroke;
            juce::PathStrokeType (3.0f).createStrokedPath (arcStroke, arc);
            theme::glowPath (g, arcStroke, 0.30f, 22);

            g.setColour (theme::accent);
            g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        }

        // radius line from centre to node
        g.setColour (theme::accent.withAlpha (0.5f));
        g.drawLine (c.x, c.y, node.x, node.y, 1.6f);

        // centre: a small white dome anchor
        {
            const float cr = 5.0f;
            KnobLookAndFeel::paintWhiteDome (g, { c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f });
        }

        // glowing accent voice marker: a white dome ringed in glowing accent
        {
            const float nr = 13.0f;
            juce::Path glow;
            glow.addEllipse (node.x - nr, node.y - nr, nr * 2.0f, nr * 2.0f);
            theme::glowPath (g, glow, 0.55f, 26);
            theme::glowPath (g, glow, 0.45f, 14);

            // accent ring base
            g.setColour (theme::accent);
            g.fillEllipse (node.x - nr, node.y - nr, nr * 2.0f, nr * 2.0f);

            // white dome face inset within the accent ring
            const float dr = nr - 3.5f;
            KnobLookAndFeel::paintWhiteDome (g, { node.x - dr, node.y - dr, dr * 2.0f, dr * 2.0f });
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { updateFromMouse (e); }
    void mouseDrag (const juce::MouseEvent& e) override { updateFromMouse (e); }

private:
    void updateFromMouse (const juce::MouseEvent& e)
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto c = bounds.getCentre();
        const float R = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 6.0f;

        const float dx = (float) e.x - c.x;
        const float dy = (float) e.y - c.y;

        float theta = std::atan2 (dx, -dy); // clockwise from top, -pi..pi
        if (theta < 0.0f) theta += juce::MathConstants<float>::twoPi;
        const float newSep = theta / juce::MathConstants<float>::twoPi;

        const float dist = std::sqrt (dx * dx + dy * dy);
        const float newVar = juce::jlimit (0.0f, 1.0f,
            juce::jmap (dist, R * 0.16f, R * 0.74f, 0.0f, 1.0f));

        sep = newSep;
        var = newVar;
        if (onChange) onChange (sep, var);
        repaint();
    }

    void drawWavyRings (juce::Graphics& g, juce::Point<float> c, float R)
    {
        const int numRings = 5;
        const float amp = (0.012f + 0.05f * energy) * R;       // wobble amplitude
        const int   lobes = 11;                                 // wave count around ring

        for (int ring = 0; ring < numRings; ++ring)
        {
            const float baseR = R * (1.02f + ring * 0.055f);
            const float ringPhase = phase * (1.0f + 0.18f * ring) + ring * 0.7f;
            const float alpha = 0.16f * (1.0f - ring / (float) numRings);

            juce::Path p;
            const int steps = 160;
            for (int i = 0; i <= steps; ++i)
            {
                const float t = (float) i / (float) steps;
                const float a = t * juce::MathConstants<float>::twoPi;
                const float wob = amp * std::sin (lobes * a + ringPhase);
                const float rr = baseR + wob;
                const float x = c.x + rr * std::cos (a);
                const float y = c.y + rr * std::sin (a);
                if (i == 0) p.startNewSubPath (x, y);
                else        p.lineTo (x, y);
            }
            p.closeSubPath();

            g.setColour (theme::accent.withAlpha (alpha));
            g.strokePath (p, juce::PathStrokeType (1.2f));
        }
    }

    void timerCallback() override
    {
        phase += 0.045f;
        if (phase > juce::MathConstants<float>::twoPi * 100.0f) phase = 0.0f;
        repaint();
    }

    float sep = 0.56f, var = 1.0f, energy = 0.6f;
    float phase = 0.0f;
};
