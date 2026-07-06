#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Photoreal UI skin loader (shared by all plugins).
//
// Assets live in <repo>/assets/ui and are embedded per-plugin via a
// juce_add_binary_data target using NAMESPACE SkinBinary / HEADER_NAME
// SkinBinary.h. When that target is present the build defines VG_HAS_SKIN=1
// and every asset is looked up by its ORIGINAL filename (e.g.
// "2a-vu-face@2x.png"). Anything missing returns an invalid image, so callers
// must fall back to vector drawing — the skin is always optional.

#if VG_HAS_SKIN
 #include "SkinBinary.h"
#endif

namespace skin
{
    inline bool available() noexcept
    {
       #if VG_HAS_SKIN
        return true;
       #else
        return false;
       #endif
    }

    // Load a bundled UI image by its original filename. Cached after first load.
    inline juce::Image image (const juce::String& originalFilename)
    {
       #if VG_HAS_SKIN
        static juce::HashMap<juce::String, juce::Image> cache;
        if (cache.contains (originalFilename))
            return cache[originalFilename];

        juce::Image result;
        for (int i = 0; i < SkinBinary::namedResourceListSize; ++i)
        {
            if (originalFilename == SkinBinary::originalFilenames[i])
            {
                int sz = 0;
                if (const char* data = SkinBinary::getNamedResource (SkinBinary::namedResourceList[i], sz))
                    result = juce::ImageFileFormat::loadFrom (data, (size_t) sz);
                break;
            }
        }
        cache.set (originalFilename, result);
        return result;
       #else
        juce::ignoreUnused (originalFilename);
        return {};
       #endif
    }

    inline bool has (const juce::String& originalFilename)
    {
        return image (originalFilename).isValid();
    }

    //==========================================================================
    // Drawing helpers (geometry / motion stay in code; images carry material).

    // The rectangle an image occupies when scaled to *contain* it inside dest
    // (preserve aspect, centred, may leave margins).
    inline juce::Rectangle<float> containRect (const juce::Image& img, juce::Rectangle<float> dest)
    {
        if (! img.isValid() || img.getWidth() <= 0 || img.getHeight() <= 0)
            return dest;
        const float ar   = (float) img.getWidth() / (float) img.getHeight();
        const float dar  = dest.getWidth() / dest.getHeight();
        float w = dest.getWidth(), h = dest.getHeight();
        if (ar > dar) h = w / ar; else w = h * ar;
        return { dest.getCentreX() - w * 0.5f, dest.getCentreY() - h * 0.5f, w, h };
    }

    // The rectangle an image occupies when scaled so its WIDTH matches dest,
    // centred vertically (used when we want the piece to fill the width and let
    // its transparent top/bottom margins extend past dest — clipped by the host).
    inline juce::Rectangle<float> widthRect (const juce::Image& img, juce::Rectangle<float> dest)
    {
        if (! img.isValid() || img.getWidth() <= 0)
            return dest;
        const float ar = (float) img.getWidth() / (float) img.getHeight();
        const float w  = dest.getWidth();
        const float h  = w / ar;
        return { dest.getX(), dest.getCentreY() - h * 0.5f, w, h };
    }

    // Bounding box (in source pixels) of everything above `alphaThresh` opacity.
    // Cached per filename — used to trim the transparent margins off sprite art
    // (pills, toggles) so the visible piece can be stretched to fill a slot.
    inline juce::Rectangle<int> opaqueBounds (const juce::String& originalFilename,
                                              float alphaThresh = 0.06f)
    {
        static juce::HashMap<juce::String, juce::Rectangle<int>> cache;
        if (cache.contains (originalFilename))
            return cache[originalFilename];

        juce::Rectangle<int> bounds;
        auto img = image (originalFilename);
        if (img.isValid())
        {
            const juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
            const int w = img.getWidth(), h = img.getHeight();
            const juce::uint8 aT = (juce::uint8) juce::jlimit (0, 255, (int) (alphaThresh * 255.0f));
            int minX = w, minY = h, maxX = -1, maxY = -1;
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    if (bd.getPixelColour (x, y).getAlpha() > aT)
                    {
                        minX = juce::jmin (minX, x); minY = juce::jmin (minY, y);
                        maxX = juce::jmax (maxX, x); maxY = juce::jmax (maxY, y);
                    }
            bounds = (maxX >= minX && maxY >= minY)
                         ? juce::Rectangle<int> (minX, minY, maxX - minX + 1, maxY - minY + 1)
                         : juce::Rectangle<int> (0, 0, w, h);
        }
        cache.set (originalFilename, bounds);
        return bounds;
    }

    // Draw the trimmed (opaque) region of a sprite stretched to fill `rect`.
    inline void drawTrimmedInRect (juce::Graphics& g, const juce::String& originalFilename,
                                   juce::Rectangle<float> rect, float alpha = 1.0f)
    {
        auto img = image (originalFilename);
        if (! img.isValid()) return;
        const auto src = opaqueBounds (originalFilename);
        g.saveState();
        g.setOpacity (alpha);
        g.drawImage (img, rect.getX(), rect.getY(), rect.getWidth(), rect.getHeight(),
                     src.getX(), src.getY(), src.getWidth(), src.getHeight(), false);
        g.restoreState();
    }

    inline void drawInRect (juce::Graphics& g, const juce::Image& img,
                            juce::Rectangle<float> rect, float alpha = 1.0f)
    {
        if (! img.isValid()) return;
        g.saveState();
        g.setOpacity (alpha);
        g.drawImage (img, rect,
                     juce::RectanglePlacement::stretchToFit, false);
        g.restoreState();
    }

    // 9-slice: draw `img` into `dest` keeping the corner regions (given by
    // `inset` px in the source image) unscaled and stretching the edges/centre.
    inline void drawNineSlice (juce::Graphics& g, const juce::Image& img,
                               juce::Rectangle<float> dest, int inset, float alpha = 1.0f)
    {
        if (! img.isValid()) { return; }
        const int iw = img.getWidth(), ih = img.getHeight();
        const int L = juce::jmin (inset, iw / 2 - 1), T = juce::jmin (inset, ih / 2 - 1);
        g.saveState();
        g.setOpacity (alpha);

        const float dx = dest.getX(), dy = dest.getY(), dw = dest.getWidth(), dh = dest.getHeight();
        // Shrink the drawn corner size if the target is smaller than 2x the
        // source inset, otherwise the corners overlap and the centre collapses.
        const float rL = juce::jmin ((float) L, dw * 0.5f);
        const float rT = juce::jmin ((float) T, dh * 0.5f);

        auto blit = [&] (int sx, int sy, int sw, int sh,
                         float ddx, float ddy, float ddw, float ddh)
        {
            if (sw <= 0 || sh <= 0 || ddw <= 0 || ddh <= 0) return;
            g.drawImage (img, ddx, ddy, ddw, ddh, sx, sy, sw, sh, false);
        };

        const int cX = iw - 2 * L, cY = ih - 2 * T;     // source centre span
        const float dCX = dw - 2 * rL, dCY = dh - 2 * rT; // dest centre span

        // corners
        blit (0, 0, L, T,               dx, dy, rL, rT);
        blit (iw - L, 0, L, T,          dx + dw - rL, dy, rL, rT);
        blit (0, ih - T, L, T,          dx, dy + dh - rT, rL, rT);
        blit (iw - L, ih - T, L, T,     dx + dw - rL, dy + dh - rT, rL, rT);
        // edges
        blit (L, 0, cX, T,              dx + rL, dy, dCX, rT);
        blit (L, ih - T, cX, T,         dx + rL, dy + dh - rT, dCX, rT);
        blit (0, T, L, cY,              dx, dy + rT, rL, dCY);
        blit (iw - L, T, L, cY,         dx + dw - rL, dy + rT, rL, dCY);
        // centre
        blit (L, T, cX, cY,             dx + rL, dy + rT, dCX, dCY);

        g.restoreState();
    }

    // Crop a knob sprite to a square hugging its dome. domeCx is a fraction of
    // the image width, domeCy a fraction of the image height, domeDiaFrac a
    // fraction of the image width (canvases aren't always square). The result
    // is centred on the dome, so callers can rotate about (0.5, 0.5) and the
    // visible dome fills ~94% of the cropped canvas.
    inline juce::Image cropToDome (const juce::Image& img,
                                   float domeCx, float domeCy, float domeDiaFrac,
                                   float pad = 1.06f)
    {
        if (! img.isValid()) return img;
        const float halfPx = domeDiaFrac * 0.5f * pad * (float) img.getWidth();
        const int   side   = juce::roundToInt (halfPx * 2.0f);
        const int   x      = juce::roundToInt (domeCx * (float) img.getWidth()  - halfPx);
        const int   y      = juce::roundToInt (domeCy * (float) img.getHeight() - halfPx);
        return img.getClippedImage (juce::Rectangle<int> (x, y, side, side)
                                        .getIntersection (img.getBounds()));
    }

    // Fraction of a cropToDome() result's width occupied by the visible dome.
    constexpr float croppedDomeFrac (float pad = 1.06f) { return 1.0f / pad; }

    // Draw a knob image rotated about its dome centre by `angleRad` (clockwise).
    // pivotFx/pivotFy give the dome centre as a fraction of the image (AI art is
    // rarely perfectly centred; rotating about the true dome centre stops the
    // dome from orbiting as it turns).
    inline void drawKnobRotated (juce::Graphics& g, const juce::Image& img,
                                 juce::Rectangle<float> dest, float angleRad,
                                 float pivotFx = 0.5f, float pivotFy = 0.5f)
    {
        if (! img.isValid()) return;
        const float s = juce::jmin (dest.getWidth(), dest.getHeight()) / (float) img.getWidth();
        const juce::Point<float> c { (float) img.getWidth() * pivotFx,
                                     (float) img.getHeight() * pivotFy };
        auto t = juce::AffineTransform::translation (-c.x, -c.y)
                     .scaled (s)
                     .rotated (angleRad)
                     .translated (dest.getCentreX(), dest.getCentreY());
        g.drawImageTransformed (img, t);
    }

    // Draw a needle image so its pivot (given as a fraction of the image) lands
    // on `pivot`, scaled so the pivot->tip span equals `lengthToTip`, then
    // rotated by `angleRad` (clockwise, 0 = straight up).
    inline void drawNeedle (juce::Graphics& g, const juce::Image& img,
                            juce::Point<float> pivot, float pivotFracX, float pivotFracY,
                            float tipFracY, float lengthToTip, float angleRad,
                            float alpha = 1.0f)
    {
        if (! img.isValid()) return;
        const float spanImg = (pivotFracY - tipFracY) * (float) img.getHeight(); // px in image
        if (spanImg <= 0.0f) return;
        const float scale = lengthToTip / spanImg;
        const juce::Point<float> pivotPx { pivotFracX * (float) img.getWidth(),
                                           pivotFracY * (float) img.getHeight() };
        g.saveState();
        g.setOpacity (alpha);
        auto t = juce::AffineTransform::translation (-pivotPx.x, -pivotPx.y)
                     .scaled (scale)
                     .rotated (angleRad)
                     .translated (pivot.x, pivot.y);
        g.drawImageTransformed (img, t);
        g.restoreState();
    }
}
