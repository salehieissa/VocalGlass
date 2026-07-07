#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "../dsp/ChopEngine.h"

//==============================================================================
// Live chop screen — the centrepiece of the UI. A dark glass window showing:
//   * the WET waveform of the current refresh cycle as mirrored neon bars
//   * vertical slice lines on every CHOP division
//   * a sweeping playhead with bloom; the active slice glows hot
//   * FREEZE state tints the whole screen ice-blue
// All drawing is code (no baked pixels), so it works in vector AND plate mode.
// The editor's timer calls refresh() at 60 Hz.
//==============================================================================
class ChopDisplay : public juce::Component
{
public:
    explicit ChopDisplay (ChopEngine& e) : engine (e)
    {
        setInterceptsMouseClicks (false, false);
        setOpaque (false);
    }

    void refresh (bool frozenState)
    {
        frozen = frozenState;
        engine.copyVis (bins.data(), ChopEngine::kVisSlots);
        phase = engine.getVisPhase();
        chopFrac = juce::jmax (0.01f, engine.getVisChopFrac());

        // envelope follower on the display so bars decay smoothly, not jitter
        for (int i = 0; i < ChopEngine::kVisSlots; ++i)
        {
            const float target = juce::jlimit (0.0f, 1.0f, bins[(size_t) i] * 1.4f);
            auto& s = smooth[(size_t) i];
            s = target > s ? target : s * 0.86f + target * 0.14f;
        }
        idlePhase += 0.012f;
        if (idlePhase > 1.0f) idlePhase -= 1.0f;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float rad = juce::jmin (18.0f, r.getHeight() * 0.12f);

        const auto accent = frozen ? juce::Colour (0xff59c8ff) : theme::accent;

        // ---- recessed dark glass window ----
        {
            juce::ColourGradient bg (juce::Colour (0xff17181d), r.getX(), r.getY(),
                                     juce::Colour (0xff0b0c10), r.getX(), r.getBottom(), false);
            g.setGradientFill (bg);
            g.fillRoundedRectangle (r, rad);

            // inner shadow top + subtle glass sheen
            g.setGradientFill ({ juce::Colours::black.withAlpha (0.55f), r.getX(), r.getY(),
                                 juce::Colours::transparentBlack, r.getX(), r.getY() + r.getHeight() * 0.22f, false });
            g.fillRoundedRectangle (r, rad);
            g.setGradientFill ({ juce::Colours::white.withAlpha (0.07f), r.getX(), r.getY(),
                                 juce::Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.4f, false });
            g.fillRoundedRectangle (r.reduced (1.5f), rad - 1.0f);
        }

        // bars span the full glass width; a soft alpha ramp at both ends makes
        // the waveform feather out instead of stopping at a hard border
        auto inner = r.reduced (r.getWidth() * 0.004f, r.getHeight() * 0.12f);
        const float midY = inner.getCentreY();
        const float edgeFadeW = inner.getWidth() * 0.045f;
        auto edgeFade = [&] (float x)
        {
            const float dl = (x - inner.getX()) / edgeFadeW;
            const float dr = (inner.getRight() - x) / edgeFadeW;
            return juce::jlimit (0.0f, 1.0f, juce::jmin (dl, dr));
        };

        g.saveState();
        juce::Path clip; clip.addRoundedRectangle (r.reduced (2.0f), rad - 1.5f);
        g.reduceClipRegion (clip);

        // ---- slice grid on every chop division ----
        const int nSlices = juce::jmax (1, (int) std::round (1.0f / chopFrac));
        for (int i = 1; i < nSlices && i < 64; ++i)
        {
            const float x = inner.getX() + inner.getWidth() * (float) i * chopFrac;
            if (x > inner.getRight()) break;
            g.setColour (juce::Colours::white.withAlpha (0.08f * edgeFade (x)));
            g.drawVerticalLine ((int) x, inner.getY(), inner.getBottom());
        }
        // centre hairline
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawHorizontalLine ((int) midY, inner.getX(), inner.getRight());

        // ---- waveform bars (mirrored around centre) ----
        constexpr int kBars = 128;                       // draw every 2nd bin
        const float bw = inner.getWidth() / (float) kBars;
        const int playBar = (int) (phase * kBars);
        const int activeSlice = (int) (phase / chopFrac);

        float energy = 0.0f;
        for (auto v : smooth) energy = juce::jmax (energy, v);

        for (int i = 0; i < kBars; ++i)
        {
            const float v = juce::jmax (smooth[(size_t) i * 2], smooth[(size_t) i * 2 + 1]);
            const float h = juce::jmax (1.5f, v * inner.getHeight() * 0.5f);
            const float x = inner.getX() + bw * (float) i;

            const int slice = (int) ((float) i / (float) kBars / chopFrac);
            const bool isActive = slice == activeSlice;
            const bool ahead = i > playBar;              // previous cycle, ghosted

            float alpha = ahead ? 0.28f : 0.85f;
            if (isActive && ! ahead) alpha = 1.0f;

            const float dist = std::abs ((float) (i - playBar));
            if (dist < 6.0f && ! ahead)
                alpha = juce::jmin (1.0f, alpha + (6.0f - dist) * 0.05f);

            alpha *= edgeFade (x + bw * 0.5f);
            g.setColour (accent.withAlpha (alpha * juce::jmax (0.35f, v + 0.3f)));
            g.fillRoundedRectangle (x + bw * 0.18f, midY - h, bw * 0.64f, h * 2.0f, bw * 0.3f);
        }

        // ---- active slice glow band ----
        if (energy > 0.02f)
        {
            const float sx = inner.getX() + inner.getWidth() * (float) activeSlice * chopFrac;
            const float sw = inner.getWidth() * chopFrac;
            juce::ColourGradient gl (accent.withAlpha (0.14f), sx, midY,
                                     accent.withAlpha (0.0f), sx, inner.getY(), false);
            g.setGradientFill (gl);
            g.fillRect (juce::Rectangle<float> (sx, r.getY() + 2.0f, sw, r.getHeight() - 4.0f));
        }

        // ---- playhead with bloom ----
        {
            const float x = inner.getX() + inner.getWidth() * phase;
            juce::ColourGradient bloom (accent.withAlpha (0.35f), x, midY,
                                        accent.withAlpha (0.0f), x + 26.0f, midY, true);
            g.setGradientFill (bloom);
            g.fillRect (juce::Rectangle<float> (x - 26.0f, r.getY(), 52.0f, r.getHeight()));
            g.setColour (accent.withAlpha (0.95f));
            g.fillRoundedRectangle (x - 1.0f, r.getY() + 3.0f, 2.0f, r.getHeight() - 6.0f, 1.0f);
        }

        // ---- idle scan shimmer when no audio ----
        if (energy < 0.02f)
        {
            const float x = inner.getX() + inner.getWidth() * idlePhase;
            juce::ColourGradient scan (accent.withAlpha (0.10f), x, midY,
                                       accent.withAlpha (0.0f), x + 60.0f, midY, true);
            g.setGradientFill (scan);
            g.fillRect (juce::Rectangle<float> (x - 60.0f, r.getY(), 120.0f, r.getHeight()));

            g.setColour (juce::Colours::white.withAlpha (0.22f));
            g.setFont (theme::font (11.0f, false));
            g.drawText (frozen ? "FROZEN" : "AWAITING SIGNAL", inner,
                        juce::Justification::centred);
        }
        else if (frozen)
        {
            g.setColour (accent.withAlpha (0.65f));
            g.setFont (theme::font (10.0f, true));
            g.drawText ("FROZEN", r.reduced (10.0f, 6.0f), juce::Justification::topRight);
        }

        g.restoreState();

        // hairline frame
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (r.reduced (0.75f), rad, 1.2f);
    }

private:
    ChopEngine& engine;
    std::array<float, ChopEngine::kVisSlots> bins {}, smooth {};
    float phase = 0.0f, chopFrac = 0.25f, idlePhase = 0.0f;
    bool frozen = false;
};
