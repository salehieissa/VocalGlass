#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/GeekEngine.h"
#include <array>
#include <map>

//==============================================================================
// The pixel screen. Everything is drawn on a fine fixed pixel grid in the
// theme's phosphor colour: a boot sequence, cartridge-swap static, a header
// line ("lean . 2oz"), a dense mirrored waveform with drip trails and diamond
// sparkles, a textured ground band, block meters, a "tolerance" (dose)
// read-out, a clickable AUTO tag, a d-pad HUD overlay, and per-theme idle
// animations when no vocal is feeding it. Text uses a tiny built-in 3x5 pixel
// font so it reads like real handheld firmware.
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
            case 3:  return "1g";
            case 4:  return "30mg";
            case 5:  return "all of it";
            default: return "2oz";
        }
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
        g.fillAll (juce::Colour (0xff050507));

        const float px = juce::jmax (1.5f, (float) getWidth() / (float) gridCols);
        const int cols = gridCols;
        const int rows = (int) ((float) getHeight() / px);
        const auto now = juce::Time::getMillisecondCounter();

        const auto col = phosphor (state.theme);

        if (now < bootEnd)   { paintBoot (g, px, cols, rows, now); return; }
        if (now < swapEnd)   { paintSwap (g, px, cols, rows); return; }

        const auto dim   = col.withMultipliedBrightness (0.55f);
        const auto faint = col.withMultipliedBrightness (0.28f);

        auto cell = [&] (float cx, float cy, juce::Colour c, float scale = 1.0f)
        {
            const float s = px * 0.88f * scale;
            g.setColour (c);
            g.fillRect (cx * px + (px - s) * 0.5f, cy * px + (px - s) * 0.5f, s, s);
        };
        auto diamond = [&] (float cx, float cy, juce::Colour c)
        {
            cell (cx, cy, c); 
            cell (cx - 1, cy, c.withMultipliedAlpha (0.6f));
            cell (cx + 1, cy, c.withMultipliedAlpha (0.6f));
            cell (cx, cy - 1, c.withMultipliedAlpha (0.6f));
            cell (cx, cy + 1, c.withMultipliedAlpha (0.6f));
        };

        // ---- header line
        drawPixelText (g, px, 3, 3, juce::String (themeName (state.theme)) + " . "
                                        + themeDose (state.theme), col, 1);
        if (state.stutter)
            drawPixelText (g, px, cols - 4 - 4 * (5 + state.rateText.length()), 3,
                           "stut " + state.rateText, col, 1);
        else if (state.tape)
            drawPixelText (g, px, cols - 4 - 4 * 6, 3, "brakes", col, 1);

        const bool idle = quietFrames > 5 * 60;

        // ---- ground band (textured floor like the mockup)
        const int groundY = rows - 26;
        for (int x = 3; x < cols - 3; ++x)
        {
            const float nse = noise (x * 7 + 13);
            const int h = 1 + (int) (nse * 2.9f);
            for (int dy = 0; dy < h; ++dy)
                cell ((float) x, (float) (groundY - dy),
                      faint.withMultipliedAlpha (0.5f + 0.5f * nse));
        }

        juce::ignoreUnused (diamond);
        if (idle)
            paintIdle (g, px, cols, rows, col, dim, faint, groundY);
        else
            paintWave (g, px, cols, rows, col, dim, faint, groundY);

        // ---- bottom rows: L/R block meters + tolerance read-out
        const int metY1 = rows - 18, metY2 = rows - 12;
        const int metLen = 18;
        const float lvl = juce::jlimit (0.0f, 1.0f, state.outLevel * 1.4f);
        drawPixelText (g, px, 3, metY1 - 1, "l", col, 1);
        drawPixelText (g, px, 3, metY2 - 1, "r", col, 1);
        for (int i = 0; i < metLen; ++i)
        {
            const bool on1 = (float) i / metLen < lvl;
            const bool on2 = (float) i / metLen < lvl * 0.92f;
            for (int b = 0; b < 3; ++b)
            {
                cell ((float) (8 + i * 3),     (float) (metY1 + b), on1 ? col : faint);
                cell ((float) (8 + i * 3) + 1, (float) (metY1 + b), on1 ? col : faint);
                cell ((float) (8 + i * 3),     (float) (metY2 + b), on2 ? dim : faint);
                cell ((float) (8 + i * 3) + 1, (float) (metY2 + b), on2 ? dim : faint);
            }
        }

        drawPixelText (g, px, cols - 4 - 4 * 13, metY2 - 1,
                       "tolerance " + juce::String ((int) (state.dose * 100.0f)), col, 1);

        // ---- AUTO tag (clickable): dim when off, lit when armed, blinking
        //      brighter while the auto pilot is actually firing
        {
            const int ay = rows - 6;
            juce::Colour ac = state.autoOn ? col : faint;
            if (state.autoOn && state.autoFiring && ((int) (animPhase * 6.0f) & 1))
                ac = juce::Colours::white;
            drawPixelText (g, px, 3, ay, state.autoOn ? "auto on" : "auto", ac, 1);
            autoZone = juce::Rectangle<int> (0, (int) ((float) (ay - 2) * px),
                                             (int) (px * 4 * 8), (int) (px * 9));
        }

        // ---- d-pad HUD overlay
        if (now < hudEnd)
        {
            const int hy = 12;
            const int hw = 4 * (int) hudName.length() + 24;
            const int hx = (cols - hw) / 2;
            drawPixelText (g, px, hx, hy, hudName, col, 1);
            const int barX = hx + 4 * (int) hudName.length() + 3;
            for (int i = 0; i < 10; ++i)
            {
                const bool on = (float) i / 10.0f < hudValue;
                for (int b = 0; b < 2; ++b)
                {
                    cell ((float) (barX + i * 2), (float) (hy + 1 + b), on ? col : faint);
                    cell ((float) (barX + i * 2 + 1), (float) (hy + 1 + b), on ? col : faint);
                }
            }
        }

        // ---- flash message (konami, etc.)
        if (now < flashEnd && ((int) (animPhase * 5.0f) & 1))
            drawPixelText (g, px, (cols - 4 * (int) flashText.length()) / 2, rows / 2 - 8,
                           flashText, juce::Colours::white, 1);

        paintGlassOverlay (g, px, rows);
    }

private:
    static constexpr int gridCols = 150;

    //==========================================================================
    void paintWave (juce::Graphics& g, float px, int cols, int rows,
                    juce::Colour col, juce::Colour dim, juce::Colour faint, int groundY)
    {
        auto cell = [&] (float cx, float cy, juce::Colour c)
        {
            const float s = px * 0.88f;
            g.setColour (c);
            g.fillRect (cx * px + (px - s) * 0.5f, cy * px + (px - s) * 0.5f, s, s);
        };

        juce::ignoreUnused (rows);
        const int waveTop = 14, waveBot = groundY - 8;
        const int waveMid = (waveTop + waveBot) / 2;
        const int halfSpan = (waveBot - waveTop) / 2;

        const int w = engine.scopeWrite.load();
        const int span = cols - 6;
        for (int x = 3; x < cols - 3; ++x)
        {
            const int idx = (w + (int) engine.scope.size() * 8
                               - (span - (x - 3)) * (int) engine.scope.size() / span)
                            % (int) engine.scope.size();
            const float v = juce::jlimit (0.0f, 1.0f, engine.scope[(size_t) idx] * 1.6f);
            const int h = juce::jmax (1, (int) (v * (float) halfSpan));
            for (int dy = 0; dy < h; ++dy)
            {
                const float fade = 1.0f - 0.5f * (float) dy / (float) juce::jmax (1, halfSpan);
                cell ((float) x, (float) (waveMid - dy), col.withMultipliedBrightness (fade));
                cell ((float) x, (float) (waveMid + dy), col.withMultipliedBrightness (fade * 0.85f));
            }
            // drip trail: occasional descender hanging off louder columns
            if (v > 0.25f && noise (x * 31 + 7) > 0.82f)
            {
                const int trail = (int) (v * 10.0f * noise (x * 13 + 3)) + 2;
                for (int t = 0; t < trail; ++t)
                    cell ((float) x, (float) (waveMid + h + t),
                          dim.withMultipliedAlpha (1.0f - (float) t / (float) trail));
            }
        }

        // sparkles + falling particles
        for (const auto& d : particles)
            if (d.alive)
            {
                if (d.kind == 1)
                {
                    cell (d.x, d.y, col.withMultipliedAlpha (juce::jlimit (0.2f, 1.0f, d.life)));
                    cell (d.x - 1, d.y, faint); cell (d.x + 1, d.y, faint);
                    cell (d.x, d.y - 1, faint); cell (d.x, d.y + 1, faint);
                }
                else
                    cell (d.x, d.y, dim.withMultipliedAlpha (juce::jlimit (0.2f, 1.0f, d.life)));
            }
    }

    //==========================================================================
    void paintIdle (juce::Graphics& g, float px, int cols, int rows,
                    juce::Colour col, juce::Colour dim, juce::Colour faint, int groundY)
    {
        auto cell = [&] (float cx, float cy, juce::Colour c)
        {
            const float s = px * 0.88f;
            g.setColour (c);
            g.fillRect (cx * px + (px - s) * 0.5f, cy * px + (px - s) * 0.5f, s, s);
        };

        // theme playground: idle particles already advanced in advanceParticles()
        for (const auto& d : particles)
            if (d.alive)
            {
                cell (d.x, d.y, (d.kind == 1 ? col : dim)
                                    .withMultipliedAlpha (juce::jlimit (0.2f, 1.0f, d.life)));
                if (state.theme == 4 || state.theme == 5)   // capsule shape
                    cell (d.x + 1, d.y, faint.withMultipliedAlpha (juce::jlimit (0.2f, 1.0f, d.life)));
                if (state.theme == 1)                        // puff blob
                {
                    cell (d.x + 1, d.y, faint); cell (d.x - 1, d.y, faint);
                    cell (d.x, d.y - 1, faint);
                }
            }

        // wavy syrup pool for lean / snow drift line
        if (state.theme == 0 || state.theme == 3 || state.theme == 5)
            for (int x = 3; x < cols - 3; ++x)
            {
                const float wob = std::sin (animPhase * 2.0f + (float) x * 0.22f) * 1.5f;
                const int y = groundY - 4 + (int) wob;
                cell ((float) x, (float) y, dim);
                cell ((float) x, (float) (y + 1), faint);
            }

        // blinking prompt
        if (((int) (animPhase * 1.4f) & 1) == 0)
            drawPixelText (g, px, (cols - 4 * 12) / 2, rows / 2 - 3, "feed me a vocal",
                           col, 1);
        juce::ignoreUnused (rows);
    }

    //==========================================================================
    void paintBoot (juce::Graphics& g, float px, int cols, int rows, juce::uint32 now)
    {
        const auto col = phosphor (state.theme);
        const float t = 1.0f - (float) (bootEnd - now) / 1500.0f;   // 0..1

        // logo drops in
        const int targetY = rows / 2 - 8;
        const int y = (int) juce::jmap (juce::jmin (1.0f, t * 2.2f), (float) -8, (float) targetY);
        drawPixelText (g, px, (cols - 8 * 9) / 2, y, "vocalgeek", col, 2);

        if (t > 0.45f)
            drawPixelText (g, px, (cols - 4 * 12) / 2, targetY + 14, "dose console", 
                           col.withMultipliedBrightness (0.6f), 1);

        // progress cells
        const int done = (int) (juce::jlimit (0.0f, 1.0f, (t - 0.2f) / 0.7f) * 20.0f);
        for (int i = 0; i < 20; ++i)
        {
            g.setColour (i < done ? col : col.withMultipliedBrightness (0.25f));
            const float bx = ((float) cols / 2.0f - 20.0f + (float) i * 2.0f) * px;
            g.fillRect (bx, (float) (targetY + 24) * px, px * 1.7f, px * 1.7f);
        }
        paintGlassOverlay (g, px, rows);
    }

    void paintSwap (juce::Graphics& g, float px, int cols, int rows)
    {
        const auto col = phosphor (state.theme);
        // one frame of static
        for (int i = 0; i < 320; ++i)
        {
            const float x = rng.nextFloat() * (float) cols;
            const float y = rng.nextFloat() * (float) rows;
            g.setColour (col.withMultipliedAlpha (rng.nextFloat() * 0.5f));
            g.fillRect (x * px, y * px, px * 0.9f, px * 0.9f);
        }
        drawPixelText (g, px, (cols - 4 * 15) / 2, rows / 2 - 3, "loading dose...", col, 1);
        paintGlassOverlay (g, px, rows);
    }

    // faint scanlines + corner vignette so the grid reads as a lit LCD
    void paintGlassOverlay (juce::Graphics& g, float px, int rows)
    {
        g.setColour (juce::Colours::black.withAlpha (0.13f));
        for (int y = 0; y < rows; y += 2)
            g.fillRect (0.0f, (float) y * px + px * 0.5f, (float) getWidth(), px * 0.5f);

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
                                        px * 0.88f * (float) scale, px * 0.88f * (float) scale);
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
    struct Particle { float x = 0, y = 0, vx = 0, vy = 0, life = 0; int kind = 0; bool alive = false; };

    void advanceParticles()
    {
        const float px = juce::jmax (1.5f, (float) getWidth() / (float) gridCols);
        const int cols = gridCols;
        const int rows = (int) ((float) getHeight() / px);
        const int groundY = rows - 26;

        const bool idle = quietFrames > 5 * 60;
        const float level = juce::jlimit (0.0f, 1.0f, state.outLevel * 1.4f);

        auto spawn = [&] (float x, float y, float vx, float vy, int kind)
        {
            for (auto& d : particles)
                if (! d.alive)
                {
                    d = { x, y, vx, vy, 1.0f, kind, true };
                    return;
                }
        };

        if (idle)
        {
            // per-theme playground
            switch (state.theme)
            {
                case 0:   // lean: syrup drips from the top edge
                    if (rng.nextFloat() < 0.25f)
                        spawn (5.0f + rng.nextFloat() * (float) (cols - 10), 12.0f,
                               0.0f, 0.10f + rng.nextFloat() * 0.15f, 0);
                    break;
                case 1:   // smoke: puffs drifting up with sway
                    if (rng.nextFloat() < 0.30f)
                        spawn (20.0f + rng.nextFloat() * (float) (cols - 40), (float) groundY - 4.0f,
                               0.0f, -(0.15f + rng.nextFloat() * 0.2f), 1);
                    break;
                case 2:   // acid: orbiting spiral
                    if (rng.nextFloat() < 0.5f)
                    {
                        const float a = animPhase * 2.0f + rng.nextFloat() * 0.4f;
                        const float r = 4.0f + std::fmod (animPhase * 9.0f + rng.nextFloat() * 20.0f, 26.0f);
                        spawn ((float) cols / 2.0f + std::cos (a) * r * 1.4f,
                               (float) rows / 2.0f + std::sin (a) * r * 0.8f,
                               0.0f, 0.0f, 1);
                    }
                    break;
                case 3:   // snow: snowfall with sway
                    if (rng.nextFloat() < 0.5f)
                        spawn (rng.nextFloat() * (float) cols, 10.0f,
                               0.0f, 0.2f + rng.nextFloat() * 0.25f, 1);
                    break;
                default:  // geeked / overdose: pills popping up like popcorn
                    if (rng.nextFloat() < 0.22f)
                        spawn (15.0f + rng.nextFloat() * (float) (cols - 30), (float) groundY - 2.0f,
                               (rng.nextFloat() - 0.5f) * 0.8f, -(0.8f + rng.nextFloat() * 0.7f), 0);
                    break;
            }
        }
        else if (level > 0.05f)
        {
            if (rng.nextFloat() < level * 0.9f)   // falling debris under the wave
                spawn (5.0f + rng.nextFloat() * (float) (cols - 10),
                       (float) rows * 0.5f + rng.nextFloat() * 4.0f,
                       0.0f, state.theme == 0 ? 0.12f + rng.nextFloat() * 0.1f
                                              : 0.3f + rng.nextFloat() * 0.4f, 0);
            if (rng.nextFloat() < level * 0.35f)  // diamond sparkles around the wave
                spawn (6.0f + rng.nextFloat() * (float) (cols - 12),
                       (float) rows * 0.28f + rng.nextFloat() * (float) rows * 0.4f,
                       0.0f, 0.03f, 1);
        }

        for (auto& d : particles)
            if (d.alive)
            {
                d.x += d.vx;
                d.y += d.vy;
                if (idle)
                {
                    if (state.theme == 1) d.x += std::sin (animPhase * 3.0f + d.y * 0.3f) * 0.25f;
                    if (state.theme == 3) d.x += std::sin (animPhase * 2.0f + d.y * 0.2f) * 0.35f;
                    if (state.theme >= 4) d.vy += 0.045f;   // gravity for popcorn pills
                }
                d.life -= (d.kind == 1 && ! idle) ? 0.05f : (state.theme == 0 ? 0.008f : 0.02f);
                if (d.life <= 0.0f || d.y > (float) rows - 20.0f || d.y < 6.0f
                    || d.x < 2.0f || d.x > (float) cols - 2.0f)
                    d.alive = false;
            }
    }

    GeekEngine& engine;
    State state;
    std::array<Particle, 48> particles;
    juce::Random rng;
    float animPhase = 0.0f;
    int quietFrames = 0;

    juce::uint32 bootEnd = 0, swapEnd = 0, hudEnd = 0, flashEnd = 0;
    juce::String hudName;
    float hudValue = 0.0f;
    juce::String flashText;
    juce::Rectangle<int> autoZone;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GeekDisplay)
};
