#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/GeekEngine.h"
#include <array>
#include <map>

//==============================================================================
// The pixel screen. Everything is drawn on a dense fixed pixel grid (216
// cells across, like the approved screen mockups) into a small offscreen
// buffer at 3px per cell, then blitted to the component — fast, and the grid
// texture survives at any window size. Content: boot sequence, cartridge-swap
// static, header line, a textured mirrored waveform with per-theme dressing
// (syrup drips, smoke plumes, acid melt gradient, icicles, glitch), the wavy
// diamond-studded ground band, block meters, tolerance read-out, clickable
// AUTO tag, d-pad HUD, and an idle "trip": two pixel brawlers fighting.
// Text uses a tiny built-in 3x5 pixel font drawn at 2x so it reads like real
// handheld firmware.
//==============================================================================
class GeekDisplay : public juce::Component
{
public:
    explicit GeekDisplay (GeekEngine& e) : engine (e)
    {
        rng.setSeedRandomly();
        bootEnd = juce::Time::getMillisecondCounter() + 1500;
    }

    std::function<void()> onAutoToggle;

    struct State
    {
        int   theme = 0;
        float dose = 0.35f, outLevel = 0.0f, inLevel = 0.0f;
        bool  stutter = false, tape = false, autoOn = false, autoFiring = false;
        juce::String rateText { "1/8" };
    };

    void refresh (const State& s)
    {
        state = s;
        animPhase += 1.0f / 60.0f;

        if (s.inLevel > 0.02f) quietFrames = 0;
        else                   ++quietFrames;

        advanceParticles();
        if (quietFrames > idleAfterFrames)
            advanceFight (gridCols);
        repaint();
    }

    void startSwap()  { swapEnd = juce::Time::getMillisecondCounter() + 450; }

    void showHud (const juce::String& name, float value01)
    {
        hudName = name;
        hudValue = value01;
        hudEnd = juce::Time::getMillisecondCounter() + 900;
    }

    void flashMessage (const juce::String& msg)
    {
        flashText = msg;
        flashEnd = juce::Time::getMillisecondCounter() + 1600;
    }

    juce::Colour phosphor (int theme) const
    {
        switch (theme)
        {
            case 1:  return juce::Colour (0xff3ee06a);   // smoke  — green
            case 2:  return juce::Colour (0xffff8a1e);   // acid   — orange
            case 3:  return juce::Colour (0xff35c7f0);   // snow   — ice blue
            case 4:  return juce::Colour (0xffffd21e);   // geeked — yellow
            case 5:  return juce::Colour::fromHSV (std::fmod (animPhase * 0.25f, 1.0f),
                                                   0.85f, 1.0f, 1.0f); // overdose
            default: return juce::Colour (0xfffc22c3);   // lean   — hot pink
        }
    }

    static const char* themeName (int theme)
    {
        switch (theme)
        {
            case 1:  return "smoke";
            case 2:  return "acid";
            case 3:  return "snow";
            case 4:  return "geeked";
            case 5:  return "overdose";
            default: return "lean";
        }
    }

    static const char* themeDose (int theme)
    {
        switch (theme)
        {
            case 1:  return "3.5g";
            case 2:  return "1 tab";
            case 3:  return "8ball";
            case 4:  return "30mg";
            case 5:  return "all of it";
            default: return "2oz";
        }
    }

    // acid / overdose wave gradient: orange at the top, hot pink through the
    // middle, ice blue at the bottom (matches the approved screen mockups)
    static juce::Colour tripGradient (float t)
    {
        const juce::Colour orange (0xffff8a1e), pink (0xfffc22c3), blue (0xff35c7f0);
        return t < 0.5f ? orange.interpolatedWith (pink, t * 2.0f)
                        : pink.interpolatedWith (blue, (t - 0.5f) * 2.0f);
    }

    // Only the AUTO tag corner takes clicks; everything else stays a display.
    bool hitTest (int x, int y) override
    {
        return autoZone.contains (x, y);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (autoZone.contains (e.getPosition()) && onAutoToggle != nullptr)
            onAutoToggle();
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        const int cols = gridCols;
        const int rows = juce::jmax (60, (int) std::round ((float) gridCols
                             * (float) getHeight() / juce::jmax (1.0f, (float) getWidth())));

        if (! screenBuf.isValid()
            || screenBuf.getWidth() != cols * bufPx || screenBuf.getHeight() != rows * bufPx)
            screenBuf = juce::Image (juce::Image::ARGB, cols * bufPx, rows * bufPx, true);

        {
            juce::Graphics gi (screenBuf);
            gi.fillAll (juce::Colour (0xff050507));
            paintContent (gi, (float) bufPx, cols, rows);
        }

        g.saveState();
        g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
        g.drawImage (screenBuf, getLocalBounds().toFloat(),
                     juce::RectanglePlacement::stretchToFit, false);
        g.restoreState();

        paintGlassOverlay (g, (float) getWidth() / (float) cols, rows);

        // map the auto tag zone (set during content paint, in cell coords)
        const float k = (float) getWidth() / (float) cols;
        autoZone = { 0, (int) ((float) autoTagRow * k) - 4,
                     (int) (k * 4.0f * 8.0f) + 8, (int) (k * 7.0f) + 8 };
    }

private:
    static constexpr int gridCols = 216;
    static constexpr int bufPx = 3;               // buffer px per cell
    static constexpr int idleAfterFrames = 5 * 60;

    //==========================================================================
    void paintContent (juce::Graphics& g, float px, int cols, int rows)
    {
        const auto now = juce::Time::getMillisecondCounter();
        const auto col = phosphor (state.theme);

        if (now < bootEnd)   { paintBoot (g, px, cols, rows, now); return; }
        if (now < swapEnd)   { paintSwap (g, px, cols, rows); return; }

        const auto dim   = col.withMultipliedBrightness (0.55f);
        const auto faint = col.withMultipliedBrightness (0.28f);

        auto cell = [&] (float cx, float cy, juce::Colour c)
        {
            const float s = px * 0.82f;
            g.setColour (c);
            g.fillRect (cx * px + (px - s) * 0.5f, cy * px + (px - s) * 0.5f, s, s);
        };

        // ---- header line (2x text like the mockups)
        drawPixelText (g, px, 4, 4, juce::String (themeName (state.theme)) + " . "
                                        + themeDose (state.theme), col, 2);
        if (state.autoFiring)
            drawPixelText (g, px, cols - 4 - 8 * 10, 4, "geekin out",
                           ((int) (animPhase * 8.0f) & 1) ? juce::Colours::white : col, 2);
        else if (state.stutter)
            drawPixelText (g, px, cols - 4 - 8 * (5 + state.rateText.length()), 4,
                           "stut " + state.rateText, col, 2);
        else if (state.tape)
            drawPixelText (g, px, cols - 4 - 8 * 6, 4, "brakes", col, 2);

        const bool idle = quietFrames > idleAfterFrames;
        const int groundY = rows - 36;

        paintDecor  (g, px, cols, rows, col, dim, faint);
        paintGround (g, px, cols, groundY, col, dim, faint);

        if (idle)
            paintIdle (g, px, cols, rows, col, dim, faint, groundY);
        else
            paintWave (g, px, cols, rows, col, dim, faint, groundY);

        // ---- bottom rows: chunky L/R block meters + tolerance read-out
        const int metY1 = rows - 30, metY2 = rows - 21;
        const int metLen = 12;
        const float lvl = juce::jlimit (0.0f, 1.0f, state.outLevel * 1.4f);
        drawPixelText (g, px, 4, metY1 - 1, "l", col, 2);
        drawPixelText (g, px, 4, metY2 - 1, "r", col, 2);
        for (int i = 0; i < metLen; ++i)
        {
            const bool on1 = (float) i / metLen < lvl;
            const bool on2 = (float) i / metLen < lvl * 0.92f;
            for (int b = 0; b < 6; ++b)
                for (int wblk = 0; wblk < 5; ++wblk)
                {
                    cell ((float) (14 + i * 7 + wblk), (float) (metY1 + b), on1 ? col : faint);
                    cell ((float) (14 + i * 7 + wblk), (float) (metY2 + b), on2 ? dim : faint);
                }
        }

        drawPixelText (g, px, cols - 6 - 8 * 13, metY2 - 1,
                       "tolerance " + juce::String ((int) (state.dose * 100.0f)), col, 2);

        // ---- AUTO tag (clickable): dim when off, lit when armed, blinking
        //      brighter while the auto pilot is actually firing
        {
            autoTagRow = rows - 9;
            juce::Colour ac = state.autoOn ? col : faint;
            if (state.autoOn && state.autoFiring && ((int) (animPhase * 6.0f) & 1))
                ac = juce::Colours::white;
            drawPixelText (g, px, 4, autoTagRow, state.autoOn ? "auto on" : "auto", ac, 1);
        }

        // ---- d-pad HUD overlay
        if (now < hudEnd)
        {
            const int hy = 16;
            const int hw = 8 * (int) hudName.length() + 46;
            const int hx = (cols - hw) / 2;
            drawPixelText (g, px, hx, hy, hudName, col, 2);
            const int barX = hx + 8 * (int) hudName.length() + 6;
            for (int i = 0; i < 10; ++i)
            {
                const bool on = (float) i / 10.0f < hudValue;
                for (int b = 0; b < 5; ++b)
                    for (int wblk = 0; wblk < 3; ++wblk)
                        cell ((float) (barX + i * 4 + wblk), (float) (hy + 2 + b),
                              on ? col : faint);
            }
        }

        // ---- flash message (konami, etc.)
        if (now < flashEnd && ((int) (animPhase * 5.0f) & 1))
            drawPixelText (g, px, (cols - 8 * (int) flashText.length()) / 2, rows / 2 - 10,
                           flashText, juce::Colours::white, 2);
    }

    //==========================================================================
    // The wavy sparkle ground band from the mockups: a sine drift line, a
    // dithered fill underneath, and bright diamond gems embedded in the crest.
    void paintGround (juce::Graphics& g, float px, int cols, int groundY,
                      juce::Colour col, juce::Colour dim, juce::Colour faint)
    {
        auto cell = [&] (float cx, float cy, juce::Colour c)
        {
            const float s = px * 0.82f;
            g.setColour (c);
            g.fillRect (cx * px + (px - s) * 0.5f, cy * px + (px - s) * 0.5f, s, s);
        };

        for (int x = 4; x < cols - 4; ++x)
        {
            const float crest = std::sin (animPhase * 0.8f + (float) x * 0.085f) * 3.0f
                              + std::sin ((float) x * 0.022f) * 2.2f;
            const int top = groundY - 6 + (int) crest;
            cell ((float) x, (float) top, dim);                          // crest line
            for (int y = top + 1; y <= groundY + 3; ++y)                 // dither fill
                if (((x + y * 3) % 3) != 0)
                    cell ((float) x, (float) y,
                          faint.withMultipliedAlpha (0.4f + 0.5f * noise (x * 7 + y * 13)));
        }

        // embedded diamond gems, twinkling
        for (int k = 0; k < 13; ++k)
        {
            const int x = 10 + (k * 17 + (int) (noise (k * 91) * 11.0f)) % (cols - 20);
            const float crest = std::sin (animPhase * 0.8f + (float) x * 0.085f) * 3.0f
                              + std::sin ((float) x * 0.022f) * 2.2f;
            const int y = groundY - 3 + (int) crest;
            const float tw = 0.55f + 0.45f * std::sin (animPhase * 2.5f + (float) k * 1.7f);
            cell ((float) x, (float) y, juce::Colours::white.withMultipliedAlpha (tw));
            for (int r = 1; r <= 2; ++r)
            {
                const auto cc = col.withMultipliedAlpha (tw * (r == 1 ? 0.8f : 0.35f));
                cell ((float) x - r, (float) y, cc); cell ((float) x + r, (float) y, cc);
                cell ((float) x, (float) y - r, cc); cell ((float) x, (float) y + r, cc);
            }
        }
    }

    // Per-theme static dressing: acid gets pixel spirals in the corners,
    // snow gets frost creeping in from every corner.
    void paintDecor (juce::Graphics& g, float px, int cols, int rows,
                     juce::Colour col, juce::Colour dim, juce::Colour faint)
    {
        auto cell = [&] (float cx, float cy, juce::Colour c)
        {
            const float s = px * 0.82f;
            g.setColour (c);
            g.fillRect (cx * px + (px - s) * 0.5f, cy * px + (px - s) * 0.5f, s, s);
        };

        const int theme = state.theme;
        if (theme == 2 || theme == 5)   // spirals, slowly rotating
        {
            const float spin = animPhase * 0.6f;
            const struct { int x, y; } corners[3] = { { cols - 16, 14 },
                                                      { 14, rows / 2 + 8 },
                                                      { cols - 14, rows - 46 } };
            for (int c2 = 0; c2 < 3; ++c2)
                for (float a = 0.0f; a < 14.0f; a += 0.22f)
                {
                    const float r = 0.7f + a * 0.75f;
                    juce::Colour cc = theme == 5
                        ? juce::Colour::fromHSV (std::fmod (a * 0.06f + animPhase * 0.2f, 1.0f), 0.9f, 1.0f, 1.0f)
                        : dim;
                    cell ((float) corners[c2].x + std::cos (a + spin) * r,
                          (float) corners[c2].y + std::sin (a + spin) * r * 0.8f,
                          cc.withMultipliedAlpha (0.85f));
                }
        }
        else if (theme == 3)            // frost corners
        {
            for (int cy = 0; cy < 2; ++cy)
                for (int cx2 = 0; cx2 < 2; ++cx2)
                    for (int k = 0; k < 110; ++k)
                    {
                        const float u = noise (k * 17 + cx2 * 31 + cy * 57);
                        const float v = noise (k * 29 + cx2 * 13 + cy * 71);
                        const float dx = u * u * 24.0f, dy = v * v * 18.0f;
                        if (dx + dy > 26.0f) continue;
                        const float x = cx2 == 0 ? 3.0f + dx : (float) cols - 4.0f - dx;
                        const float y = cy == 0 ? 12.0f + dy : (float) rows - 5.0f - dy;
                        cell (x, y, juce::Colours::white.withMultipliedAlpha (
                                        0.25f + 0.35f * noise (k * 7)));
                    }
        }
        juce::ignoreUnused (col, faint);
    }

    //==========================================================================
    // Live scene: the waveform, dressed per cartridge to match the approved
    // screen mockups — lean drips syrup, smoke exhales textured plumes off the
    // wave, acid melts through an orange->pink->blue gradient with wiggling
    // tendrils, snow hangs icicles under a flurry, geeked glitches into
    // blocks. Overdose stacks the acid gradient on everything.
    void paintWave (juce::Graphics& g, float px, int cols, int rows,
                    juce::Colour col, juce::Colour dim, juce::Colour faint, int groundY)
    {
        auto cell = [&] (float cx, float cy, juce::Colour c)
        {
            const float s = px * 0.82f;
            g.setColour (c);
            g.fillRect (cx * px + (px - s) * 0.5f, cy * px + (px - s) * 0.5f, s, s);
        };

        juce::ignoreUnused (rows);
        const int theme = state.theme;
        const bool trippy  = theme == 2 || theme == 5;    // gradient + tendrils
        const bool glitchy = theme == 4 || theme == 5;    // blocky + tears
        const bool icy     = theme == 3;

        const int waveTop = 22, waveBot = groundY - 12;
        const int waveMid = (waveTop + waveBot) / 2;
        const int halfSpan = (waveBot - waveTop) / 2;

        auto waveColour = [&] (int y, float fade) -> juce::Colour
        {
            if (trippy)
            {
                const float t = juce::jlimit (0.0f, 1.0f,
                    ((float) y - (float) waveTop) / (float) (waveBot - waveTop)
                        + std::sin (animPhase * 0.7f) * 0.06f);
                return tripGradient (t).withMultipliedBrightness (0.7f + 0.3f * fade);
            }
            if (icy)
                return juce::Colours::white.interpolatedWith (col, 1.0f - fade * 0.7f)
                           .withMultipliedBrightness (0.75f + 0.25f * fade);
            return col.withMultipliedBrightness (fade);
        };

        const int w = engine.scopeWrite.load();
        const int span = cols - 8;
        for (int x = 4; x < cols - 4; ++x)
        {
            const int idx = (w + (int) engine.scope.size() * 12
                               - (span - (x - 4)) * (int) engine.scope.size() / span)
                            % (int) engine.scope.size();
            float v = juce::jlimit (0.0f, 1.0f, engine.scope[(size_t) idx] * 1.6f);

            // geeked: quantize the envelope into chunky blocks + column dropouts
            int xDraw = x;
            if (glitchy)
            {
                v = std::round (v * 5.0f) / 5.0f;
                if (noise (x * 41 + 5) > 0.94f) continue;                 // dropout
                if (noise (x * 17 + 9) > 0.90f)
                    xDraw = x + (int) ((noise (x * 23) - 0.5f) * 7.0f);   // tear
            }

            const int h = juce::jmax (1, (int) (v * (float) halfSpan));
            for (int dy = 0; dy < h; ++dy)
            {
                const float fade = 1.0f - 0.5f * (float) dy / (float) juce::jmax (1, halfSpan);
                const float knit = 0.78f + 0.22f * noise (x * 5 + dy * 11);
                cell ((float) xDraw, (float) (waveMid - dy),
                      waveColour (waveMid - dy, fade).withMultipliedBrightness (knit));
                cell ((float) xDraw, (float) (waveMid + dy),
                      waveColour (waveMid + dy, fade * 0.9f).withMultipliedBrightness (knit));
            }
            if (glitchy && v > 0.1f && noise (x * 29 + 1) > 0.93f)        // hot pixel
                cell ((float) xDraw, (float) (waveMid - h - 1), juce::Colours::white);

            // ---- per-theme dressing hanging off / rising from the wave ----
            const float seed = noise (x * 31 + 7);

            if ((theme == 0 || theme == 5) && v > 0.2f && seed > 0.78f)
            {   // lean: syrup drips
                const int trail = (int) (v * 18.0f * noise (x * 13 + 3)) + 3;
                for (int t = 0; t < trail; ++t)
                    cell ((float) x, (float) (waveMid + h + t),
                          dim.withMultipliedAlpha (1.0f - (float) t / (float) trail));
            }

            if (trippy && v > 0.15f && seed > 0.72f)
            {   // acid: melting tendrils wiggling to the ground
                const int reach = juce::jmin (groundY - 8 - (waveMid + h),
                                              (int) (v * 42.0f * noise (x * 13 + 3)) + 8);
                for (int t = 0; t < reach; ++t)
                {
                    const int y = waveMid + h + t;
                    const float wig = std::sin ((float) y * 0.20f + animPhase * 2.0f
                                                + (float) x * 0.7f)
                                      * (1.0f + (float) t * 0.09f);
                    const float tt = juce::jlimit (0.0f, 1.0f,
                        ((float) y - (float) waveTop) / (float) (waveBot - waveTop));
                    cell ((float) x + wig, (float) y,
                          tripGradient (tt).withMultipliedAlpha (
                              0.9f - 0.5f * (float) t / (float) juce::jmax (1, reach)));
                }
            }

            if (theme == 1 && v > 0.18f && seed > 0.74f)
            {   // smoke: textured plume rising off the wave crest
                const int reach = juce::jmin (waveMid - h - waveTop + 8,
                                              (int) (v * 38.0f * noise (x * 13 + 3)) + 10);
                for (int t = 0; t < reach; ++t)
                {
                    const int y = waveMid - h - t;
                    const float swy = std::sin ((float) y * 0.16f + animPhase * 1.6f
                                                + (float) x * 0.5f)
                                      * (1.0f + (float) t * 0.12f);
                    const float a = 0.8f - 0.6f * (float) t / (float) juce::jmax (1, reach);
                    const float wid = 1.0f + (float) t * 0.14f;
                    for (int dx = (int) -wid; dx <= (int) wid; ++dx)
                        if (noise (x * 3 + t * 7 + dx * 13) > 0.42f)
                            cell ((float) x + swy + (float) dx, (float) y,
                                  dim.withMultipliedAlpha (a * (0.5f + 0.5f * noise (t * 5 + dx))));
                }
            }

            if (icy && v > 0.2f && seed > 0.80f)
            {   // snow: icicles hanging under the wave
                const int len = (int) (v * 20.0f * noise (x * 13 + 3)) + 4;
                for (int t = 0; t < len; ++t)
                    cell ((float) x, (float) (waveMid + h + t),
                          juce::Colours::white.withMultipliedAlpha (
                              0.85f - 0.6f * (float) t / (float) len));
                cell ((float) x, (float) (waveMid + h + len),
                      col.withMultipliedAlpha (0.5f));
            }
        }

        // lean: wobbling syrup pool sitting on the ground band
        if (theme == 0 || theme == 5)
            for (int x = 4; x < cols - 4; ++x)
            {
                const float wob = std::sin (animPhase * 2.2f + (float) x * 0.13f) * 1.6f;
                cell ((float) x, (float) groundY - 9 + wob, dim);
                cell ((float) x, (float) groundY - 8 + wob, faint);
            }

        drawParticles (g, px, col, dim, faint);
    }

    // shared particle renderer (kinds: 0 drip/debris, 1 diamond, 2 puff,
    // 3 flake, 4 hollow square)
    void drawParticles (juce::Graphics& g, float px, juce::Colour col,
                        juce::Colour dim, juce::Colour faint)
    {
        auto cell = [&] (float cx, float cy, juce::Colour c)
        {
            const float s = px * 0.82f;
            g.setColour (c);
            g.fillRect (cx * px + (px - s) * 0.5f, cy * px + (px - s) * 0.5f, s, s);
        };

        const bool trippy = state.theme == 2 || state.theme == 5;
        for (const auto& d : particles)
            if (d.alive)
            {
                const float a = juce::jlimit (0.2f, 1.0f, d.life);
                juce::Colour c = col;
                if (trippy)
                    c = juce::Colour::fromHSV (std::fmod (d.hue + animPhase * 0.3f, 1.0f),
                                               0.9f, 1.0f, 1.0f);
                switch (d.kind)
                {
                    case 1:   // diamond sparkle
                        cell (d.x, d.y, c.withMultipliedAlpha (a));
                        cell (d.x - 1, d.y, faint); cell (d.x + 1, d.y, faint);
                        cell (d.x, d.y - 1, faint); cell (d.x, d.y + 1, faint);
                        break;
                    case 2:   // smoke puff: grows as it fades
                    {
                        const int r = 1 + (int) ((1.0f - d.life) * 5.0f);
                        cell (d.x, d.y, dim.withMultipliedAlpha (a));
                        for (int k = 1; k <= r; ++k)
                        {
                            const auto f = faint.withMultipliedAlpha (a * (1.0f - (float) k / (r + 1.0f)));
                            cell (d.x - k, d.y, f); cell (d.x + k, d.y, f);
                            cell (d.x, d.y - k, f);
                        }
                        break;
                    }
                    case 3:   // snowflake
                        cell (d.x, d.y, juce::Colours::white.withMultipliedAlpha (a * 0.9f));
                        cell (d.x, d.y + 1, c.withMultipliedAlpha (a * 0.4f));
                        break;
                    case 4:   // floating hollow square (mockup confetti)
                        cell (d.x - 1, d.y - 1, faint); cell (d.x, d.y - 1, faint); cell (d.x + 1, d.y - 1, faint);
                        cell (d.x - 1, d.y, faint);                                 cell (d.x + 1, d.y, faint);
                        cell (d.x - 1, d.y + 1, faint); cell (d.x, d.y + 1, faint); cell (d.x + 1, d.y + 1, faint);
                        break;
                    default:  // drip / debris / pill
                        cell (d.x, d.y, dim.withMultipliedAlpha (a));
                        if (state.theme >= 4)
                            cell (d.x + 1, d.y, faint.withMultipliedAlpha (a));
                        break;
                }
            }
    }

    //==========================================================================
    // Awaiting-signal scene: theme weather + two pixel brawlers scrapping on
    // the ground line, with knockbacks and pow flashes. Prompt blinks up top.
    void paintIdle (juce::Graphics& g, float px, int cols, int rows,
                    juce::Colour col, juce::Colour dim, juce::Colour faint, int groundY)
    {
        auto cell = [&] (float cx, float cy, juce::Colour c)
        {
            const float s = px * 0.82f;
            g.setColour (c);
            g.fillRect (cx * px + (px - s) * 0.5f, cy * px + (px - s) * 0.5f, s, s);
        };

        drawParticles (g, px, col, dim, faint);

        // lean/overdose: syrup pool slowly rising the longer it waits
        if (state.theme == 0 || state.theme == 5)
        {
            const int rise = juce::jmin (8, (quietFrames - idleAfterFrames) / 240);
            for (int x = 4; x < cols - 4; ++x)
            {
                const float wob = std::sin (animPhase * 2.0f + (float) x * 0.15f) * 1.6f;
                const int top = groundY - 9 - rise + (int) wob;
                for (int y = top; y <= groundY - 7; ++y)
                    cell ((float) x, (float) y, y == top ? dim : faint);
            }
        }

        // snow/overdose: snow piles up on the ground band
        if (state.theme == 3 || state.theme == 5)
        {
            const int pile = juce::jmin (7, (quietFrames - idleAfterFrames) / 300);
            for (int x = 4; x < cols - 4; ++x)
            {
                const int h = 1 + (int) (noise (x * 3) * (float) pile);
                for (int dy = 0; dy < h; ++dy)
                    cell ((float) x, (float) (groundY - 8 - dy),
                          juce::Colours::white.withMultipliedAlpha (0.55f));
            }
        }

        // ---- the trip: two pixel dudes fighting on the ground line
        {
            const int floorRow = groundY - 7;
            const juce::Colour c1 = state.theme == 2 || state.theme == 5
                ? tripGradient (std::fmod (animPhase * 0.3f, 1.0f)) : col;
            const juce::Colour c2 = state.theme == 3 ? dim : juce::Colours::white
                                        .withMultipliedAlpha (0.85f);
            drawFighter (g, px, fight.x1, floorRow, fight.pose1, c1, true);
            drawFighter (g, px, fight.x2, floorRow, fight.pose2, c2, false);

            if (fight.powT > 0)
            {
                const float powRow = (float) floorRow - 44.0f;
                const juce::Colour pc = ((int) (animPhase * 10.0f) & 1)
                                            ? juce::Colours::white : col;
                drawPixelText (g, px, (int) fight.powX - 4 * (int) fight.powWord.length(),
                               (int) powRow, fight.powWord, pc, 2);
                // impact star between them
                cell (fight.powX, powRow + 13.0f, juce::Colours::white);
                cell (fight.powX - 2, powRow + 11.0f, pc); cell (fight.powX + 2, powRow + 11.0f, pc);
                cell (fight.powX - 2, powRow + 15.0f, pc); cell (fight.powX + 2, powRow + 15.0f, pc);
            }
        }

        // per-cartridge prompt
        const char* prompt = "feed me a vocal";
        switch (state.theme)
        {
            case 0:  prompt = "pour up a vocal";   break;
            case 1:  prompt = "spark up a vocal";  break;
            case 2:  prompt = "drop a vocal tab";  break;
            case 3:  prompt = "line up a vocal";   break;
            case 4:  prompt = "pop a vocal in";    break;
            case 5:  prompt = "feed the machine";  break;
        }
        if (((int) (animPhase * 1.4f) & 1) == 0)
        {
            const int len = (int) juce::String (prompt).length();
            drawPixelText (g, px, (cols - 8 * len) / 2, 18, prompt, col, 2);
        }
        juce::ignoreUnused (rows);
    }

    //==========================================================================
    // Pixel brawlers: 7x10 sprites, four poses, drawn at 3-cell scale.
    static const juce::uint8* fighterPose (int pose)
    {
        static const juce::uint8 stand[10] = { 0b0011000, 0b0011000, 0b0001000, 0b0111100,
                                               0b1011010, 0b0011000, 0b0011000, 0b0010100,
                                               0b0010100, 0b0110110 };
        static const juce::uint8 walk[10]  = { 0b0011000, 0b0011000, 0b0001000, 0b0111100,
                                               0b1011010, 0b0011000, 0b0011000, 0b0010100,
                                               0b0100010, 0b1100011 };
        static const juce::uint8 punch[10] = { 0b0011000, 0b0011000, 0b0001000, 0b0111111,
                                               0b1011000, 0b0011000, 0b0011000, 0b0010100,
                                               0b0010100, 0b0110110 };
        static const juce::uint8 hit[10]   = { 0b0001100, 0b0001100, 0b0000100, 0b0011110,
                                               0b0101101, 0b0001100, 0b0011000, 0b0010100,
                                               0b0100100, 0b1100110 };
        switch (pose)
        {
            case 1:  return walk;
            case 2:  return punch;
            case 3:  return hit;
            default: return stand;
        }
    }

    void drawFighter (juce::Graphics& g, float px, float xCentre, int floorRow,
                      int pose, juce::Colour colour, bool faceRight)
    {
        const auto* rows10 = fighterPose (pose);
        constexpr int scale = 3;
        g.setColour (colour);
        for (int r = 0; r < 10; ++r)
            for (int c = 0; c < 7; ++c)
            {
                const int bit = faceRight ? c : 6 - c;
                if (rows10[r] & (0b1000000 >> bit))
                    g.fillRect ((xCentre + (float) ((c - 3) * scale)) * px,
                                ((float) (floorRow - (10 - r) * scale)) * px,
                                px * 0.82f * scale, px * 0.82f * scale);
            }
    }

    struct Fight
    {
        float x1 = 50.0f, x2 = 170.0f;
        int pose1 = 0, pose2 = 0, t1 = 0, t2 = 0;
        int cool = 90, powT = 0;
        float powX = 0, powY = 0;
        juce::String powWord { "pow" };
    };

    void advanceFight (int cols)
    {
        auto& f = fight;
        if (f.t1 > 0 && --f.t1 == 0) f.pose1 = 0;
        if (f.t2 > 0 && --f.t2 == 0) f.pose2 = 0;
        if (f.powT > 0) --f.powT;
        if (f.cool > 0) --f.cool;

        const float gap = f.x2 - f.x1;

        // knockback while in the hit pose
        if (f.pose1 == 3) f.x1 -= 1.0f;
        if (f.pose2 == 3) f.x2 += 1.0f;

        // approach when nobody is mid-move
        if (f.pose1 == 0 && f.pose2 == 0 && gap > 22.0f)
        {
            f.x1 += 0.42f; f.x2 -= 0.42f;
            const int step = ((int) (animPhase * 6.0f) & 1);
            f.pose1 = step; f.pose2 = 1 - step;
            if (f.pose1 == 1) f.t1 = 1;
            if (f.pose2 == 1) f.t2 = 1;
        }

        // swing when close
        if (gap <= 22.0f && f.cool == 0 && f.pose1 != 3 && f.pose2 != 3)
        {
            static const char* words[] = { "pow", "bap", "smack", "oof" };
            f.powWord = words[rng.nextInt (4)];
            if (rng.nextBool())  { f.pose1 = 2; f.t1 = 14; f.pose2 = 3; f.t2 = 22; }
            else                 { f.pose2 = 2; f.t2 = 14; f.pose1 = 3; f.t1 = 22; }
            f.powX = (f.x1 + f.x2) * 0.5f;
            f.powT = 34;
            f.cool = 60 + rng.nextInt (80);
        }

        f.x1 = juce::jlimit (16.0f, (float) cols - 40.0f, f.x1);
        f.x2 = juce::jlimit (40.0f, (float) cols - 16.0f, f.x2);
        if (f.x2 - f.x1 < 19.0f) { f.x1 -= 0.5f; f.x2 += 0.5f; }
    }

    //==========================================================================
    void paintBoot (juce::Graphics& g, float px, int cols, int rows, juce::uint32 now)
    {
        const auto col = phosphor (state.theme);
        const float t = 1.0f - (float) (bootEnd - now) / 1500.0f;   // 0..1

        // logo drops in
        const int targetY = rows / 2 - 12;
        const int y = (int) juce::jmap (juce::jmin (1.0f, t * 2.2f), (float) -12, (float) targetY);
        drawPixelText (g, px, (cols - 12 * 9) / 2, y, "vocalgeek", col, 3);

        if (t > 0.45f)
            drawPixelText (g, px, (cols - 8 * 12) / 2, targetY + 20, "dose console",
                           col.withMultipliedBrightness (0.6f), 2);

        // progress cells
        const int done = (int) (juce::jlimit (0.0f, 1.0f, (t - 0.2f) / 0.7f) * 20.0f);
        for (int i = 0; i < 20; ++i)
        {
            g.setColour (i < done ? col : col.withMultipliedBrightness (0.25f));
            const float bx = ((float) cols / 2.0f - 30.0f + (float) i * 3.0f) * px;
            g.fillRect (bx, (float) (targetY + 34) * px, px * 2.5f, px * 2.5f);
        }
    }

    void paintSwap (juce::Graphics& g, float px, int cols, int rows)
    {
        const auto col = phosphor (state.theme);
        // one frame of static
        for (int i = 0; i < 700; ++i)
        {
            const float x = rng.nextFloat() * (float) cols;
            const float y = rng.nextFloat() * (float) rows;
            g.setColour (col.withMultipliedAlpha (rng.nextFloat() * 0.5f));
            g.fillRect (x * px, y * px, px * 0.85f, px * 0.85f);
        }
        drawPixelText (g, px, (cols - 8 * 15) / 2, rows / 2 - 5, "loading dose...", col, 2);
    }

    // faint scanlines + corner vignette so the grid reads as a lit LCD.
    // Drawn on the component (not the buffer) so they stay hairline-thin.
    void paintGlassOverlay (juce::Graphics& g, float cellPx, int rows)
    {
        g.setColour (juce::Colours::black.withAlpha (0.13f));
        for (int y = 0; y < rows; y += 2)
            g.fillRect (0.0f, (float) y * cellPx + cellPx * 0.5f, (float) getWidth(), cellPx * 0.5f);

        juce::ColourGradient vg (juce::Colours::transparentBlack,
                                 (float) getWidth() * 0.5f, (float) getHeight() * 0.45f,
                                 juce::Colours::black.withAlpha (0.35f),
                                 0.0f, 0.0f, true);
        g.setGradientFill (vg);
        g.fillRect (getLocalBounds());
    }

    //==========================================================================
    // 3x5 pixel font (lowercase letters, digits, few symbols) — each glyph is
    // five rows of 3 bits, top to bottom.
    static const juce::uint8* glyph (juce::juce_wchar c)
    {
        static const std::map<juce::juce_wchar, std::array<juce::uint8, 5>> font =
        {
            { 'a', {{ 0b010,0b101,0b111,0b101,0b101 }} }, { 'b', {{ 0b110,0b101,0b110,0b101,0b110 }} },
            { 'c', {{ 0b011,0b100,0b100,0b100,0b011 }} }, { 'd', {{ 0b110,0b101,0b101,0b101,0b110 }} },
            { 'e', {{ 0b111,0b100,0b110,0b100,0b111 }} }, { 'f', {{ 0b111,0b100,0b110,0b100,0b100 }} },
            { 'g', {{ 0b011,0b100,0b101,0b101,0b011 }} }, { 'h', {{ 0b101,0b101,0b111,0b101,0b101 }} },
            { 'i', {{ 0b111,0b010,0b010,0b010,0b111 }} }, { 'j', {{ 0b001,0b001,0b001,0b101,0b010 }} },
            { 'k', {{ 0b101,0b110,0b100,0b110,0b101 }} }, { 'l', {{ 0b100,0b100,0b100,0b100,0b111 }} },
            { 'm', {{ 0b101,0b111,0b111,0b101,0b101 }} }, { 'n', {{ 0b101,0b111,0b111,0b111,0b101 }} },
            { 'o', {{ 0b010,0b101,0b101,0b101,0b010 }} }, { 'p', {{ 0b110,0b101,0b110,0b100,0b100 }} },
            { 'q', {{ 0b010,0b101,0b101,0b110,0b011 }} }, { 'r', {{ 0b110,0b101,0b110,0b110,0b101 }} },
            { 's', {{ 0b011,0b100,0b010,0b001,0b110 }} }, { 't', {{ 0b111,0b010,0b010,0b010,0b010 }} },
            { 'u', {{ 0b101,0b101,0b101,0b101,0b111 }} }, { 'v', {{ 0b101,0b101,0b101,0b101,0b010 }} },
            { 'w', {{ 0b101,0b101,0b111,0b111,0b101 }} }, { 'x', {{ 0b101,0b101,0b010,0b101,0b101 }} },
            { 'y', {{ 0b101,0b101,0b010,0b010,0b010 }} }, { 'z', {{ 0b111,0b001,0b010,0b100,0b111 }} },
            { '0', {{ 0b010,0b101,0b101,0b101,0b010 }} }, { '1', {{ 0b010,0b110,0b010,0b010,0b111 }} },
            { '2', {{ 0b110,0b001,0b010,0b100,0b111 }} }, { '3', {{ 0b110,0b001,0b010,0b001,0b110 }} },
            { '4', {{ 0b101,0b101,0b111,0b001,0b001 }} }, { '5', {{ 0b111,0b100,0b110,0b001,0b110 }} },
            { '6', {{ 0b011,0b100,0b110,0b101,0b010 }} }, { '7', {{ 0b111,0b001,0b010,0b010,0b010 }} },
            { '8', {{ 0b010,0b101,0b010,0b101,0b010 }} }, { '9', {{ 0b010,0b101,0b011,0b001,0b110 }} },
            { '.', {{ 0b000,0b000,0b000,0b000,0b010 }} }, { '/', {{ 0b001,0b001,0b010,0b100,0b100 }} },
            { '-', {{ 0b000,0b000,0b111,0b000,0b000 }} },
        };
        const auto it = font.find (c);
        return it != font.end() ? it->second.data() : nullptr;
    }

    void drawPixelText (juce::Graphics& g, float px, int col, int row,
                        const juce::String& text, juce::Colour colour, int scale)
    {
        g.setColour (colour);
        int x = col;
        for (const auto c : text)
        {
            if (c == ' ') { x += 2 * scale; continue; }
            if (const auto* rowsBits = glyph (juce::CharacterFunctions::toLowerCase (c)))
            {
                for (int ry = 0; ry < 5; ++ry)
                    for (int rx = 0; rx < 3; ++rx)
                        if (rowsBits[ry] & (0b100 >> rx))
                            g.fillRect (((float) x + (float) (rx * scale)) * px,
                                        ((float) row + (float) (ry * scale)) * px,
                                        px * 0.82f * (float) scale, px * 0.82f * (float) scale);
            }
            x += 4 * scale;
        }
    }

    // cheap deterministic per-column noise
    float noise (int seed) const
    {
        juce::uint32 h = (juce::uint32) seed * 2654435761u + (juce::uint32) (animPhase * 2.0f);
        h ^= h >> 13; h *= 0x5bd1e995; h ^= h >> 15;
        return (float) (h & 0xffff) / 65535.0f;
    }

    //==========================================================================
    struct Particle { float x = 0, y = 0, vx = 0, vy = 0, life = 0, hue = 0;
                      int kind = 0; bool alive = false; };

    void advanceParticles()
    {
        const int cols = gridCols;
        const int rows = juce::jmax (60, (int) std::round ((float) gridCols
                             * (float) getHeight() / juce::jmax (1.0f, (float) getWidth())));
        const int groundY = rows - 36;

        const bool idle = quietFrames > idleAfterFrames;
        const float level = juce::jlimit (0.0f, 1.0f, state.outLevel * 1.4f);

        auto spawn = [&] (float x, float y, float vx, float vy, int kind)
        {
            for (auto& d : particles)
                if (! d.alive)
                {
                    d = { x, y, vx, vy, 1.0f, rng.nextFloat(), kind, true };
                    return;
                }
        };

        const int theme = state.theme;
        const float boost = idle ? 1.0f : level;   // idle runs full tilt

        // per-theme weather, live AND idle (idle just goes harder)
        if (idle || level > 0.05f)
        {
            switch (theme)
            {
                case 0:   // lean: syrup drips crawling down
                    if (rng.nextFloat() < 0.40f * boost)
                        spawn (6.0f + rng.nextFloat() * (float) (cols - 12),
                               idle ? 16.0f : (float) rows * 0.5f,
                               0.0f, 0.12f + rng.nextFloat() * 0.18f, 0);
                    break;
                case 1:   // smoke: puffs exhaled upward
                    if (rng.nextFloat() < 0.6f * boost)
                        spawn (12.0f + rng.nextFloat() * (float) (cols - 24),
                               idle ? (float) groundY - 8.0f
                                    : (float) rows * 0.5f - rng.nextFloat() * 10.0f,
                               (rng.nextFloat() - 0.5f) * 0.2f,
                               -(0.22f + rng.nextFloat() * 0.3f), 2);
                    break;
                case 3:   // snow: constant flurry from the top
                    for (int rep = 0; rep < 2; ++rep)
                        if (rng.nextFloat() < 0.9f * juce::jmax (0.4f, boost))
                            spawn (rng.nextFloat() * (float) cols, 11.0f,
                                   0.0f, 0.25f + rng.nextFloat() * 0.35f, 3);
                    break;
                default: break;
            }

            if (theme == 2 || theme == 5)   // acid/overdose: orbiting colour sparks
            {
                if (rng.nextFloat() < 0.9f * juce::jmax (0.4f, boost))
                {
                    const float a = animPhase * 2.2f + rng.nextFloat() * 6.28f;
                    const float r = 8.0f + std::fmod (animPhase * 14.0f
                                                      + rng.nextFloat() * 34.0f, 42.0f);
                    spawn ((float) cols / 2.0f + std::cos (a) * r * 1.5f,
                           (float) rows / 2.0f + std::sin (a) * r * 0.75f,
                           std::cos (a + 1.57f) * 0.4f, std::sin (a + 1.57f) * 0.22f, 1);
                }
            }

            if (theme >= 4 && idle)         // geeked idle: pills popping like popcorn
            {
                if (rng.nextFloat() < 0.30f)
                    spawn (20.0f + rng.nextFloat() * (float) (cols - 40), (float) groundY - 4.0f,
                           (rng.nextFloat() - 0.5f) * 1.1f, -(1.0f + rng.nextFloat() * 0.9f), 0);
            }

            if (! idle && rng.nextFloat() < level * 0.4f)    // sparkles near the wave
                spawn (8.0f + rng.nextFloat() * (float) (cols - 16),
                       (float) rows * 0.28f + rng.nextFloat() * (float) rows * 0.4f,
                       0.0f, 0.03f, 1);

            if (rng.nextFloat() < 0.12f)                     // floating hollow squares
                spawn (8.0f + rng.nextFloat() * (float) (cols - 16),
                       14.0f + rng.nextFloat() * (float) (rows - 60),
                       (rng.nextFloat() - 0.5f) * 0.06f, -0.02f - rng.nextFloat() * 0.05f, 4);
        }

        for (auto& d : particles)
            if (d.alive)
            {
                d.x += d.vx;
                d.y += d.vy;
                if (d.kind == 2) d.x += std::sin (animPhase * 3.0f + d.y * 0.3f) * 0.25f;  // puff sway
                if (d.kind == 3) d.x += std::sin (animPhase * 2.0f + d.y * 0.2f) * 0.4f;   // flake sway
                if (theme >= 4 && idle && d.kind == 0) d.vy += 0.055f;   // popcorn gravity

                d.life -= d.kind == 2 ? 0.012f
                        : d.kind == 3 ? 0.004f
                        : (d.kind == 1 && ! idle) ? 0.05f
                        : (theme == 0 ? 0.008f : 0.02f);
                if (d.life <= 0.0f || d.y > (float) groundY - 6.0f || d.y < 7.0f
                    || d.x < 3.0f || d.x > (float) cols - 3.0f)
                    d.alive = false;
            }
    }

    GeekEngine& engine;
    State state;
    Fight fight;
    std::array<Particle, 224> particles;
    juce::Random rng;
    float animPhase = 0.0f;
    int quietFrames = 0;

    juce::Image screenBuf;
    int autoTagRow = 0;

    juce::uint32 bootEnd = 0, swapEnd = 0, hudEnd = 0, flashEnd = 0;
    juce::String hudName;
    float hudValue = 0.0f;
    juce::String flashText;
    juce::Rectangle<int> autoZone;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GeekDisplay)
};
