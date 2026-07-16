#include "PluginEditor.h"

//==============================================================================
// Baked-plate geometry, measured on the 1744x2336 geek chassis plates
// (ON-vs-OFF diff components for pills/discs/meters, least-squares circle fits
// on the saturated neon cores for the knob rings, OFF-plate luminance for the
// screen window and the bay rim).
namespace plategeo
{
    // full generated canvas in px (helpers below convert through the crop)
    constexpr float screenX0 = 440.0f,  screenY0 = 322.0f,  screenX1 = 1296.0f, screenY1 = 1118.0f;

    constexpr float headerX0 = 290.0f,  headerY0 = 112.0f,  headerX1 = 1460.0f, headerY1 = 196.0f;
    constexpr float recX0 = 318.0f, recY0 = 590.0f, recX1 = 384.0f, recY1 = 657.0f;

    // cartridge bay sprite canvas (the cut sprites share this exact rect)
    constexpr float bayX0 = 640.0f, bayY0 = 1240.0f, bayX1 = 1120.0f, bayY1 = 1560.0f;

    constexpr float tapX0 = 805.0f, tapY0 = 1531.0f, tapX1 = 926.0f, tapY1 = 1619.0f;
    constexpr float prnX0 = 802.0f, prnY0 = 1640.0f, prnX1 = 926.0f, prnY1 = 1729.0f;

    constexpr float dpadCx = 478.0f, dpadCy = 1614.0f, dpadSide = 500.0f;

    constexpr float hitACx = 1136.0f, hitACy = 1680.0f;
    constexpr float hitBCx = 1352.0f, hitBCy = 1516.0f;
    constexpr float hitDomeDia = 212.0f;   // sprite draw size
    constexpr float hitGlowR   = 126.0f;   // lit-disc blit half-size

    constexpr float doseCx = 394.0f, doseCy = 2030.0f;
    constexpr float outCx = 1348.0f, outCy = 2032.0f;
    constexpr float knobDomeDia = 150.0f;  // hugs the measured seat groove (r 71-75)
    constexpr float ringDomeR = 76.0f, ringSolidR = 112.0f, ringMaxR = 128.0f;

    // 11 meter bars (x0,x1 pairs) spanning y 1975..2112
    constexpr float meterY0 = 1970.0f, meterY1 = 2118.0f;
    constexpr float meterBars[11][2] = {
        { 549.0f, 584.0f }, { 606.0f, 642.0f }, { 664.0f, 700.0f }, { 723.0f, 760.0f },
        { 784.0f, 820.0f }, { 841.0f, 878.0f }, { 900.0f, 937.0f }, { 958.0f, 994.0f },
        { 1016.0f, 1052.0f }, { 1074.0f, 1111.0f }, { 1132.0f, 1168.0f } };
}

namespace
{
    const char* onPlateFor (int theme)
    {
        switch (theme)
        {
            case 1:  return "geek-chassis-on-smoke@2x.png";
            case 2:  return "geek-chassis-on-acid@2x.png";
            case 3:  return "geek-chassis-on-snow@2x.png";
            case 4:  return "geek-chassis-on-geeked@2x.png";
            default: return "geek-chassis-on@2x.png";
        }
    }

    const char* baySpriteFor (int theme)
    {
        switch (theme)
        {
            case 1:  return "geek-bay-smoke@2x.png";
            case 2:  return "geek-bay-acid@2x.png";
            case 3:  return "geek-bay-snow@2x.png";
            case 4:  return "geek-bay-geeked@2x.png";
            default: return "geek-bay-lean@2x.png";
        }
    }

    const char* hitSpriteFor (int theme)
    {
        switch (theme)
        {
            case 1:  return "geek-hit-smoke@2x.png";
            case 2:  return "geek-hit-acid@2x.png";
            case 3:  return "geek-hit-snow@2x.png";
            case 4:  return "geek-hit-geeked@2x.png";
            default: return "geek-hit@2x.png";
        }
    }
}

//==============================================================================
VocalGeekEditor::VocalGeekEditor (VocalGeekProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);

    chassisImg   = skin::image ("geek-chassis@2x.png");
    chassisOnImg = skin::image ("geek-chassis-on@2x.png");
    plateBaked = chassisImg.isValid() && chassisOnImg.isValid();

    brand.setText ({}, juce::dontSendNotification);
    brand.setFont (theme::font (26.0f, true));
    brand.setColour (juce::Label::textColourId, theme::ink);
    addAndMakeVisible (brand);

    // ---- pixel screen (under everything else) ----
    addAndMakeVisible (display);

    // ---- knobs + attachments ----
    auto addKnob = [this] (LabeledKnob& k, const char* id, int slot)
    {
        addAndMakeVisible (k);
        knobAtt[(size_t) slot] = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
    };
    addKnob (doseKnob,   "dose",   0);
    addKnob (outputKnob, "output", 1);

    // ---- performance pads ----
    hitA.setParam (proc.apvts.getParameter ("hita"));
    hitB.setParam (proc.apvts.getParameter ("hitb"));
    addAndMakeVisible (hitA);
    addAndMakeVisible (hitB);

    dpad.onNudge = [this] (int dx, int dy)
    {
        auto nudge = [this] (const char* id, float delta)
        {
            if (auto* par = proc.apvts.getParameter (id))
            {
                par->beginChangeGesture();
                par->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, par->getValue() + delta));
                par->endChangeGesture();
            }
        };
        if (dx != 0) nudge ("space",   (float) dx * 0.1f);
        if (dy != 0) nudge ("texture", (float) -dy * 0.1f);   // up = more texture
    };
    addAndMakeVisible (dpad);

    bay.onCycle = [this] { cycleTheme(); };
    addAndMakeVisible (bay);

    tapBtn.onClick = [this] { tapClicked(); };
    printBtn.onClick = [this] { togglePrint(); };
    addAndMakeVisible (tapBtn);
    addAndMakeVisible (printBtn);

    if (plateBaked)
        setupPlateMode();

    applyTheme ((int) proc.apvts.getRawParameterValue ("theme")->load());

    startTimerHz (60);
    if (plateBaked)
        setSize (470, juce::roundToInt (470.0f * (float) plateCrop.getHeight()
                                               / (float) plateCrop.getWidth()));
    else
        setSize (470, 700);

    addChildComponent (licenseOverlay);
    licenseOverlay.setBounds (getLocalBounds());
}

VocalGeekEditor::~VocalGeekEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void VocalGeekEditor::applyTheme (int themeIndex)
{
    if (themeIndex == currentTheme) return;
    currentTheme = themeIndex;

    if (plateBaked)
    {
        auto themed = skin::image (onPlateFor (themeIndex));
        chassisOnImg = themed.isValid() ? themed : skin::image ("geek-chassis-on@2x.png");
        if (getWidth() > 0)
            plateOnScaled = skin::renderPlate (chassisOnImg, plateCrop, getWidth(), getHeight());
    }

    bay.setSprite (skin::image (baySpriteFor (themeIndex)));
    auto hitImg = skin::image (hitSpriteFor (themeIndex));
    if (! hitImg.isValid()) hitImg = skin::image ("geek-hit@2x.png");
    hitA.setSprite (hitImg);
    hitB.setSprite (hitImg);

    repaint();
}

void VocalGeekEditor::cycleTheme()
{
    if (auto* par = proc.apvts.getParameter ("theme"))
    {
        const int next = (((int) proc.apvts.getRawParameterValue ("theme")->load()) + 1) % 5;
        par->beginChangeGesture();
        par->setValueNotifyingHost (par->convertTo0to1 ((float) next));
        par->endChangeGesture();
    }
}

void VocalGeekEditor::tapClicked()
{
    if (auto* par = proc.apvts.getParameter ("rate"))
    {
        const int next = (((int) proc.apvts.getRawParameterValue ("rate")->load()) + 1) % 4;
        par->beginChangeGesture();
        par->setValueNotifyingHost (par->convertTo0to1 ((float) next));
        par->endChangeGesture();
    }
    tapFlashUntil = juce::Time::currentTimeMillis() + 160;
}

void VocalGeekEditor::togglePrint()
{
    if (auto* par = proc.apvts.getParameter ("print"))
    {
        const bool on = proc.apvts.getRawParameterValue ("print")->load() > 0.5f;
        par->beginChangeGesture();
        par->setValueNotifyingHost (on ? 0.0f : 1.0f);
        par->endChangeGesture();
    }
}

//==============================================================================
void VocalGeekEditor::setupPlateMode()
{
    laf.plate = true;
    plateCrop = skin::plateBounds (chassisImg);
    laf.domeSmall = skin::cropToDome (skin::image ("grit-knob-small@2x.png"),
                                      0.4993f, 0.4648f, 0.615f);
    dpad.setSprite (skin::image ("geek-dpad@2x.png"));

    doseKnob.setPlate (true, "dome-small");
    outputKnob.setPlate (true, "dome-small");

    for (auto* b : { (juce::Button*) &tapBtn, (juce::Button*) &printBtn })
        b->setComponentID ("hit");

    brand.setVisible (false);
}

// plategeo values are px on the FULL generated canvas; the window shows only
// the plateCrop region, so map canvas px -> cropped window px.
juce::Rectangle<int> VocalGeekEditor::platePxRect (float x0, float y0, float x1, float y1) const
{
    const float sx = (float) getWidth()  / (float) plateCrop.getWidth();
    const float sy = (float) getHeight() / (float) plateCrop.getHeight();
    return juce::Rectangle<float> ((x0 - (float) plateCrop.getX()) * sx,
                                   (y0 - (float) plateCrop.getY()) * sy,
                                   (x1 - x0) * sx, (y1 - y0) * sy).toNearestInt();
}

void VocalGeekEditor::maskFromOn (juce::Graphics& g, juce::Rectangle<int> rect)
{
    g.drawImage (plateOnScaled,
                 rect.getX(), rect.getY(), rect.getWidth(), rect.getHeight(),
                 rect.getX(), rect.getY(), rect.getWidth(), rect.getHeight());
}

void VocalGeekEditor::maskFromOnFeathered (juce::Graphics& g, juce::Rectangle<int> rect,
                                           int featherPx)
{
    constexpr int n = 4;
    const float s = (float) featherPx / (float) n;
    const auto r = rect.toFloat();

    g.saveState();
    g.reduceClipRegion (r.reduced ((float) featherPx).toNearestInt());
    maskFromOn (g, rect);
    g.restoreState();

    for (int j = 0; j < n; ++j)
    {
        juce::Path band;
        band.addRectangle (r.reduced (s * (float) j));
        band.addRectangle (r.reduced (s * (float) (j + 1)));
        band.setUsingNonZeroWinding (false);
        g.saveState();
        g.reduceClipRegion (band);
        g.setOpacity (((float) j + 0.5f) / (float) n);
        maskFromOn (g, rect);
        g.restoreState();
    }
}

// Reveal a knob's lit neon ring as an annular wedge clipped to the value sweep
// (6 o'clock -> 6 o'clock), with an angular feather on the leading edge and a
// radial fade on the outer bloom so no hard cut ever shows.
void VocalGeekEditor::drawRingWedge (juce::Graphics& g, juce::Slider& s, float cxPx, float cyPx,
                                     float domeRPx, float solidRPx, float maxRPx)
{
    const float sx = (float) getWidth()  / (float) plateCrop.getWidth();
    const float sy = (float) getHeight() / (float) plateCrop.getHeight();
    const juce::Point<float> c ((cxPx - (float) plateCrop.getX()) * sx,
                                (cyPx - (float) plateCrop.getY()) * sy);
    const float domeR  = domeRPx  * sx;
    const float solidR = solidRPx * sx;
    const float R      = maxRPx   * sx;

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
        juce::Path pth;
        pth.addPieSegment (c.x - rOut, c.y - rOut, rOut * 2.0f, rOut * 2.0f, from, to, rIn / rOut);
        g.saveState();
        g.reduceClipRegion (pth);
        g.setOpacity (alpha);
        maskFromOn (g, box);
        g.restoreState();
    };

    const float span = a1 - a0;
    const float fOut = full ? 0.0f : juce::jmin (0.22f, span * 0.40f);
    const float fIn  = full ? 0.0f : juce::jmin (0.10f, span * 0.20f);
    const float aEnd = full ? a0 + juce::MathConstants<float>::twoPi : a1;

    wedge (a0 + fIn, aEnd - fOut, domeR, solidR, 1.0f);
    constexpr int aSteps = 10;
    for (int i = 0; i < aSteps; ++i)
    {
        const float t0 = (float) i / aSteps, t1 = (float) (i + 1) / aSteps;
        wedge (aEnd - fOut * (1.0f - t0), aEnd - fOut * (1.0f - t1),
               domeR, solidR, 1.0f - (t0 + t1) * 0.5f);
        wedge (a0 + fIn * t0, a0 + fIn * t1,
               domeR, solidR, (t0 + t1) * 0.5f);
    }

    constexpr int rSteps = 4;
    for (int i = 0; i < rSteps; ++i)
    {
        const float alpha = 0.85f * (1.0f - ((float) i + 0.5f) / rSteps);
        const float rIn  = solidR + (R - solidR) * (float) i / rSteps;
        const float rOut = solidR + (R - solidR) * (float) (i + 1) / rSteps;
        wedge (a0 + fIn, aEnd - fOut * 0.5f, rIn, rOut, alpha);
        wedge (a0,             a0 + fIn * 0.5f, rIn, rOut, alpha * 0.33f);
        wedge (a0 + fIn * 0.5f, a0 + fIn,       rIn, rOut, alpha * 0.66f);
    }

    juce::ignoreUnused (sy);
}

void VocalGeekEditor::paintPlate (juce::Graphics& g)
{
    using namespace plategeo;

    g.drawImageAt (plateScaled, 0, 0);

    const int feather = juce::roundToInt ((float) getWidth() * 0.008f);

    // ---- header always lit (pinstripes + themed "Geek")
    maskFromOnFeathered (g, platePxRect (headerX0, headerY0, headerX1, headerY1), feather);

    // ---- rec LED breathes with input
    if (proc.engine.inLevel.load() > 0.02f)
        maskFromOnFeathered (g, platePxRect (recX0, recY0, recX1, recY1), feather);

    // ---- tap flash + print latch
    if (juce::Time::currentTimeMillis() < tapFlashUntil)
        maskFromOnFeathered (g, platePxRect (tapX0, tapY0, tapX1, tapY1), feather);
    if (proc.apvts.getRawParameterValue ("print")->load() > 0.5f)
        maskFromOnFeathered (g, platePxRect (prnX0, prnY0, prnX1, prnY1), feather);

    // ---- hit halos under the pads
    if (proc.apvts.getRawParameterValue ("hita")->load() > 0.5f)
        maskFromOnFeathered (g, platePxRect (hitACx - hitGlowR, hitACy - hitGlowR,
                                             hitACx + hitGlowR, hitACy + hitGlowR), feather);
    if (proc.apvts.getRawParameterValue ("hitb")->load() > 0.5f)
        maskFromOnFeathered (g, platePxRect (hitBCx - hitGlowR, hitBCy - hitGlowR,
                                             hitBCx + hitGlowR, hitBCy + hitGlowR), feather);

    // ---- knob neon ring wedges
    drawRingWedge (g, doseKnob.slider,   doseCx, doseCy, ringDomeR, ringSolidR, ringMaxR);
    drawRingWedge (g, outputKnob.slider, outCx,  outCy,  ringDomeR, ringSolidR, ringMaxR);

    // ---- output meter bars light left -> right
    {
        const float lvl = juce::jlimit (0.0f, 1.0f, proc.engine.outLevel.load() * 1.4f);
        const int lit = juce::roundToInt (lvl * 11.0f);
        for (int i = 0; i < lit; ++i)
            maskFromOn (g, platePxRect (meterBars[i][0], meterY0, meterBars[i][1], meterY1));
    }
}

void VocalGeekEditor::layoutPlate()
{
    using namespace plategeo;

    plateScaled   = skin::renderPlate (chassisImg,   plateCrop, getWidth(), getHeight());
    plateOnScaled = skin::renderPlate (chassisOnImg, plateCrop, getWidth(), getHeight());

    const float sx = (float) getWidth() / (float) plateCrop.getWidth();

    auto square = [&] (float cx, float cy, float dia)
    {
        const float side = dia * sx * 1.06f;
        const auto r = platePxRect (cx, cy, cx, cy);
        return juce::Rectangle<float> ((float) r.getX() - side * 0.5f,
                                       (float) r.getY() - side * 0.5f, side, side).toNearestInt();
    };

    doseKnob.setBounds   (square (doseCx, doseCy, knobDomeDia));
    outputKnob.setBounds (square (outCx,  outCy,  knobDomeDia));

    hitA.setBounds (square (hitACx, hitACy, hitDomeDia));
    hitB.setBounds (square (hitBCx, hitBCy, hitDomeDia));
    dpad.setBounds (square (dpadCx, dpadCy, dpadSide));

    bay.setBounds (platePxRect (bayX0, bayY0, bayX1, bayY1));
    display.setBounds (platePxRect (screenX0, screenY0, screenX1, screenY1));

    tapBtn.setBounds   (platePxRect (tapX0, tapY0, tapX1, tapY1));
    printBtn.setBounds (platePxRect (prnX0, prnY0, prnX1, prnY1));

    brand.setBounds (0, 0, 0, 0);
}

//==============================================================================
void VocalGeekEditor::timerCallback()
{
    applyTheme ((int) proc.apvts.getRawParameterValue ("theme")->load());

    GeekDisplay::State st;
    st.theme    = currentTheme;
    st.dose     = proc.apvts.getRawParameterValue ("dose")->load() * 0.01f;
    st.outLevel = proc.engine.outLevel.load();
    st.stutter  = proc.apvts.getRawParameterValue ("hita")->load() > 0.5f
               || proc.apvts.getRawParameterValue ("print")->load() > 0.5f;
    st.tape     = proc.apvts.getRawParameterValue ("hitb")->load() > 0.5f;
    const int rate = (int) proc.apvts.getRawParameterValue ("rate")->load();
    st.rateText = rate == 0 ? "1/4" : rate == 1 ? "1/8" : rate == 2 ? "1/16" : "1/32";
    display.refresh (st);

    if (! plateBaked)
    {
        repaint();
        return;
    }

    // dirty-region repaints
    using namespace plategeo;

    const struct { LabeledKnob* k; float cx, cy; } knobs[] = {
        { &doseKnob, doseCx, doseCy }, { &outputKnob, outCx, outCy } };
    for (size_t i = 0; i < 2; ++i)
    {
        const double v = knobs[i].k->slider.getValue();
        if (v != shownKnob[i])
        {
            shownKnob[i] = v;
            repaint (platePxRect (knobs[i].cx - ringMaxR, knobs[i].cy - ringMaxR,
                                  knobs[i].cx + ringMaxR, knobs[i].cy + ringMaxR).expanded (3));
        }
    }

    const bool rec = proc.engine.inLevel.load() > 0.02f;
    if (rec != shownRec)
    {
        shownRec = rec;
        repaint (platePxRect (recX0, recY0, recX1, recY1).expanded (6));
    }

    const bool tapLit = juce::Time::currentTimeMillis() < tapFlashUntil;
    if (tapLit != shownTapLit)
    {
        shownTapLit = tapLit;
        repaint (platePxRect (tapX0, tapY0, tapX1, tapY1).expanded (8));
    }

    const bool prn = proc.apvts.getRawParameterValue ("print")->load() > 0.5f;
    if (prn != shownPrint)
    {
        shownPrint = prn;
        repaint (platePxRect (prnX0, prnY0, prnX1, prnY1).expanded (8));
    }

    const bool a = proc.apvts.getRawParameterValue ("hita")->load() > 0.5f;
    if (a != shownHitA)
    {
        shownHitA = a;
        repaint (platePxRect (hitACx - hitGlowR, hitACy - hitGlowR,
                              hitACx + hitGlowR, hitACy + hitGlowR).expanded (8));
    }
    const bool bDown = proc.apvts.getRawParameterValue ("hitb")->load() > 0.5f;
    if (bDown != shownHitB)
    {
        shownHitB = bDown;
        repaint (platePxRect (hitBCx - hitGlowR, hitBCy - hitGlowR,
                              hitBCx + hitGlowR, hitBCy + hitGlowR).expanded (8));
    }

    const int lit = juce::roundToInt (juce::jlimit (0.0f, 1.0f,
                        proc.engine.outLevel.load() * 1.4f) * 11.0f);
    if (lit != shownMeter)
    {
        shownMeter = lit;
        repaint (platePxRect (meterBars[0][0], meterY0, meterBars[10][1], meterY1).expanded (3));
    }
}

//==============================================================================
void VocalGeekEditor::paint (juce::Graphics& g)
{
    if (plateBaked)
    {
        paintPlate (g);
        return;
    }

    // ---- vector fallback: clean card with the family look
    theme::backdrop (g, getLocalBounds());

    auto frame = getLocalBounds().toFloat().reduced (8.0f);
    g.setColour (theme::card);
    g.fillRoundedRectangle (frame, 26.0f);
    theme::topHighlight (g, frame, 26.0f);
    g.setColour (theme::cardLine);
    g.drawRoundedRectangle (frame, 26.0f, 1.2f);

    auto wm = theme::font (26.0f, true);
    g.setFont (wm);
    const float vw = juce::GlyphArrangement::getStringWidth (wm, "vocal");
    g.setColour (theme::ink);
    g.drawText ("vocal", 28, 22, 240, 34, juce::Justification::left);
    g.setColour (GeekDisplay::phosphor (juce::jmax (0, currentTheme)));
    g.drawText ("geek", 28 + (int) vw, 22, 240, 34, juce::Justification::left);
}

//==============================================================================
void VocalGeekEditor::resized()
{
    licenseOverlay.setBounds (getLocalBounds());

    if (plateBaked)
    {
        layoutPlate();
        return;
    }

    // ---- vector fallback layout
    brand.setBounds (28, 22, 240, 34);
    display.setBounds (30, 70, getWidth() - 60, 240);
    bay.setBounds (getWidth() / 2 - 90, 322, 180, 64);
    dpad.setBounds (40, 400, 170, 170);
    tapBtn.setBounds (getWidth() / 2 - 45, 420, 90, 34);
    printBtn.setBounds (getWidth() / 2 - 45, 470, 90, 34);
    hitA.setBounds (getWidth() - 190, 440, 84, 84);
    hitB.setBounds (getWidth() - 110, 390, 84, 84);
    doseKnob.setBounds (60, getHeight() - 130, 110, 118);
    outputKnob.setBounds (getWidth() - 170, getHeight() - 130, 110, 118);
}
