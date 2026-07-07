#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "../dsp/ClipEngine.h"

//==============================================================================
// Transfer-curve screen (FL Soft Clipper style) — the centrepiece of the UI.
// A dark glass window showing:
//   * a fine grid + the dashed unity diagonal (what "no clipping" would be)
//   * the live transfer curve in neon, recomputed from drive/shape/ceiling
//   * the ceiling as a horizontal line the curve flattens into
//   * a glowing dot riding the curve at the current input level, with a
//     trailing bloom along the curve showing the recent signal path
// All drawing is code (no baked pixels), so it works in vector AND plate mode.
// The editor's timer calls refresh() at 60 Hz.
//==============================================================================
class ClipDisplay : public juce::Component
{
public:
    explicit ClipDisplay (ClipEngine& e) : engine (e)
    {
        setInterceptsMouseClicks (false, false);
        setOpaque (false);
    }

    void refresh (float driveDbNow, int shapeNow, float ceilingDbNow)
    {
        driveDb = driveDbNow;
        shape = shapeNow;
        ceilingDb = ceilingDbNow;

        // live input position (dB -> linear, pre-drive) with a smooth fall
        const float inLin = juce::Decibels::decibelsToGain (engine.inDb.load());
        liveIn = inLin > liveIn ? inLin : liveIn * 0.90f + inLin * 0.10f;
        peakIn = juce::jmax (peakIn * 0.985f, liveIn);

        clipAmt = engine.clipDb.load();
        clipSmooth = clipAmt > clipSmooth ? clipAmt : clipSmooth * 0.92f + clipAmt * 0.08f;

        idlePhase += 0.010f;
        if (idlePhase > 1.0f) idlePhase -= 1.0f;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float rad = juce::jmin (18.0f, r.getHeight() * 0.12f);
        const auto accent = theme::accent;

        // ---- recessed dark glass window ----
        {
            juce::ColourGradient bg (juce::Colour (0xff17181d), r.getX(), r.getY(),
                                     juce::Colour (0xff0b0c10), r.getX(), r.getBottom(), false);
            g.setGradientFill (bg);
            g.fillRoundedRectangle (r, rad);

            g.setGradientFill ({ juce::Colours::black.withAlpha (0.55f), r.getX(), r.getY(),
                                 juce::Colours::transparentBlack, r.getX(), r.getY() + r.getHeight() * 0.22f, false });
            g.fillRoundedRectangle (r, rad);
            g.setGradientFill ({ juce::Colours::white.withAlpha (0.07f), r.getX(), r.getY(),
                                 juce::Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.4f, false });
            g.fillRoundedRectangle (r.reduced (1.5f), rad - 1.0f);
        }

        g.saveState();
        juce::Path clipPath; clipPath.addRoundedRectangle (r.reduced (2.0f), rad - 1.5f);
        g.reduceClipRegion (clipPath);

        // plot area: input runs left->right, output bottom->top (positive
        // quadrant only, like FL's soft clipper)
        auto plot = r.reduced (r.getWidth() * 0.035f, r.getHeight() * 0.14f);

        // the x axis spans up to `xMax` input units (1.0 = full scale before
        // drive); with drive the knee slides left, so keep a fixed window
        const float xMax = 1.25f;
        const float ceilLin = juce::Decibels::decibelsToGain (ceilingDb);
        const float yMax = 1.1f;   // output axis headroom above the ceiling

        auto toX = [&] (float in)  { return plot.getX() + plot.getWidth() * (in / xMax); };
        auto toY = [&] (float out) { return plot.getBottom() - plot.getHeight() * (out / yMax); };

        // ---- fine grid ----
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        for (int i = 1; i <= 4; ++i)
        {
            const float fx = plot.getX() + plot.getWidth() * (float) i / 5.0f;
            const float fy = plot.getY() + plot.getHeight() * (float) i / 5.0f;
            g.drawVerticalLine ((int) fx, plot.getY(), plot.getBottom());
            g.drawHorizontalLine ((int) fy, plot.getX(), plot.getRight());
        }

        // ---- dashed unity diagonal (no clipping reference) ----
        {
            g.setColour (juce::Colours::white.withAlpha (0.16f));
            const float dash[] = { 5.0f, 5.0f };
            g.drawDashedLine ({ toX (0.0f), toY (0.0f), toX (yMax), toY (yMax) }, dash, 2, 1.1f);
        }

        // ---- ceiling line (blooms pink while clipping) ----
        {
            const float glow = juce::jlimit (0.0f, 1.0f, clipSmooth / 6.0f);
            g.setColour (juce::Colours::white.withAlpha (0.20f).interpolatedWith (accent, glow));
            const float dash[] = { 6.0f, 5.0f };
            g.drawDashedLine ({ plot.getX(), toY (ceilLin), plot.getRight(), toY (ceilLin) }, dash, 2, 1.2f);
        }

        // ---- transfer curve ----
        const float drive = juce::Decibels::decibelsToGain (driveDb);
        auto transfer = [&] (float in)   // in = input units pre-drive
        {
            const float u = in * drive / ceilLin;
            float y;
            switch (shape)
            {
                case ClipEngine::Warm: y = std::tanh (u); break;
                case ClipEngine::Hard:
                {
                    constexpr float knee = 0.1f;
                    const float a = std::abs (u);
                    if (a <= 1.0f - knee)      y = a;
                    else if (a >= 1.0f + knee) y = 1.0f;
                    else { const float d2 = a - (1.0f - knee); y = a - d2 * d2 / (4.0f * knee); }
                    break;
                }
                default:
                {
                    const float a = juce::jlimit (0.0f, 1.5f, std::abs (u));
                    y = juce::jlimit (0.0f, 1.0f, a - (a * a * a) / 6.75f);
                    break;
                }
            }
            return y * ceilLin;
        };

        juce::Path curve;
        constexpr int kPts = 120;
        for (int i = 0; i <= kPts; ++i)
        {
            const float in = xMax * (float) i / (float) kPts;
            const float x = toX (in), y = toY (transfer (in));
            if (i == 0) curve.startNewSubPath (x, y);
            else        curve.lineTo (x, y);
        }

        // soft fill under the curve, then glow passes, then the hot core
        {
            juce::Path fill (curve);
            fill.lineTo (plot.getRight(), plot.getBottom());
            fill.lineTo (plot.getX(), plot.getBottom());
            fill.closeSubPath();
            g.setGradientFill ({ accent.withAlpha (0.16f), plot.getX(), plot.getY(),
                                 accent.withAlpha (0.02f), plot.getX(), plot.getBottom(), false });
            g.fillPath (fill);
        }
        g.setColour (accent.withAlpha (0.10f));
        g.strokePath (curve, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        g.setColour (accent.withAlpha (0.28f));
        g.strokePath (curve, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        // ---- live signal dot riding the curve ----
        const bool hasSignal = liveIn > 0.003f;
        if (hasSignal)
        {
            const float in = juce::jmin (liveIn, xMax);
            const float x = toX (in), y = toY (transfer (in));

            // trailing bloom along the recent stretch of curve
            juce::Path trail;
            constexpr int kTrail = 24;
            for (int i = 0; i <= kTrail; ++i)
            {
                const float t  = (float) i / (float) kTrail;
                const float ti = in * (0.55f + 0.45f * t);
                const float tx = toX (ti), ty = toY (transfer (ti));
                if (i == 0) trail.startNewSubPath (tx, ty);
                else        trail.lineTo (tx, ty);
            }
            g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.strokePath (trail, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

            juce::ColourGradient bloom (juce::Colours::white.withAlpha (0.85f), x, y,
                                        accent.withAlpha (0.0f), x + 22.0f, y, true);
            g.setGradientFill (bloom);
            g.fillEllipse (x - 22.0f, y - 22.0f, 44.0f, 44.0f);
            g.setColour (juce::Colours::white);
            g.fillEllipse (x - 3.2f, y - 3.2f, 6.4f, 6.4f);

            // faint peak-hold tick on the input axis
            const float pk = juce::jmin (peakIn, xMax);
            g.setColour (juce::Colours::white.withAlpha (0.30f));
            g.fillRoundedRectangle (toX (pk) - 1.0f, plot.getBottom() - 7.0f, 2.0f, 7.0f, 1.0f);
        }

        // ---- clip amount readout (top right) ----
        if (hasSignal)
        {
            g.setColour (clipSmooth > 0.05f ? accent : juce::Colours::white.withAlpha (0.35f));
            g.setFont (theme::font (13.0f, true));
            g.drawText (juce::String (-clipSmooth, 1) + " dB CLIP",
                        r.reduced (14.0f, 9.0f), juce::Justification::topRight);
        }

        // ---- axis captions ----
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.setFont (theme::font (9.5f, false));
        g.drawText ("IN", r.reduced (14.0f, 8.0f), juce::Justification::bottomRight);
        g.drawText ("OUT", r.reduced (14.0f, 8.0f), juce::Justification::topLeft);

        // ---- idle scan shimmer when no audio ----
        if (! hasSignal)
        {
            const float x = plot.getX() + plot.getWidth() * idlePhase;
            juce::ColourGradient scan (accent.withAlpha (0.10f), x, plot.getCentreY(),
                                       accent.withAlpha (0.0f), x + 60.0f, plot.getCentreY(), true);
            g.setGradientFill (scan);
            g.fillRect (juce::Rectangle<float> (x - 60.0f, r.getY(), 120.0f, r.getHeight()));

            g.setColour (juce::Colours::white.withAlpha (0.22f));
            g.setFont (theme::font (11.0f, false));
            g.drawText ("AWAITING SIGNAL", plot, juce::Justification::centred);
        }

        g.restoreState();

        // hairline frame
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (r.reduced (0.75f), rad, 1.2f);
    }

private:
    ClipEngine& engine;
    float driveDb = 6.0f, ceilingDb = -0.3f;
    int shape = ClipEngine::Soft;
    float liveIn = 0.0f, peakIn = 0.0f;
    float clipAmt = 0.0f, clipSmooth = 0.0f, idlePhase = 0.0f;
};
