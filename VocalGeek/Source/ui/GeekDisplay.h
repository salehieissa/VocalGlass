#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/GeekEngine.h"
#include <array>

//==============================================================================
// The pixel screen. Everything is drawn on a chunky fixed pixel grid in the
// theme's phosphor colour: a header line ("lean · 2oz"), a mirrored waveform
// fed by the engine's scope, falling particles (drips / sparks) driven by the
// signal, two block meters and a "tolerance" (dose) read-out. Text uses a
// tiny built-in 3x5 pixel font so it reads like real handheld firmware.
//==============================================================================
class GeekDisplay : public juce::Component
{
public:
    explicit GeekDisplay (GeekEngine& e) : engine (e)
    {
        setInterceptsMouseClicks (false, false);
        rng.setSeedRandomly();
    }

    struct State
    {
        int   theme = 0;
        float dose = 0.5f, outLevel = 0.0f;
        bool  stutter = false, tape = false;
        juce::String rateText { "1/8" };
    };

    void refresh (const State& s)
    {
        state = s;
        advanceParticles();
        repaint();
    }

    static juce::Colour phosphor (int theme)
    {
        switch (theme)
        {
            case 1:  return juce::Colour (0xff3ee06a);   // smoke  — green
            case 2:  return juce::Colour (0xffff8a1e);   // acid   — orange
            case 3:  return juce::Colour (0xff35c7f0);   // snow   — ice blue
            case 4:  return juce::Colour (0xffffd21e);   // geeked — yellow
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
            default: return "2oz";
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff050507));

        const auto col   = phosphor (state.theme);
        const auto dim   = col.withMultipliedBrightness (0.55f);
        const auto faint = col.withMultipliedBrightness (0.3f);

        // pixel grid: ~86 columns wide
        const float px = juce::jmax (2.0f, (float) getWidth() / 86.0f);
        auto cell = [&] (float cx, float cy, juce::Colour c, float scale = 1.0f)
        {
            const float s = px * 0.86f * scale;
            g.setColour (c);
            g.fillRect (cx * px + (px - s) * 0.5f, cy * px + (px - s) * 0.5f, s, s);
        };

        const int cols = (int) ((float) getWidth() / px);
        const int rows = (int) ((float) getHeight() / px);

        // ---- header line
        drawPixelText (g, px, 2, 2, juce::String (themeName (state.theme)) + " . "
                                        + themeDose (state.theme), col);
        if (state.stutter)
            drawPixelText (g, px, cols - 3 - 4 * (5 + state.rateText.length()), 2,
                           "stut " + state.rateText, col);
        else if (state.tape)
            drawPixelText (g, px, cols - 3 - 4 * 6, 2, "brakes", col);

        // ---- mirrored waveform
        const int waveTop = 9, waveBot = rows - 16;
        const int waveMid = (waveTop + waveBot) / 2;
        const int halfSpan = (waveBot - waveTop) / 2;

        const int w = engine.scopeWrite.load();
        for (int x = 2; x < cols - 2; ++x)
        {
            const int idx = (w + (int) engine.scope.size()
                               - (cols - 2 - x)) % (int) engine.scope.size();
            const float v = juce::jlimit (0.0f, 1.0f, engine.scope[(size_t) idx] * 1.6f);
            const int h = juce::jmax (1, (int) (v * (float) halfSpan));
            for (int dy = 0; dy < h; ++dy)
            {
                const float fade = 1.0f - 0.55f * (float) dy / (float) juce::jmax (1, halfSpan);
                cell ((float) x, (float) (waveMid - dy), col.withMultipliedBrightness (fade));
                cell ((float) x, (float) (waveMid + dy), col.withMultipliedBrightness (fade * 0.8f));
            }
        }

        // ---- falling particles (drips / sparks) under the waveform
        for (const auto& d : particles)
            if (d.alive)
                cell (d.x, d.y, dim.withAlpha (juce::jlimit (0.2f, 1.0f, d.life)), 0.8f);

        // ---- bottom rows: L/R block meters + tolerance read-out
        const int metY1 = rows - 12, metY2 = rows - 8;
        const int metLen = 12;
        const float lvl = juce::jlimit (0.0f, 1.0f, state.outLevel * 1.4f);
        drawPixelText (g, px, 2, metY1 - 1, "l", col);
        drawPixelText (g, px, 2, metY2 - 1, "r", col);
        for (int i = 0; i < metLen; ++i)
        {
            const bool on1 = (float) i / metLen < lvl;
            const bool on2 = (float) i / metLen < lvl * 0.92f;
            for (int b = 0; b < 2; ++b)
            {
                cell ((float) (6 + i * 2),     (float) (metY1 + b), on1 ? col : faint);
                cell ((float) (6 + i * 2) + 1, (float) (metY1 + b), on1 ? col : faint);
                cell ((float) (6 + i * 2),     (float) (metY2 + b), on2 ? dim : faint);
                cell ((float) (6 + i * 2) + 1, (float) (metY2 + b), on2 ? dim : faint);
            }
        }

        drawPixelText (g, px, cols - 4 * 13, metY2 - 1,
                       "tolerance " + juce::String ((int) (state.dose * 100.0f)), col);
    }

private:
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
                        const juce::String& text, juce::Colour colour)
    {
        g.setColour (colour);
        int x = col;
        for (const auto c : text)
        {
            if (c == ' ') { x += 2; continue; }
            if (const auto* rows = glyph (juce::CharacterFunctions::toLowerCase (c)))
            {
                for (int ry = 0; ry < 5; ++ry)
                    for (int rx = 0; rx < 3; ++rx)
                        if (rows[ry] & (0b100 >> rx))
                            g.fillRect (((float) x + rx) * px, ((float) row + ry) * px,
                                        px * 0.86f, px * 0.86f);
            }
            x += 4;
        }
    }

    //==========================================================================
    struct Particle { float x = 0, y = 0, vy = 0, life = 0; bool alive = false; };

    void advanceParticles()
    {
        const float px = juce::jmax (2.0f, (float) getWidth() / 86.0f);
        const int cols = (int) ((float) getWidth() / px);
        const int rows = (int) ((float) getHeight() / px);

        const float level = juce::jlimit (0.0f, 1.0f, state.outLevel * 1.4f);

        // spawn: heavier signal sheds more particles
        if (level > 0.05f && rng.nextFloat() < level * 0.9f)
            for (auto& d : particles)
                if (! d.alive)
                {
                    d.alive = true;
                    d.x = 3.0f + rng.nextFloat() * (float) (cols - 6);
                    d.y = (float) rows * 0.5f + rng.nextFloat() * 3.0f;
                    d.vy = state.theme == 0 ? 0.12f + rng.nextFloat() * 0.1f    // lean drips crawl
                                            : 0.3f + rng.nextFloat() * 0.4f;    // others fall
                    d.life = 1.0f;
                    break;
                }

        for (auto& d : particles)
            if (d.alive)
            {
                d.y += d.vy;
                d.life -= state.theme == 0 ? 0.008f : 0.02f;
                if (d.life <= 0.0f || d.y > (float) rows - 9.0f)
                    d.alive = false;
            }
    }

    GeekEngine& engine;
    State state;
    std::array<Particle, 24> particles;
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GeekDisplay)
};
