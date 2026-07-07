#include "PluginEditor.h"

namespace
{
    juce::String pctText (float v)
    {
        return juce::String (juce::roundToInt (v)) + "%";
    }

    juce::String dbText (float v)
    {
        const juce::String s = juce::String (v, 1) + " dB";
        return v > 0.0f ? "+" + s : s;
    }
}

//==============================================================================
// Baked-plate geometry, measured on the 2336x1744 clip chassis plates
// (ON-vs-OFF diff for pill bands, pink radial profiles for ring centres and
// radii, OFF-plate luminance for the screen window and caption rows).
namespace plategeo
{
    constexpr float PX = 2336.0f, PY = 1744.0f;
    constexpr float px (float v) { return v / PX; }
    constexpr float py (float v) { return v / PY; }

    // hero rings (drive / ceiling) — pink radial profile: rIn 119, solid 152, bloom 168
    constexpr float drvCx = px (611.0f),   drvCy = py (1023.5f);
    constexpr float ceilCx = px (1705.5f), ceilCy = py (1022.5f);
    constexpr float bigDomeDia = px (238.0f);
    constexpr float bigDomeR = px (119.0f), bigSolidR = px (152.0f), bigMaxR = px (168.0f);
    // live values go in the gap between ring bottom and the baked caption
    constexpr float bigValY0 = py (1198.0f), bigValY1 = py (1248.0f);
    constexpr float bigValHalfW = px (170.0f);

    // small rings (mix / output) — rIn 44, solid 62, bloom 72
    constexpr float mixCx = px (611.0f),  mixCy = py (1456.5f);
    constexpr float outCx = px (1716.0f), outCy = py (1459.0f);
    constexpr float smallDomeDia = px (88.0f);
    constexpr float smallDomeR = px (44.0f), smallSolidR = px (62.0f), smallMaxR = px (72.0f);
    constexpr float smallValY0 = py (1538.0f), smallValY1 = py (1596.0f);
    constexpr float valHalfW = px (120.0f);

    // shape pills (SOFT / WARM / HARD stack) — bodies with seam-to-seam bounds
    // (ON plate has ALL pills lit; masks must stop at the seam valleys)
    constexpr float shpX0 = px (990.0f), shpX1 = px (1344.0f);
    constexpr float shpTop = py (885.0f);
    constexpr float shpSeam1 = py (1017.5f);   // SOFT | WARM valley
    constexpr float shpSeam2 = py (1128.0f);   // WARM | HARD valley
    constexpr float shpBot = py (1233.0f);

    // HQ pill (isolated, safe to include glow)
    constexpr float hqX0 = px (1040.0f), hqX1 = px (1300.0f);
    constexpr float hqY0 = py (1402.0f), hqY1 = py (1527.0f);

    // live clip screen window (baked dark glass; display insets past the lip)
    constexpr float scrX0 = px (166.0f), scrX1 = px (2153.0f);
    constexpr float scrY0 = py (242.0f), scrY1 = py (780.0f);

    // preset dropdown pill + arrow steppers (top-right)
    constexpr float boxX0 = px (1556.0f), boxX1 = px (1950.0f);
    constexpr float boxY0 = py (99.0f),   boxY1 = py (200.0f);
    constexpr float boxTxtX1 = px (1880.0f);
    constexpr float arrX0 = px (1956.0f), arrMid = px (2064.0f), arrX1 = px (2172.0f);
    constexpr float arrY0 = py (99.0f),   arrY1 = py (198.0f);
}

//==============================================================================
VocalClipEditor::VocalClipEditor (VocalClipProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);

    chassisImg   = skin::image ("clip-chassis@2x.png");
    chassisOnImg = skin::image ("clip-chassis-on@2x.png");
    plateBaked = chassisImg.isValid() && chassisOnImg.isValid();

    // ---- branding ----
    brand.setText ({}, juce::dontSendNotification);
    brand.setFont (theme::font (26.0f, true));
    brand.setColour (juce::Label::textColourId, theme::ink);
    addAndMakeVisible (brand);

    brandSub.setText ({}, juce::dontSendNotification);
    brandSub.setFont (theme::font (12.0f, false));
    brandSub.setColour (juce::Label::textColourId, theme::inkSoft);
    addAndMakeVisible (brandSub);

    // ---- preset capsule ----
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0) proc.setCurrentProgram (id - 1);
    };
    addAndMakeVisible (presetBox);

    prevBtn.setClickingTogglesState (false);
    nextBtn.setClickingTogglesState (false);
    prevBtn.onClick = [this] { stepProgram (-1); };
    nextBtn.onClick = [this] { stepProgram ( 1); };
    for (auto* b : { &prevBtn, &nextBtn })
    {
        b->setColour (juce::TextButton::textColourOffId, theme::accent);
        addAndMakeVisible (*b);
    }

    // ---- live clip screen (added before knobs so it sits underneath) ----
    addAndMakeVisible (display);

    // ---- knobs + attachments ----
    auto addKnob = [this] (LabeledKnob& k, const char* id, int slot)
    {
        addAndMakeVisible (k);
        knobAtt[(size_t) slot] = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
    };
    addKnob (driveKnob,   "drive",   0);
    addKnob (ceilingKnob, "ceiling", 1);
    addKnob (mixKnob,     "mix",     2);
    addKnob (outputKnob,  "output",  3);

    // ---- shape selector (segmented pill, radio behaviour) ----
    int shapeIdx = 0;
    for (auto* b : { &softBtn, &warmBtn, &hardBtn })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (42);
        const int idx = shapeIdx++;
        b->onClick = [this, idx] { setShape (idx); };
        addAndMakeVisible (*b);
    }
    syncShapeButtons();

    // ---- HQ pill ----
    hqBtn.setClickingTogglesState (true);
    addAndMakeVisible (hqBtn);
    hqAtt = std::make_unique<ButtonAtt> (proc.apvts, "hq", hqBtn);

    if (plateBaked)
        setupPlateMode();

    startTimerHz (60);
    // plate mode: the window shows ONLY the plate (cropped at the chrome edge),
    // and must match the crop's aspect exactly or circular dome sprites sit in
    // vertically-stretched (elliptical) baked grooves
    if (plateBaked)
        setSize (900, juce::roundToInt (900.0f * (float) plateCrop.getHeight()
                                               / (float) plateCrop.getWidth()));
    else
        setSize (900, 660);

    // License overlay sits on top of everything; it shows itself until activated.
    addChildComponent (licenseOverlay);
    licenseOverlay.setBounds (getLocalBounds());
}

VocalClipEditor::~VocalClipEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void VocalClipEditor::stepProgram (int delta)
{
    const int n = proc.getNumPrograms();
    if (n <= 0) return;
    const int idx = (proc.getCurrentProgram() + delta + n) % n;
    proc.setCurrentProgram (idx);
}

void VocalClipEditor::setShape (int shapeIndex)
{
    if (auto* param = proc.apvts.getParameter ("shape"))
        param->setValueNotifyingHost (param->convertTo0to1 ((float) shapeIndex));
}

void VocalClipEditor::syncShapeButtons()
{
    const int shape = (int) proc.apvts.getRawParameterValue ("shape")->load();
    softBtn.setToggleState (shape == 0, juce::dontSendNotification);
    warmBtn.setToggleState (shape == 1, juce::dontSendNotification);
    hardBtn.setToggleState (shape == 2, juce::dontSendNotification);
}

//==============================================================================
// Baked-plate mode: static art comes from the OFF chassis plate; lit states
// are revealed by blitting the same regions from the pixel-registered ON
// plate. Knobs draw rotating chrome dome sprites; captions are baked, value
// read-outs drawn live.
void VocalClipEditor::setupPlateMode()
{
    laf.plate = true;
    plateCrop = skin::plateBounds (chassisImg);
    laf.domeLarge = skin::cropToDome (skin::image ("grit-knob-large@2x.png"),
                                      0.1999f, 0.3533f, 0.199f);
    laf.domeSmall = skin::cropToDome (skin::image ("grit-knob-small@2x.png"),
                                      0.4993f, 0.4648f, 0.615f);

    driveKnob.setPlate (true, "dome-large");
    ceilingKnob.setPlate (true, "dome-large");
    for (auto* k : { &mixKnob, &outputKnob })
        k->setPlate (true, "dome-small");

    // buttons/combos become invisible hit areas; lit states masked in paintPlate
    for (auto* b : { &softBtn, &warmBtn, &hardBtn, &hqBtn, &prevBtn, &nextBtn })
        b->setComponentID ("hit");
    presetBox.setComponentID ("hit");

    brand.setVisible (false);
    brandSub.setVisible (false);
}

// plategeo fractions are of the FULL generated canvas; the window shows only
// the plateCrop region, so map full-canvas fraction -> cropped screen px.
juce::Rectangle<int> VocalClipEditor::plateFracRect (float fx0, float fy0, float fx1, float fy1) const
{
    const float iw = (float) chassisImg.getWidth(), ih = (float) chassisImg.getHeight();
    const float sx = (float) getWidth()  / (float) plateCrop.getWidth();
    const float sy = (float) getHeight() / (float) plateCrop.getHeight();
    return juce::Rectangle<float> ((fx0 * iw - (float) plateCrop.getX()) * sx,
                                   (fy0 * ih - (float) plateCrop.getY()) * sy,
                                   (fx1 - fx0) * iw * sx,
                                   (fy1 - fy0) * ih * sy).toNearestInt();
}

// Blit the matching region of the lit plate over the base plate. Both scaled
// caches share the editor's coordinate space, so this is a 1:1 copy — cheap,
// and registration is exact by construction.
void VocalClipEditor::maskFromOn (juce::Graphics& g, juce::Rectangle<int> screenRect)
{
    g.drawImage (plateOnScaled,
                 screenRect.getX(), screenRect.getY(), screenRect.getWidth(), screenRect.getHeight(),
                 screenRect.getX(), screenRect.getY(), screenRect.getWidth(), screenRect.getHeight());
}

// Same reveal but with a soft alpha ramp along the rect border, so the slight
// global tone drift between the two plates never shows as a hard rectangle.
void VocalClipEditor::maskFromOnFeathered (juce::Graphics& g, juce::Rectangle<int> screenRect,
                                           int featherPx)
{
    constexpr int n = 4;
    const float s = (float) featherPx / (float) n;
    const auto r = screenRect.toFloat();

    g.saveState();
    g.reduceClipRegion (r.reduced ((float) featherPx).toNearestInt());
    maskFromOn (g, screenRect);
    g.restoreState();

    for (int j = 0; j < n; ++j)     // j = 0 is the outermost, faintest band
    {
        juce::Path band;
        band.addRectangle (r.reduced (s * (float) j));
        band.addRectangle (r.reduced (s * (float) (j + 1)));
        band.setUsingNonZeroWinding (false);
        g.saveState();
        g.reduceClipRegion (band);
        g.setOpacity (((float) j + 0.5f) / (float) n);
        maskFromOn (g, screenRect);
        g.restoreState();
    }
}

// Reveal a knob's lit neon ring as an annular wedge clipped to the value sweep
// (6 o'clock -> 6 o'clock), with an angular feather on the leading edge and a
// radial fade on the outer bloom so no hard cut ever shows.
void VocalClipEditor::drawRingWedge (juce::Graphics& g, juce::Slider& s, float cxFrac, float cyFrac,
                                     float domeRFrac, float solidRFrac, float maxRFrac)
{
    // fractions are of the full canvas; convert through the crop mapping
    const float iw = (float) chassisImg.getWidth(), ih = (float) chassisImg.getHeight();
    const float sx = (float) getWidth() / (float) plateCrop.getWidth();
    const juce::Point<float> c ((cxFrac * iw - (float) plateCrop.getX()) * sx,
                                (cyFrac * ih - (float) plateCrop.getY())
                                    * (float) getHeight() / (float) plateCrop.getHeight());
    const float domeR  = domeRFrac  * iw * sx;
    const float solidR = solidRFrac * iw * sx;
    const float R      = maxRFrac   * iw * sx;

    const float prop = juce::jlimit (0.0f, 1.0f,
                                     (float) s.valueToProportionOfLength (s.getValue()));
    const float a0 = juce::MathConstants<float>::pi;
    const bool full = prop >= 0.995f;
    if (! full && prop <= 0.002f) return;
    const float a1 = a0 + prop * juce::MathConstants<float>::twoPi;

    const juce::Rectangle<int> box ((int) std::floor (c.x - R), (int) std::floor (c.y - R),
                                    (int) std::ceil (R * 2.0f), (int) std::ceil (R * 2.0f));

    auto wedge = [&] (float from, float to, float rIn, float rOut, float alpha)
    {
        if (to - from <= 0.0005f || rOut - rIn <= 0.5f) return;
        juce::Path p;
        p.addPieSegment (c.x - rOut, c.y - rOut, rOut * 2.0f, rOut * 2.0f, from, to, rIn / rOut);
        g.saveState();
        g.reduceClipRegion (p);
        g.setOpacity (alpha);
        maskFromOn (g, box);
        g.restoreState();
    };

    // both ends of the arc are feathered so there is never a hard radial cut:
    // fIn fades the start (6 o'clock) in, fOut fades the leading edge out. At
    // full value the ring is a seamless uninterrupted annulus.
    const float span = a1 - a0;
    const float fOut = full ? 0.0f : juce::jmin (0.22f, span * 0.40f);
    const float fIn  = full ? 0.0f : juce::jmin (0.10f, span * 0.20f);
    const float aEnd = full ? a0 + juce::MathConstants<float>::twoPi : a1;

    wedge (a0 + fIn, aEnd - fOut, domeR, solidR, 1.0f);
    constexpr int aSteps = 10;
    for (int i = 0; i < aSteps; ++i)
    {
        const float t0 = (float) i / aSteps, t1 = (float) (i + 1) / aSteps;
        // leading edge fade-out
        wedge (aEnd - fOut * (1.0f - t0), aEnd - fOut * (1.0f - t1),
               domeR, solidR, 1.0f - (t0 + t1) * 0.5f);
        // start edge fade-in
        wedge (a0 + fIn * t0, a0 + fIn * t1,
               domeR, solidR, (t0 + t1) * 0.5f);
    }

    // outer bloom band, faded radially and softened at both angular ends
    constexpr int rSteps = 4;
    for (int i = 0; i < rSteps; ++i)
    {
        const float alpha = 0.85f * (1.0f - ((float) i + 0.5f) / rSteps);
        const float rIn  = solidR + (R - solidR) * (float) i / rSteps;
        const float rOut = solidR + (R - solidR) * (float) (i + 1) / rSteps;
        wedge (a0 + fIn, aEnd - fOut * 0.5f, rIn, rOut, alpha);
        wedge (a0,            a0 + fIn * 0.5f, rIn, rOut, alpha * 0.33f);
        wedge (a0 + fIn * 0.5f, a0 + fIn,      rIn, rOut, alpha * 0.66f);
    }
}

void VocalClipEditor::paintPlate (juce::Graphics& g)
{
    using namespace plategeo;

    g.drawImageAt (plateScaled, 0, 0);   // cached 1:1 blit — no per-frame rescale

    const int feather = juce::roundToInt ((float) getWidth() * 0.008f);

    // ---- shape pills (only the selected pill's seam-to-seam band lights up)
    {
        const int shape = (int) proc.apvts.getRawParameterValue ("shape")->load();
        const float ys[4] = { shpTop, shpSeam1, shpSeam2, shpBot };
        maskFromOnFeathered (g, plateFracRect (shpX0, ys[shape], shpX1, ys[shape + 1]), feather);
    }

    // ---- HQ pill
    if (hqBtn.getToggleState())
        maskFromOnFeathered (g, plateFracRect (hqX0, hqY0, hqX1, hqY1), feather);

    // ---- knob neon ring wedges
    drawRingWedge (g, driveKnob.slider,   drvCx,  drvCy,  bigDomeR, bigSolidR, bigMaxR);
    drawRingWedge (g, ceilingKnob.slider, ceilCx, ceilCy, bigDomeR, bigSolidR, bigMaxR);
    drawRingWedge (g, mixKnob.slider,    mixCx, mixCy, smallDomeR, smallSolidR, smallMaxR);
    drawRingWedge (g, outputKnob.slider, outCx, outCy, smallDomeR, smallSolidR, smallMaxR);

    // ---- live value read-outs (captions are baked; values are dynamic ink)
    auto value = [&] (const LabeledKnob& k, float fx0, float fy0, float fx1, float fy1, float fontH)
    {
        g.setColour (theme::ink);
        g.setFont (theme::font (fontH, false));
        g.drawText (k.value.getText(), plateFracRect (fx0, fy0, fx1, fy1),
                    juce::Justification::centred);
    };
    value (driveKnob,   drvCx - bigValHalfW,  bigValY0, drvCx + bigValHalfW,  bigValY1, 16.0f);
    value (ceilingKnob, ceilCx - bigValHalfW, bigValY0, ceilCx + bigValHalfW, bigValY1, 16.0f);
    value (mixKnob,    mixCx - valHalfW, smallValY0, mixCx + valHalfW, smallValY1, 13.5f);
    value (outputKnob, outCx - valHalfW, smallValY0, outCx + valHalfW, smallValY1, 13.5f);

    // ---- preset name inside the dropdown pill
    g.setColour (theme::ink);
    g.setFont (theme::font (14.0f, false));
    g.drawText (presetBox.getText(), plateFracRect (boxX0 + px (30.0f), boxY0, boxTxtX1, boxY1),
                juce::Justification::centred);
}

void VocalClipEditor::layoutPlate()
{
    using namespace plategeo;

    // rebuild the scaled plate caches for the new size (1:1 blits per frame)
    plateScaled   = skin::renderPlate (chassisImg,   plateCrop, getWidth(), getHeight());
    plateOnScaled = skin::renderPlate (chassisOnImg, plateCrop, getWidth(), getHeight());

    auto fr = [this] (float fx0, float fy0, float fx1, float fy1)
    {
        return plateFracRect (fx0, fy0, fx1, fy1);
    };
    const float iw = (float) chassisImg.getWidth(), ih = (float) chassisImg.getHeight();
    const float sx = (float) getWidth()  / (float) plateCrop.getWidth();
    const float sy = (float) getHeight() / (float) plateCrop.getHeight();

    auto domeSquare = [&] (float cx, float cy, float diaFrac)
    {
        const float side = diaFrac * iw * sx * 1.06f;
        const float px2 = (cx * iw - (float) plateCrop.getX()) * sx;
        const float py2 = (cy * ih - (float) plateCrop.getY()) * sy;
        return juce::Rectangle<float> (px2 - side * 0.5f, py2 - side * 0.5f,
                                       side, side).toNearestInt();
    };
    driveKnob.setBounds   (domeSquare (drvCx,  drvCy,  bigDomeDia));
    ceilingKnob.setBounds (domeSquare (ceilCx, ceilCy, bigDomeDia));
    mixKnob.setBounds    (domeSquare (mixCx, mixCy, smallDomeDia));
    outputKnob.setBounds (domeSquare (outCx, outCy, smallDomeDia));

    softBtn.setBounds (fr (shpX0, shpTop,   shpX1, shpSeam1));
    warmBtn.setBounds (fr (shpX0, shpSeam1, shpX1, shpSeam2));
    hardBtn.setBounds (fr (shpX0, shpSeam2, shpX1, shpBot));
    hqBtn.setBounds (fr (hqX0, hqY0, hqX1, hqY1));
    display.setBounds (fr (scrX0, scrY0, scrX1, scrY1));

    presetBox.setBounds (fr (boxX0, boxY0, boxX1, boxY1));
    prevBtn.setBounds (fr (arrX0, arrY0, arrMid, arrY1));
    nextBtn.setBounds (fr (arrMid, arrY0, arrX1, arrY1));

    arrowBox = {};
    shapeBox = {};
    brand.setBounds (0, 0, 0, 0);
    brandSub.setBounds (0, 0, 0, 0);
}

//==============================================================================
void VocalClipEditor::timerCallback()
{
    const float ceilDb = proc.apvts.getRawParameterValue ("ceiling")->load();
    display.refresh (proc.apvts.getRawParameterValue ("drive")->load(),
                     (int) proc.apvts.getRawParameterValue ("shape")->load(),
                     ceilDb);   // repaints only its own bounds

    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);

    syncShapeButtons();

    driveKnob.setValueText (dbText (proc.apvts.getRawParameterValue ("drive")->load()));
    ceilingKnob.setValueText (dbText (ceilDb));
    mixKnob.setValueText (pctText (proc.apvts.getRawParameterValue ("mix")->load()));
    outputKnob.setValueText (dbText (proc.apvts.getRawParameterValue ("output")->load()));

    if (! plateBaked)
    {
        repaint();
        return;
    }

    // dirty-region repaints: only invalidate what actually changed this tick
    using namespace plategeo;
    struct KnobRegion { LabeledKnob* k; float cx, cy, maxR; juce::Rectangle<int> val; };
    const KnobRegion regions[] = {
        { &driveKnob,   drvCx,  drvCy,  bigMaxR, plateFracRect (drvCx - bigValHalfW,  bigValY0, drvCx + bigValHalfW,  bigValY1) },
        { &ceilingKnob, ceilCx, ceilCy, bigMaxR, plateFracRect (ceilCx - bigValHalfW, bigValY0, ceilCx + bigValHalfW, bigValY1) },
        { &mixKnob,    mixCx, mixCy, smallMaxR, plateFracRect (mixCx - valHalfW, smallValY0, mixCx + valHalfW, smallValY1) },
        { &outputKnob, outCx, outCy, smallMaxR, plateFracRect (outCx - valHalfW, smallValY0, outCx + valHalfW, smallValY1) },
    };
    // radius fractions are of image WIDTH; vertical extents need the aspect factor
    const float ar = (float) chassisImg.getWidth() / (float) chassisImg.getHeight();
    for (size_t i = 0; i < 4; ++i)
    {
        const double v = regions[i].k->slider.getValue();
        if (v != shownKnob[i])
        {
            shownKnob[i] = v;
            repaint (plateFracRect (regions[i].cx - regions[i].maxR, regions[i].cy - regions[i].maxR * ar,
                                    regions[i].cx + regions[i].maxR, regions[i].cy + regions[i].maxR * ar)
                         .expanded (2));
            repaint (regions[i].val.expanded (2));
        }
    }
    const int shape = (int) proc.apvts.getRawParameterValue ("shape")->load();
    if (shape != shownShape)
    {
        shownShape = shape;
        repaint (plateFracRect (shpX0, shpTop, shpX1, shpBot).expanded (12));
    }
    const bool hq = hqBtn.getToggleState();
    if (hq != shownHq)
    {
        shownHq = hq;
        repaint (plateFracRect (hqX0, hqY0, hqX1, hqY1).expanded (12));
    }
    if (presetBox.getText() != shownPreset)
    {
        shownPreset = presetBox.getText();
        repaint (plateFracRect (boxX0, boxY0, boxTxtX1, boxY1).expanded (2));
    }
}

//==============================================================================
void VocalClipEditor::paint (juce::Graphics& g)
{
    if (plateBaked)
    {
        paintPlate (g);
        return;
    }

    // warm off-white gradient backdrop + soft top light
    theme::backdrop (g, getLocalBounds());

    // main panel: a floating card surface (top highlight + hairline)
    auto frame = getLocalBounds().toFloat().reduced (8.0f);
    g.setColour (theme::card);
    g.fillRoundedRectangle (frame, 26.0f);
    theme::topHighlight (g, frame, 26.0f);
    g.setColour (theme::cardLine);
    g.drawRoundedRectangle (frame, 26.0f, 1.2f);

    // two-tone wordmark: ink "vocal" + accent "clip" with an accent underline
    {
        auto wm = theme::font (26.0f, true);
        g.setFont (wm);
        const float vw = juce::GlyphArrangement::getStringWidth (wm, "vocal");
        g.setColour (theme::ink);
        g.drawText ("vocal", 28, 22, 240, 34, juce::Justification::left);
        g.setColour (theme::accent);
        g.drawText ("clip", 28 + (int) vw, 22, 240, 34, juce::Justification::left);
        g.setColour (theme::accent);
        g.fillRoundedRectangle (29.0f, 51.0f, 22.0f, 2.5f, 1.25f);
    }
    theme::spacedText (g, "VOCAL CLIPPER", brandSub.getBounds().toFloat(),
                       theme::inkSoft, 9.5f, 2.6f, false, juce::Justification::centredLeft);

    // preset stepper box: clean white pill with a centre divider
    {
        auto ab = arrowBox.toFloat();
        juce::ColourGradient wg (juce::Colours::white, ab.getX(), ab.getY(),
                                 juce::Colour (0xfff4f5f8), ab.getX(), ab.getBottom(), false);
        g.setGradientFill (wg);
        g.fillRoundedRectangle (ab, 12.0f);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (ab, 12.0f, 1.2f);
        g.drawVerticalLine (arrowBox.getCentreX(), (float) arrowBox.getY() + 6.0f,
                            (float) arrowBox.getBottom() - 6.0f);
    }

    // shape column caption
    theme::spacedText (g, "SHAPE", juce::Rectangle<float> ((float) shapeBox.getX(),
                       (float) shapeBox.getY() - 24.0f, (float) shapeBox.getWidth(), 16.0f),
                       theme::inkSoft, 9.5f, 2.6f, false, juce::Justification::centred);

    // hairline divider between the hero row and the small-knob row
    g.setColour (theme::cardLine);
    g.drawLine (60.0f, 524.0f, 840.0f, 524.0f, 1.2f);
}

//==============================================================================
void VocalClipEditor::resized()
{
    licenseOverlay.setBounds (getLocalBounds());

    if (plateBaked)
    {
        layoutPlate();
        return;
    }

    // ---- branding ----
    brand.setBounds (28, 22, 240, 34);
    brandSub.setBounds (30, 56, 240, 18);

    // ---- preset capsule (top-right) ----
    arrowBox = juce::Rectangle<int> (776, 28, 96, 38);
    prevBtn.setBounds (arrowBox.getX() + 5, arrowBox.getY() + 4,
                       arrowBox.getWidth() / 2 - 7, arrowBox.getHeight() - 8);
    nextBtn.setBounds (arrowBox.getCentreX() + 2, arrowBox.getY() + 4,
                       arrowBox.getWidth() / 2 - 7, arrowBox.getHeight() - 8);
    presetBox.setBounds (598, 28, 170, 38);

    // ---- live transfer-curve screen (the FL-style centrepiece) ----
    display.setBounds (40, 90, getWidth() - 80, 210);

    // ---- hero row: DRIVE | shape column | CEILING ----
    driveKnob.setBounds   (150, 318, 172, 202);
    ceilingKnob.setBounds (578, 318, 172, 202);

    shapeBox = juce::Rectangle<int> (386, 344, 128, 150);
    const int bh = shapeBox.getHeight() / 3;
    softBtn.setBounds (shapeBox.getX(), shapeBox.getY(),               shapeBox.getWidth(), bh - 6);
    warmBtn.setBounds (shapeBox.getX(), shapeBox.getY() + bh,          shapeBox.getWidth(), bh - 6);
    hardBtn.setBounds (shapeBox.getX(), shapeBox.getY() + bh * 2,      shapeBox.getWidth(), bh - 6);

    // ---- second row aligned under the hero columns:
    //      mix under DRIVE, HQ under the shape stack, output under CEILING ----
    {
        const int w = 120, h = 118, y = 532;
        mixKnob.setBounds    (driveKnob.getBounds().getCentreX() - w / 2, y, w, h);
        outputKnob.setBounds (ceilingKnob.getBounds().getCentreX() - w / 2, y, w, h);
        hqBtn.setBounds (shapeBox.getCentreX() - 46, y + 34, 92, 36);
    }
}
