#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Shared visual language for the "vocal" plugin family — a premium light look
// built from technique (layered shadows, material gradients, fine engraving)
// rather than bitmap assets. Hot-pink accent on a warm, dimensional off-white.
//==============================================================================
namespace theme
{
    // ---- palette ------------------------------------------------------------
    const juce::Colour bg        { 0xfff1f2f5 }; // app background base
    const juce::Colour bgHi      { 0xfffbfbfe }; // background highlight (top light)
    const juce::Colour bgLo      { 0xffe6e7ec }; // background shade (bottom)
    const juce::Colour card      { 0xfffdfdff }; // panel fill (near white)
    const juce::Colour cardLine  { 0xffe2e3ea }; // panel hairline border
    const juce::Colour track      { 0xffe7e8ee }; // slider track / ring base
    const juce::Colour trackDeep { 0xffd9dae2 }; // recessed groove floor

    const juce::Colour accent    { 0xffec0f8f }; // hot pink
    const juce::Colour accentHi  { 0xfff64bad }; // accent highlight (top of gradient)
    const juce::Colour accentLo  { 0xffcc0a7c }; // accent shade (bottom of gradient)
    const juce::Colour accentSoft{ 0x33ec0f8f }; // pink glow

    const juce::Colour ink       { 0xff17171c }; // primary text
    const juce::Colour inkSoft   { 0xff8b8b96 }; // secondary text
    const juce::Colour inkFaint  { 0xffb6b7c1 }; // tertiary / ticks
    const juce::Colour thumb     { 0xffffffff }; // white knob/thumb

    // light cap material (kept for slider thumbs etc.)
    const juce::Colour capHi     { 0xffffffff };
    const juce::Colour capMid    { 0xfff3f4f8 };
    const juce::Colour capLo     { 0xffdfe0e7 };

    // dark glossy knob cap (Humanoid-style: charcoal, top-lit, with a glowing ring)
    const juce::Colour knobHi    { 0xff4a4b55 }; // top catch
    const juce::Colour knobMid   { 0xff2b2c33 }; // body
    const juce::Colour knobLo    { 0xff121216 }; // bottom / edge
    const juce::Colour knobRim   { 0xff0a0a0d }; // outer rim line
    const juce::Colour ringTrack { 0xffd4d5dd }; // unlit ring groove on light panels

    // ---- type ---------------------------------------------------------------
    const juce::String fontFamily { "SF Pro Display" };

    inline juce::Font font (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (fontFamily, size,
                                              bold ? juce::Font::bold : juce::Font::plain));
    }

    // Letter-spaced text (premium small-caps style labels). Draws each glyph
    // with manual tracking since JUCE fonts have no native letter-spacing.
    inline void spacedText (juce::Graphics& g, const juce::String& text,
                            juce::Rectangle<float> area, juce::Colour colour,
                            float size, float tracking = 2.0f, bool bold = true,
                            juce::Justification just = juce::Justification::centredLeft)
    {
        const auto f = font (size, bold);
        g.setFont (f);
        g.setColour (colour);

        const juce::String s = text.toUpperCase();
        float total = 0.0f;
        for (int i = 0; i < s.length(); ++i)
            total += juce::GlyphArrangement::getStringWidth (f, s.substring (i, i + 1)) + tracking;
        total -= tracking;

        float x = area.getX();
        if (just.testFlags (juce::Justification::horizontallyCentred))
            x = area.getCentreX() - total * 0.5f;
        else if (just.testFlags (juce::Justification::right))
            x = area.getRight() - total;

        for (int i = 0; i < s.length(); ++i)
        {
            const auto ch = s.substring (i, i + 1);
            const float w = juce::GlyphArrangement::getStringWidth (f, ch);
            g.drawText (ch, juce::Rectangle<float> (x, area.getY(), w + 2.0f, area.getHeight()),
                        juce::Justification::centredLeft);
            x += w + tracking;
        }
    }

    // ---- depth helpers ------------------------------------------------------

    // Soft pink glow behind a path. alpha 0..1 controls intensity (for pulsing).
    inline void glowPath (juce::Graphics& g, const juce::Path& p, float alpha, int radius = 26)
    {
        juce::DropShadow (accent.withAlpha (juce::jlimit (0.0f, 1.0f, alpha)),
                          radius, { 0, 0 }).drawForPath (g, p);
    }

    // Multi-layer ambient + contact shadow so a card reads as floating above the
    // canvas. strength scales the whole stack.
    inline void elevate (juce::Graphics& g, juce::Rectangle<float> r, float corner,
                         float strength = 1.0f)
    {
        juce::Path p;
        p.addRoundedRectangle (r, corner);
        juce::DropShadow (juce::Colours::black.withAlpha (0.10f * strength), 18, { 0, 8 }).drawForPath (g, p);
        juce::DropShadow (juce::Colours::black.withAlpha (0.07f * strength), 8,  { 0, 3 }).drawForPath (g, p);
        juce::DropShadow (juce::Colours::black.withAlpha (0.05f * strength), 2,  { 0, 1 }).drawForPath (g, p);
    }

    // Legacy alias kept so existing editors keep compiling.
    inline void cardShadow (juce::Graphics& g, juce::Rectangle<int> r, float corner)
    {
        elevate (g, r.toFloat(), corner);
    }

    // 1px inner highlight along the top edge of a panel (catches the light).
    inline void topHighlight (juce::Graphics& g, juce::Rectangle<float> r, float corner)
    {
        juce::Path p;
        p.addRoundedRectangle (r.getX() + 0.75f, r.getY() + 0.75f,
                               r.getWidth() - 1.5f, r.getHeight() - 1.5f, corner);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.saveState();
        g.reduceClipRegion ((int) r.getX(), (int) r.getY(),
                            (int) r.getWidth(), (int) (corner + 6.0f));
        g.strokePath (p, juce::PathStrokeType (1.2f));
        g.restoreState();
    }

    // Carve a recessed groove (inner shadow at top, light catch at bottom).
    inline void recess (juce::Graphics& g, juce::Rectangle<float> r, float corner)
    {
        g.setColour (trackDeep);
        g.fillRoundedRectangle (r, corner);
        juce::ColourGradient ig (juce::Colours::black.withAlpha (0.18f), r.getX(), r.getY(),
                                 juce::Colours::transparentBlack, r.getX(),
                                 r.getY() + juce::jmin (r.getHeight() * 0.7f, 10.0f), false);
        g.setGradientFill (ig);
        g.fillRoundedRectangle (r, corner);
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.drawLine (r.getX() + corner, r.getBottom() - 0.5f,
                    r.getRight() - corner, r.getBottom() - 0.5f, 1.0f);
    }

    // A thin dotted divider (Baby-Audio style) for separating groups.
    inline void dottedDivider (juce::Graphics& g, float x1, float x2, float y,
                               juce::Colour colour, float dot = 1.4f, float gap = 4.0f)
    {
        g.setColour (colour);
        for (float x = x1; x <= x2; x += dot + gap)
            g.fillEllipse (x, y - dot * 0.5f, dot, dot);
    }

    // Soft radial accent bloom used for mood lighting behind a hero element.
    inline void accentBloom (juce::Graphics& g, juce::Point<float> c, float radius, float alpha)
    {
        juce::ColourGradient ag (accent.withAlpha (alpha), c.x, c.y,
                                 juce::Colours::transparentBlack, c.x, c.y + radius, true);
        g.setGradientFill (ag);
        g.fillEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
    }

    // Full-window backdrop: soft top-down gradient + gentle top-centre light.
    inline void backdrop (juce::Graphics& g, juce::Rectangle<int> b)
    {
        juce::ColourGradient vg (bgHi, 0.0f, (float) b.getY(),
                                 bgLo, 0.0f, (float) b.getBottom(), false);
        vg.addColour (0.45, bg);
        g.setGradientFill (vg);
        g.fillRect (b);

        const float cx = (float) b.getCentreX();
        const float cy = (float) b.getY() - b.getHeight() * 0.1f;
        const float rad = (float) b.getWidth() * 0.75f;
        juce::ColourGradient light (juce::Colours::white.withAlpha (0.5f), cx, cy,
                                    juce::Colours::transparentWhite, cx, cy + rad, true);
        g.setGradientFill (light);
        g.fillRect (b);
    }
}
