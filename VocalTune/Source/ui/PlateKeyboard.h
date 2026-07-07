#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Piano keyboard for the baked-plate UI. The unlit keyboard is part of the OFF
// chassis plate, so this component draws nothing at rest; active notes are
// revealed by blitting the matching region of the pre-registered ON plate,
// clipped to that key's shape (an L for whites, a rect for blacks). Every
// octave copy of a pitch class lights together and clicking any copy toggles
// the note.
//
// Geometry is measured on the 2048x1360 plates: the component's bounds map to
// the white-key extent (kX0..kX1, kY0..kY1 in image pixels), 15 white keys
// C..C with least-squares-fitted separators, 10 black keys at their measured
// centres.
//==============================================================================
class PlateKeyboard : public juce::Component
{
public:
    std::function<bool (int)> isNoteOn;   // pitch class 0..11
    std::function<void (int)> onToggle;

    PlateKeyboard()
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    // The ON plate (full 2048x1360 canvas) — lit keys are cut from it.
    void setOnPlate (juce::Image img) { onImg = std::move (img); }

    bool ready() const { return onImg.isValid(); }

    // Repaint only when the note set actually changed (called from the editor
    // timer, which ticks at 30 Hz).
    void refresh()
    {
        int bits = 0;
        for (int i = 0; i < 12; ++i)
            if (isNoteOn && isNoteOn (i)) bits |= (1 << i);
        if (bits != noteBits) { noteBits = bits; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        if (! ready() || noteBits == 0) return;

        // whites first, blacks on top (a lit black must cover a lit white edge)
        for (int w = 0; w < kNumWhite; ++w)
        {
            if ((noteBits & (1 << whitePitch (w))) == 0) continue;

            g.saveState();
            g.reduceClipRegion (whiteLocal (w).toNearestInt());
            for (int b = 0; b < kNumBlack; ++b)
                g.excludeClipRegion (blackLocal (b, 0.0f).toNearestInt());
            blitOn (g, whiteLocal (w));
            g.restoreState();
        }

        for (int b = 0; b < kNumBlack; ++b)
        {
            if ((noteBits & (1 << kBlackPitch[b])) == 0) continue;

            g.saveState();
            g.reduceClipRegion (blackLocal (b, -1.0f).toNearestInt());
            blitOn (g, blackLocal (b, -1.0f));
            g.restoreState();
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int n = noteAt (e.position);
        if (n >= 0 && onToggle) onToggle (n);
    }

private:
    //==========================================================================
    // Measured image-pixel geometry (2048x1360 plates). The vertical extent
    // covers the keys' neon rims (750..1017) so lit keys light edge-to-edge,
    // and the black-key mask runs seam-to-seam (dark valleys at cx +/- 22,
    // bottom seam at y 912) so no unlit collar shows around a black key.
    static constexpr float kX0 = 131.3f, kX1 = 1267.0f;   // white-key extent
    static constexpr float kY0 = 750.0f, kY1 = 1017.0f;
    static constexpr float kBlackY1 = 912.0f;             // black keys' bottom seam
    static constexpr int kNumWhite = 15, kNumBlack = 10;
    static constexpr float kSepA = 205.63f, kSepB = 74.31f; // sep(i) = A + B*i
    static constexpr float kBlackCx[kNumBlack] = { 202.0f, 284.0f, 423.0f, 499.0f, 575.0f,
                                                   720.0f, 796.0f, 945.0f, 1021.0f, 1100.0f };
    static constexpr float kBlackHalfW = 22.0f;
    static constexpr int kBlackPitch[kNumBlack] = { 1, 3, 6, 8, 10, 1, 3, 6, 8, 10 };

    static int whitePitch (int w)
    {
        static constexpr int pc[7] = { 0, 2, 4, 5, 7, 9, 11 };
        return pc[w % 7];
    }

    // white key boundary i (0..15) in image pixels
    static float whiteEdge (int i)
    {
        if (i <= 0)          return kX0;
        if (i >= kNumWhite)  return kX1;
        return kSepA + kSepB * (float) (i - 1);
    }

    //==========================================================================
    // image px -> component-local coordinate mapping
    float lx (float imgX) const { return (imgX - kX0) / (kX1 - kX0) * (float) getWidth(); }
    float ly (float imgY) const { return (imgY - kY0) / (kY1 - kY0) * (float) getHeight(); }

    juce::Rectangle<float> whiteLocal (int w) const
    {
        return { lx (whiteEdge (w)), 0.0f,
                 lx (whiteEdge (w + 1)) - lx (whiteEdge (w)), (float) getHeight() };
    }

    juce::Rectangle<float> blackLocal (int b, float expandPx) const
    {
        juce::Rectangle<float> r (lx (kBlackCx[b] - kBlackHalfW), 0.0f,
                                  lx (kBlackCx[b] + kBlackHalfW) - lx (kBlackCx[b] - kBlackHalfW),
                                  ly (kBlackY1));
        return r.expanded (expandPx, 0.0f).withTrimmedBottom (-expandPx);
    }

    // Blit the ON plate region matching a component-local rect. The component
    // spans (kX0..kX1, kY0..kY1) of the plate canvas, so the source rect is the
    // local rect mapped back into image pixels.
    void blitOn (juce::Graphics& g, juce::Rectangle<float> local) const
    {
        const float sx = kX0 + local.getX()      / (float) getWidth()  * (kX1 - kX0);
        const float sw =       local.getWidth()  / (float) getWidth()  * (kX1 - kX0);
        const float sy = kY0 + local.getY()      / (float) getHeight() * (kY1 - kY0);
        const float sh =       local.getHeight() / (float) getHeight() * (kY1 - kY0);
        g.drawImage (onImg,
                     juce::roundToInt (local.getX()), juce::roundToInt (local.getY()),
                     juce::roundToInt (local.getWidth()), juce::roundToInt (local.getHeight()),
                     juce::roundToInt (sx), juce::roundToInt (sy),
                     juce::roundToInt (sw), juce::roundToInt (sh));
    }

    int noteAt (juce::Point<float> p) const
    {
        if (! getLocalBounds().toFloat().contains (p)) return -1;

        if (p.y < ly (kBlackY1))
            for (int b = 0; b < kNumBlack; ++b)
                if (blackLocal (b, 2.0f).contains (p)) return kBlackPitch[b];

        for (int w = 0; w < kNumWhite; ++w)
            if (p.x >= lx (whiteEdge (w)) && p.x <= lx (whiteEdge (w + 1)))
                return whitePitch (w);
        return -1;
    }

    juce::Image onImg;
    int noteBits = 0;
};
