#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "../PluginProcessor.h"
#include <functional>

//==============================================================================
// Interactive response display: log-frequency grid, the summed EQ curve,
// draggable band nodes, and two output meters down the right edge.
//==============================================================================
class EQDisplay : public juce::Component
{
public:
    explicit EQDisplay (VocalQProcessor& p) : proc (p) {}

    std::function<void (int)> onSelectBand;
    int selectedBand = 5;

    // Plate mode: the smoked-glass panel, grid, axis labels and empty meter
    // channels are baked into the chassis. The display draws only the live
    // layers (spectrum, curve, nodes, meter fills) using the axis mapping
    // measured from the baked plate (the AI gridlines aren't perfectly
    // log-uniform, so freq<->x interpolates between the measured octave
    // positions instead of assuming an ideal log scale).
    void setPlateMode (bool p) { plateMode = p; repaint(); }

    void updateMeters()
    {
        auto smooth = [] (float& s, float target)
        {
            const float n = juce::jlimit (-60.0f, 6.0f, target);
            s = n > s ? n : s + (n - s) * 0.2f;
        };
        smooth (mL, proc.outLDb.load());
        smooth (mR, proc.outRDb.load());
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        if (plateMode)
        {
            const float w = (float) getWidth(), h = (float) getHeight();
            plot = { plateGridFx[0] * w, 0.0245f * h,
                     (0.9307f - plateGridFx[0]) * w, (0.9870f - 0.0245f) * h };
            drawSpectrum (g);
            drawCurve (g);
            drawNodes (g);
            drawMeters (g);
            return;
        }

        plot = getLocalBounds().toFloat().reduced (2.0f);
        plot.removeFromTop (16.0f);     // freq labels
        plot.removeFromLeft (30.0f);    // gain labels
        plot.removeFromRight (58.0f);   // meters + scale
        plot.removeFromBottom (4.0f);

        drawGrid (g);
        drawSpectrum (g);
        drawCurve (g);
        drawNodes (g);
        drawMeters (g);
    }

    //========================================================================
    void mouseDown (const juce::MouseEvent& e) override
    {
        dragBand = nodeAt (e.position);
        dragging = false;
        if (dragBand >= 0)
        {
            downPos   = e.position;
            grabOffset = nodeCentre (dragBand) - e.position;   // keep node under cursor, no jump
            selectedBand = dragBand;
            if (onSelectBand) onSelectBand (dragBand);
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragBand < 0) return;
        if (! dragging && e.position.getDistanceFrom (downPos) < 3.0f) return; // a click should not move it
        dragging = true;

        const auto target = e.position + grabOffset;
        const double f   = xToFreq (juce::jlimit (plot.getX(), plot.getRight(), target.x));
        const double gdb = yToGain (juce::jlimit (plot.getY(), plot.getBottom(), target.y));
        setParam (dragBand, "freq", (float) f);
        const int type = (int) raw (dragBand, "type");
        if (type == (int) BandType::Bell || type == (int) BandType::LowShelf || type == (int) BandType::HighShelf)
            setParam (dragBand, "gain", (float) gdb);
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int b = nodeAt (e.position);
        if (b < 0) return;
        if (auto* p = proc.apvts.getParameter (VocalQProcessor::bandParamId (b, "q")))
        {
            const float cur = raw (b, "q");
            const float nv  = juce::jlimit (0.1f, 60.0f, cur * (w.deltaY > 0 ? 1.12f : 0.89f));
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (nv));
            repaint();
        }
    }

private:
    //========================================================================
    static constexpr double fLo = 16.0, fHi = 22000.0;

    float raw (int b, const char* s) const
    {
        return proc.apvts.getRawParameterValue (VocalQProcessor::bandParamId (b, s))->load();
    }

    void setParam (int b, const char* s, float v)
    {
        if (auto* p = proc.apvts.getParameter (VocalQProcessor::bandParamId (b, s)))
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (v));
    }

    // Baked-plate axis geometry (fractions of the component, measured from the
    // chassis): x positions of the 16Hz..16kHz octave gridlines, and the 0dB
    // row + px-per-dB of the gain axis.
    static constexpr std::array<double, 11> plateGridF  { 16, 32, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
    static constexpr std::array<float, 11>  plateGridFx { 0.0331f, 0.1285f, 0.2272f, 0.3168f, 0.4080f, 0.4976f,
                                                          0.5808f, 0.6629f, 0.7488f, 0.8315f, 0.9077f };
    static constexpr float plateY0Db = 0.5228f, plateFracPerDb = 0.015371f;

    float freqToX (double f) const
    {
        if (plateMode)
        {
            const float w = (float) getWidth();
            const double lf = std::log (juce::jmax (1.0, f));
            size_t i = 0;
            while (i + 2 < plateGridF.size() && f > plateGridF[i + 1]) ++i;
            const double l0 = std::log (plateGridF[i]), l1 = std::log (plateGridF[i + 1]);
            const double t = (lf - l0) / (l1 - l0);   // <0 / >1 extrapolate with edge slope
            return juce::jlimit (plot.getX(), plot.getRight(),
                                 (plateGridFx[i] + (float) t * (plateGridFx[i + 1] - plateGridFx[i])) * w);
        }
        const double n = std::log (f / fLo) / std::log (fHi / fLo);
        return plot.getX() + (float) n * plot.getWidth();
    }
    double xToFreq (float x) const
    {
        if (plateMode)
        {
            const float fx = x / (float) getWidth();
            size_t i = 0;
            while (i + 2 < plateGridFx.size() && fx > plateGridFx[i + 1]) ++i;
            const double t = (fx - plateGridFx[i]) / (plateGridFx[i + 1] - plateGridFx[i]);
            const double l0 = std::log (plateGridF[i]), l1 = std::log (plateGridF[i + 1]);
            return juce::jlimit (fLo, fHi, std::exp (l0 + t * (l1 - l0)));
        }
        const double n = (x - plot.getX()) / plot.getWidth();
        return fLo * std::pow (fHi / fLo, n);
    }
    static constexpr double gMax = 30.0;   // gain axis range (dB)
    float gainToY (double dB) const
    {
        if (plateMode)
            return (plateY0Db - (float) dB * plateFracPerDb) * (float) getHeight();
        return plot.getY() + (float) ((gMax - dB) / (2.0 * gMax)) * plot.getHeight();
    }
    double yToGain (float y) const
    {
        if (plateMode)
            return (plateY0Db - y / (float) getHeight()) / plateFracPerDb;
        return gMax - (double) (y - plot.getY()) / plot.getHeight() * (2.0 * gMax);
    }

    // Spectrum has its own vertical scale (independent of the gain axis).
    float specToY (double db) const
    {
        constexpr double floorDb = -80.0, topDb = -4.0;
        const float n = juce::jlimit (0.0f, 1.0f, (float) ((db - floorDb) / (topDb - floorDb)));
        return plot.getBottom() - n * plot.getHeight();
    }

    double bandMagDb (int b, double f) const
    {
        if (raw (b, "on") < 0.5f) return 0.0;
        return eqBandMagDb ((BandType) (int) raw (b, "type"), proc.getSampleRateHz(),
                            raw (b, "freq"), juce::jmax (0.1f, raw (b, "q")), raw (b, "gain"), f);
    }

    int nodeAt (juce::Point<float> pt) const
    {
        for (int b = 0; b < VocalQProcessor::kNumBands; ++b)
            if (nodeCentre (b).getDistanceFrom (pt) < 16.0f)
                return b;
        return -1;
    }

    juce::Point<float> nodeCentre (int b) const
    {
        const int type = (int) raw (b, "type");
        const bool hasGain = type == (int) BandType::Bell || type == (int) BandType::LowShelf || type == (int) BandType::HighShelf;
        return { freqToX (raw (b, "freq")), gainToY (hasGain ? raw (b, "gain") : 0.0f) };
    }

    //========================================================================
    void drawGrid (juce::Graphics& g)
    {
        // floating card face: near-white fill, top highlight, hairline edge
        const auto cardR = getLocalBounds().toFloat();
        g.setColour (theme::card);
        g.fillRoundedRectangle (cardR, 6.0f);
        theme::topHighlight (g, cardR, 6.0f);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (cardR.reduced (0.5f), 6.0f, 1.0f);

        // light recessed plot face — carved into the card so the graph sits down
        // into the panel (inner shadow at the top, light catch at the bottom).
        theme::recess (g, plot, 6.0f);

        const std::array<double, 11> freqs { 16, 32, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
        const std::array<const char*, 11> flab { "16","32","63","125","250","500","1K","2K","4K","8K","16k" };
        g.setFont (theme::font (10.0f));
        for (size_t i = 0; i < freqs.size(); ++i)
        {
            const float x = freqToX (freqs[i]);
            g.setColour (theme::cardLine);
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (theme::inkSoft);
            g.drawText (flab[i], (int) x - 18, 1, 36, 13, juce::Justification::centred, false);
        }

        // gain axis labels only — no horizontal grid lines
        for (int dB = -30; dB <= 30; dB += 6)
        {
            const float y = gainToY (dB);
            g.setColour (theme::inkSoft);
            g.setFont (theme::font (9.5f));
            g.drawText (juce::String (dB), 0, (int) y - 7, 27, 14, juce::Justification::centredRight, false);
        }
    }

    void drawSpectrum (juce::Graphics& g)
    {
        const int N = 220;
        juce::Path p;
        for (int i = 0; i <= N; ++i)
        {
            const double f = fLo * std::pow (fHi / fLo, (double) i / N);
            double db = proc.analyzer.levelDb (f);
            db += 3.0 * std::log2 (juce::jmax (40.0, f) / 1000.0);   // gentle tilt so highs read
            const float x = freqToX (f);   // same axis mapping as the nodes
            const float y = juce::jlimit (plot.getY(), plot.getBottom(), specToY (db));
            if (i == 0) { p.startNewSubPath (x, plot.getBottom()); p.lineTo (x, y); }
            else        p.lineTo (x, y);
        }
        p.lineTo (plot.getRight(), plot.getBottom());
        p.closeSubPath();

        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (plot.toNearestInt());
        // on the dark smoked glass the spectrum reads as soft white light
        const auto specCol = plateMode ? juce::Colours::white : theme::ink;
        juce::ColourGradient grad (specCol.withAlpha (plateMode ? 0.14f : 0.18f), 0.0f, plot.getY(),
                                   specCol.withAlpha (plateMode ? 0.03f : 0.05f), 0.0f, plot.getBottom(), false);
        g.setGradientFill (grad);
        g.fillPath (p);
    }

    void drawCurve (juce::Graphics& g)
    {
        const int N = 256;
        juce::Path curve;
        for (int i = 0; i <= N; ++i)
        {
            const double f = fLo * std::pow (fHi / fLo, (double) i / N);
            double sum = 0.0;
            for (int b = 0; b < VocalQProcessor::kNumBands; ++b) sum += bandMagDb (b, f);
            const float x = freqToX (f);   // same axis mapping as the nodes
            const float y = gainToY (juce::jlimit (-30.0, 30.0, sum));
            if (i == 0) curve.startNewSubPath (x, y);
            else        curve.lineTo (x, y);
        }

        juce::Path fill = curve;
        fill.lineTo (plot.getRight(), gainToY (0.0));
        fill.lineTo (plot.getX(), gainToY (0.0));
        fill.closeSubPath();
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (plot.toNearestInt());
            juce::ColourGradient fg (theme::accent.withAlpha (0.26f), 0.0f, plot.getY(),
                                     theme::accent.withAlpha (0.02f), 0.0f, gainToY (0.0), false);
            g.setGradientFill (fg);
            g.fillPath (fill);
        }

        // soft underglow beneath the curve line, then the crisp stroke on top
        g.setColour (theme::accent.withAlpha (0.22f));
        g.strokePath (curve, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (theme::accent);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawNodes (juce::Graphics& g)
    {
        for (int b = 0; b < VocalQProcessor::kNumBands; ++b)
        {
            const auto c = nodeCentre (b);
            const bool on = raw (b, "on") > 0.5f;
            const bool sel = (b == selectedBand);
            const float rad = sel ? 15.0f : 13.0f;

            // selected node gets a soft accent halo (cheap concentric ellipses)
            if (sel && on)
                for (int k = 3; k >= 1; --k)
                {
                    const float gr = rad + (float) k * 4.0f;
                    g.setColour (theme::accent.withAlpha (0.10f));
                    g.fillEllipse (juce::Rectangle<float> (gr * 2, gr * 2).withCentre (c));
                }

            // contact shadow under every node
            g.setColour (juce::Colours::black.withAlpha (0.12f));
            g.fillEllipse (juce::Rectangle<float> (rad * 2, rad * 2).withCentre (c.translated (0.0f, 1.6f)));

            // body — selected/active nodes use a subtle vertical gradient
            const auto body = juce::Rectangle<float> (rad * 2, rad * 2).withCentre (c);
            if (sel && on)
            {
                juce::ColourGradient ng (theme::accent.brighter (0.10f), body.getX(), body.getY(),
                                         theme::accent.darker (0.08f), body.getX(), body.getBottom(), false);
                g.setGradientFill (ng);
            }
            else
            {
                juce::ColourGradient ng (juce::Colours::white, body.getX(), body.getY(),
                                         juce::Colour (0xfff0f0f3), body.getX(), body.getBottom(), false);
                g.setGradientFill (ng);
            }
            g.fillEllipse (body);
            g.setColour (on ? (sel ? theme::accent : theme::inkSoft.withAlpha (0.6f)) : theme::cardLine);
            g.drawEllipse (body, sel ? 2.0f : 1.4f);

            const int type = (int) raw (b, "type");
            const bool numbered = (b >= 1 && b <= 6 && type == (int) BandType::Bell);
            const juce::Colour fg = sel && on ? juce::Colours::white : theme::ink;
            if (numbered)
            {
                g.setColour (fg);
                g.setFont (theme::font (13.0f, true));
                g.drawText (juce::String (b), juce::Rectangle<float> (rad * 2, rad * 2).withCentre (c),
                            juce::Justification::centred, false);
            }
            else
            {
                drawTypeGlyph (g, (BandType) type, c, fg);
            }
        }
    }

    void drawTypeGlyph (juce::Graphics& g, BandType t, juce::Point<float> c, juce::Colour col)
    {
        juce::Path p;
        const float s = 6.0f;
        switch (t)
        {
            case BandType::LowCut:    p.startNewSubPath (c.x - s, c.y + s); p.lineTo (c.x - 1, c.y + s); p.quadraticTo (c.x + 2, c.y + s, c.x + s, c.y - s); break;
            case BandType::HighCut:   p.startNewSubPath (c.x - s, c.y - s); p.quadraticTo (c.x - 2, c.y + s, c.x + 1, c.y + s); p.lineTo (c.x + s, c.y + s); break;
            case BandType::LowShelf:  p.startNewSubPath (c.x - s, c.y + s); p.lineTo (c.x - 2, c.y + s); p.quadraticTo (c.x + 1, c.y + s, c.x + 3, c.y - s); p.lineTo (c.x + s, c.y - s); break;
            case BandType::HighShelf: p.startNewSubPath (c.x - s, c.y - s); p.lineTo (c.x - 3, c.y - s); p.quadraticTo (c.x - 1, c.y - s, c.x + 2, c.y + s); p.lineTo (c.x + s, c.y + s); break;
            case BandType::Notch:     p.startNewSubPath (c.x - s, c.y - s); p.lineTo (c.x, c.y + s); p.lineTo (c.x + s, c.y - s); break;
            case BandType::Bell:
            default:                  p.addEllipse (c.x - s, c.y - s * 0.6f, s * 2.0f, s * 1.2f); break;
        }
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawMeters (juce::Graphics& g)
    {
        if (plateMode)
        {
            // fill the baked dark channels bottom-up with neon light
            const float w = (float) getWidth(), h = (float) getHeight();
            auto fillBar = [&] (float fx0, float fx1, float dB)
            {
                const float n = juce::jlimit (0.0f, 1.0f, (dB + 40.0f) / 40.0f);
                if (n <= 0.004f) return;
                juce::Rectangle<float> ch (fx0 * w, 0.0408f * h, (fx1 - fx0) * w, (0.9739f - 0.0408f) * h);
                auto fill = ch.withTop (ch.getBottom() - n * ch.getHeight()).reduced (1.5f, 0.0f);
                const float rad = fill.getWidth() * 0.5f;
                juce::Path p; p.addRoundedRectangle (fill, rad);
                theme::glowPath (g, p, 0.30f, 6);
                juce::ColourGradient mg (theme::accentHi, fill.getX(), fill.getY(),
                                         theme::accentLo, fill.getX(), fill.getBottom(), false);
                g.setGradientFill (mg);
                g.fillRoundedRectangle (fill, rad);
            };
            fillBar (0.9451f, 0.9552f, mL);
            fillBar (0.9611f, 0.9717f, mR);
            return;
        }

        auto right = getLocalBounds().toFloat().removeFromRight (58.0f);
        right.removeFromTop (16.0f);
        right.removeFromBottom (4.0f);
        auto scale = right.removeFromRight (24.0f);
        right.reduce (4.0f, 0.0f);

        const float w = (right.getWidth() - 4.0f) * 0.5f;
        auto barL = right.removeFromLeft (w);
        right.removeFromLeft (4.0f);
        auto barR = right.removeFromLeft (w);

        auto drawBar = [&] (juce::Rectangle<float> r, float dB)
        {
            theme::recess (g, r, 3.0f);
            const float n = juce::jlimit (0.0f, 1.0f, (dB + 40.0f) / 40.0f);
            auto fillR = r.withTop (r.getBottom() - n * r.getHeight());
            const auto top = dB > -1.0f ? juce::Colour (0xffe53935) : theme::accent;
            juce::ColourGradient mg (top.brighter (0.18f), fillR.getX(), fillR.getY(),
                                     top.darker (0.06f), fillR.getRight(), fillR.getY(), false);
            g.setGradientFill (mg);
            g.fillRoundedRectangle (fillR, 3.0f);
            // glossy vertical sheen down the left third of the fill
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.fillRoundedRectangle (fillR.withWidth (fillR.getWidth() * 0.4f), 3.0f);
        };
        drawBar (barL, mL);
        drawBar (barR, mR);

        g.setColour (theme::inkSoft);
        g.setFont (theme::font (9.0f));
        for (int dB = 0; dB >= -40; dB -= 4)
        {
            const float n = (float) (dB + 40) / 40.0f;
            const float y = scale.getBottom() - n * scale.getHeight();
            g.drawText (juce::String (dB), scale.withY (y - 6).withHeight (12),
                        juce::Justification::centredLeft, false);
        }
    }

    VocalQProcessor& proc;
    juce::Rectangle<float> plot;
    bool plateMode = false;
    int dragBand = -1;
    bool dragging = false;
    juce::Point<float> downPos, grabOffset;
    float mL = -100.0f, mR = -100.0f;
};
