#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include <array>
#include <cmath>

//==============================================================================
// The centre "Ratio" graph — a proper compression transfer curve (input dB ->
// output dB). A faint dB grid and a dashed unity (1:1) diagonal sit behind a
// hot-pink transfer curve whose bend reflects threshold + ratio. The region
// between unity and the curve is shaded to show how much is being compressed,
// a live operating dot rides the curve at the current input level, and a clean
// ratio / gain-reduction readout sits in the corners.
//
// The component is a vertical-drag juce::Slider whose value is the ratio (an
// APVTS attachment binds it to the "ratio" parameter); the editor feeds it the
// live threshold, input level and gain reduction each timer tick.
//==============================================================================
class CurveDisplay : public juce::Slider
{
public:
    CurveDisplay()
    {
        setSliderStyle (juce::Slider::LinearVertical);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setVelocityBasedMode (false);
        setMouseDragSensitivity (260);
    }

    void setGainReductionDb (float gr)
    {
        const float clamped = juce::jlimit (0.0f, 24.0f, gr);
        if (std::abs (clamped - grDb) > 0.01f) { grDb = clamped; repaint(); }
    }

    void setThresholdDb (float t)
    {
        if (std::abs (t - thresholdDb) > 0.01f) { thresholdDb = t; repaint(); }
    }

    void setInputDb (float in)
    {
        const float clamped = juce::jlimit (kDbMin, kDbMax, in);
        if (std::abs (clamped - inputDb) > 0.1f) { inputDb = clamped; repaint(); }
    }

    // Match the per-mode knee widths used by the DSP (Compressor.h):
    //   ARC 6 dB, Opto 12 dB, Warm 14 dB.
    void setMode (int mode)
    {
        const float k = (mode == 1) ? 12.0f : (mode == 2) ? 14.0f : 6.0f;
        setKnee (k);
    }

    void setKnee (float knee)
    {
        const float clamped = juce::jlimit (0.5f, 36.0f, knee);
        if (std::abs (clamped - kneeDb) > 0.01f) { kneeDb = clamped; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto plot   = bounds.reduced (12.0f);

        // recessed display face: the curve etches into a light, sunken panel
        const float faceCorner = 12.0f;
        theme::recess (g, bounds, faceCorner);

        // clip everything to the rounded recessed face so nothing spills out
        juce::Path clip; clip.addRoundedRectangle (bounds, faceCorner);
        g.saveState();
        g.reduceClipRegion (clip);

        const float ratio = (float) getValue();

        auto X = [&] (float db) { return plot.getX() + (db - kDbMin) / (kDbMax - kDbMin) * plot.getWidth(); };
        auto Y = [&] (float db) { return plot.getBottom() - (db - kDbMin) / (kDbMax - kDbMin) * plot.getHeight(); };

        auto outAt = [ratio, this] (float inDb)
        {
            const float slope = 1.0f / ratio - 1.0f;
            const float over  = inDb - thresholdDb;
            float gr;
            if (over <= -kneeDb * 0.5f)      gr = 0.0f;
            else if (over >= kneeDb * 0.5f)  gr = slope * over;
            else { const float k = over + kneeDb * 0.5f; gr = slope * (k * k) / (2.0f * kneeDb); }
            return inDb + gr;
        };

        // ---- grid ----
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        static const std::array<float, 4> ticks { -48.0f, -36.0f, -24.0f, -12.0f };
        for (auto db : ticks)
        {
            g.drawLine (X (db), plot.getY(), X (db), plot.getBottom(), 1.0f);
            g.drawLine (plot.getX(), Y (db), plot.getRight(), Y (db), 1.0f);
        }

        // ---- unity (1:1) diagonal, dashed ----
        {
            juce::Path unity;
            unity.startNewSubPath (X (kDbMin), Y (kDbMin));
            unity.lineTo (X (kDbMax), Y (kDbMax));
            const float dashes[] = { 4.0f, 4.0f };
            g.setColour (theme::inkSoft.withAlpha (0.45f));
            juce::Path dashed;
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, unity, dashes, 2);
            g.strokePath (dashed, juce::PathStrokeType (1.0f));
        }

        // ---- transfer curve path ----
        juce::Path curve;
        const int steps = 96;
        for (int i = 0; i <= steps; ++i)
        {
            const float inDb = kDbMin + (kDbMax - kDbMin) * (float) i / (float) steps;
            const float x = X (inDb), y = Y (outAt (inDb));
            if (i == 0) curve.startNewSubPath (x, y);
            else        curve.lineTo (x, y);
        }

        // ---- shaded compression region (between unity and curve) ----
        {
            juce::Path region = curve;
            region.lineTo (X (kDbMax), Y (kDbMax));
            region.lineTo (X (kDbMin), Y (kDbMin));
            region.closeSubPath();
            g.setColour (theme::accentSoft);
            g.fillPath (region);
        }

        // ---- threshold guide ----
        g.setColour (theme::inkSoft.withAlpha (0.35f));
        g.drawLine (X (thresholdDb), plot.getY(), X (thresholdDb), plot.getBottom(), 1.0f);

        // ---- the curve (with a soft pink glow) ----
        {
            juce::Path curveStroke;
            juce::PathStrokeType (2.5f).createStrokedPath (curveStroke, curve);
            theme::glowPath (g, curveStroke, 0.35f, 10);
        }
        g.setColour (theme::accent);
        g.strokePath (curve, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        // ---- live operating dot ----
        if (inputDb > kDbMin + 1.0f)
        {
            const float dx = X (inputDb), dy = Y (outAt (inputDb));
            g.setColour (theme::accent.withAlpha (0.25f));
            g.fillEllipse (dx - 7.0f, dy - 7.0f, 14.0f, 14.0f);
            g.setColour (juce::Colours::white);
            g.fillEllipse (dx - 4.0f, dy - 4.0f, 8.0f, 8.0f);
            g.setColour (theme::accent);
            g.drawEllipse (dx - 4.0f, dy - 4.0f, 8.0f, 8.0f, 1.6f);
        }

        // ---- ratio readout (top-left) ----
        g.setColour (theme::ink);
        g.setFont (theme::font (22.0f, true));
        g.drawText (juce::String (ratio, 2) + " : 1",
                    juce::Rectangle<float> (plot.getX() + 6.0f, plot.getY() + 4.0f,
                                            plot.getWidth() - 12.0f, 26.0f).toNearestInt(),
                    juce::Justification::topLeft, false);

        // ---- gain-reduction readout (bottom-right) ----
        g.setColour (theme::inkSoft);
        g.setFont (theme::font (12.0f, false));
        g.drawText ("GR", juce::Rectangle<float> (plot.getRight() - 96.0f, plot.getBottom() - 24.0f,
                                                  24.0f, 20.0f).toNearestInt(),
                    juce::Justification::centredRight, false);
        g.setColour (theme::ink);
        g.setFont (theme::font (16.0f, true));
        g.drawText (juce::String (-grDb, 1) + " dB",
                    juce::Rectangle<float> (plot.getRight() - 70.0f, plot.getBottom() - 26.0f,
                                            64.0f, 22.0f).toNearestInt(),
                    juce::Justification::centredRight, false);

        g.restoreState();
    }

private:
    static constexpr float kDbMin = -60.0f;
    static constexpr float kDbMax =   0.0f;

    float grDb        = 0.0f;
    float thresholdDb = -28.0f;
    float inputDb     = kDbMin;
    float kneeDb      = 6.0f;   // ARC default; updated per mode from the editor
};
