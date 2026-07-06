#include "PluginEditor.h"

namespace
{
    const std::array<juce::String, 3> kModeWords { "VOCAL", "EVEN", "BEAT" };

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
// Baked-plate geometry, measured on the 2048x1360 chassis plates (ON-vs-OFF
// diff components, pink radial profiles; 0px registration drift). Raw image
// pixels converted to canvas fractions.
namespace plategeo
{
    constexpr float px (float v) { return v / 2048.0f; }
    constexpr float py (float v) { return v / 1360.0f; }

    // hero rings (blend / glue): pink band 143..170, bloom to 179; the dome
    // (dia 300) overlaps the band's inner half so the neon hugs its edge
    constexpr float blendCx = px (512.6f),  blendCy = py (553.6f);
    constexpr float glueCx  = px (1531.5f), glueCy  = py (553.3f);
    constexpr float bigDomeDia = px (300.0f);
    constexpr float bigDomeR = px (143.0f), bigSolidR = px (170.0f), bigMaxR = px (179.0f);
    constexpr float blendValX0 = px (383.0f),  blendValX1 = px (643.0f);
    constexpr float glueValX0  = px (1402.0f), glueValX1  = px (1662.0f);
    constexpr float bigValY0 = py (742.0f), bigValY1 = py (786.0f);

    // small rings (warmth / air / width / output): band 60..83, bloom to 90
    constexpr float warmCx = px (346.6f),  warmCy = py (1022.8f);
    constexpr float airCx  = px (794.9f),  airCy  = py (1022.8f);
    constexpr float widCx  = px (1239.0f), widCy  = py (1023.1f);
    constexpr float outCx  = px (1684.2f), outCy  = py (1023.0f);
    constexpr float smallDomeDia = px (140.0f);
    constexpr float smallDomeR = px (60.0f), smallSolidR = px (83.0f), smallMaxR = px (90.0f);
    constexpr float smallValY0 = py (1130.0f), smallValY1 = py (1168.0f);
    constexpr float valHalfW = px (120.0f);

    // mode segment pills (VOCAL / EVEN / BEAT) — diff bboxes, seam-to-seam
    constexpr float modeX0[3] = { px (646.0f), px (900.0f), px (1158.0f) };
    constexpr float modeX1[3] = { px (892.0f), px (1148.0f), px (1393.0f) };
    constexpr float modeY0 = py (243.0f), modeY1 = py (337.0f);

    // LIMIT pill + gain-reduction readout (centre column; GR text ref 976..1074 x 633..654)
    constexpr float limX0 = px (875.0f), limX1 = px (1169.0f);
    constexpr float limY0 = py (501.0f), limY1 = py (602.0f);
    constexpr float grX0 = px (880.0f), grX1 = px (1170.0f);
    constexpr float grY0 = py (626.0f), grY1 = py (662.0f);

    // preset dropdown pill + arrow steppers (top-right)
    // pill measured on the plate: x 1327..1732, y 91..181 (cy 136); the text
    // must centre between the pill's left edge and the chevron at x 1653
    constexpr float boxX0 = px (1327.0f), boxX1 = px (1732.0f);
    constexpr float boxY0 = py (94.0f),   boxY1 = py (178.0f);
    constexpr float boxTxtX1 = px (1653.0f);
    constexpr float arrX0 = px (1742.0f), arrMid = px (1846.0f), arrX1 = px (1950.0f);
    constexpr float arrY0 = py (100.0f),  arrY1 = py (180.0f);
}

//==============================================================================
VocalBlendEditor::VocalBlendEditor (VocalBlendProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);

    chassisImg   = skin::image ("blend-chassis@2x.png");
    chassisOnImg = skin::image ("blend-chassis-on@2x.png");
    plateBaked = chassisImg.isValid() && chassisOnImg.isValid();

    // ---- branding ----
    // The wordmark is painted as a two-tone "vocal"+"blend" in paint(); the
    // label keeps its slot in the layout but draws nothing.
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

    // ---- mode segmented pill ----
    for (int i = 0; i < 3; ++i)
    {
        auto& b = modeBtns[(size_t) i];
        b.setButtonText (kModeWords[(size_t) i]);
        b.setClickingTogglesState (false);
        b.onClick = [this, i] { setChoiceParam ("mode", i); };
        addAndMakeVisible (b);
    }

    // ---- knobs + attachments ----
    auto addKnob = [this] (LabeledKnob& k, const char* id, int slot)
    {
        addAndMakeVisible (k);
        knobAtt[(size_t) slot] = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
    };
    addKnob (blendKnob,  "blend",  0);
    addKnob (glueKnob,   "glue",   1);
    addKnob (warmthKnob, "warmth", 2);
    addKnob (airKnob,    "air",    3);
    addKnob (widthKnob,  "width",  4);
    addKnob (outputKnob, "output", 5);

    // ---- LIMIT pill ----
    limitBtn.setClickingTogglesState (true);
    addAndMakeVisible (limitBtn);
    limitAtt = std::make_unique<ButtonAtt> (proc.apvts, "limit", limitBtn);

    if (plateBaked)
        setupPlateMode();

    startTimerHz (30);
    setSize (900, 560);

    // License overlay sits on top of everything; it shows itself until activated.
    addChildComponent (licenseOverlay);
    licenseOverlay.setBounds (getLocalBounds());
}

VocalBlendEditor::~VocalBlendEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void VocalBlendEditor::setChoiceParam (const char* id, int value)
{
    if (auto* param = proc.apvts.getParameter (id))
        param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 ((float) value));
}

void VocalBlendEditor::stepProgram (int delta)
{
    const int n = proc.getNumPrograms();
    if (n <= 0) return;
    const int idx = (proc.getCurrentProgram() + delta + n) % n;
    proc.setCurrentProgram (idx);
}

//==============================================================================
// Baked-plate mode: static art comes from the OFF chassis plate; lit states
// are revealed by blitting the same regions from the pixel-registered ON
// plate. Knobs draw rotating chrome dome sprites; captions are baked, value
// read-outs drawn live.
void VocalBlendEditor::setupPlateMode()
{
    laf.plate = true;
    laf.domeLarge = skin::cropToDome (skin::image ("grit-knob-large@2x.png"),
                                      0.1999f, 0.3533f, 0.199f);
    laf.domeSmall = skin::cropToDome (skin::image ("grit-knob-small@2x.png"),
                                      0.4993f, 0.4648f, 0.615f);

    blendKnob.setPlate (true, "dome-large");
    glueKnob.setPlate (true, "dome-large");
    for (auto* k : { &warmthKnob, &airKnob, &widthKnob, &outputKnob })
        k->setPlate (true, "dome-small");

    // buttons/combos become invisible hit areas; lit states masked in paintPlate
    for (auto& b : modeBtns) b.setComponentID ("hit");
    limitBtn.setComponentID ("hit");
    prevBtn.setComponentID ("hit");
    nextBtn.setComponentID ("hit");
    presetBox.setComponentID ("hit");

    brand.setVisible (false);
    brandSub.setVisible (false);
}

juce::Rectangle<int> VocalBlendEditor::plateFracRect (float fx0, float fy0, float fx1, float fy1) const
{
    const float W = (float) getWidth(), H = (float) getHeight();
    return juce::Rectangle<float> (fx0 * W, fy0 * H, (fx1 - fx0) * W, (fy1 - fy0) * H)
               .toNearestInt();
}

// Blit the matching region of the lit plate over the base plate — pixel
// registration is guaranteed because both images share the same canvas.
void VocalBlendEditor::maskFromOn (juce::Graphics& g, juce::Rectangle<int> screenRect)
{
    const float iw = (float) chassisOnImg.getWidth(), ih = (float) chassisOnImg.getHeight();
    g.drawImage (chassisOnImg,
                 screenRect.getX(), screenRect.getY(), screenRect.getWidth(), screenRect.getHeight(),
                 juce::roundToInt (((float) screenRect.getX()      / (float) getWidth())  * iw),
                 juce::roundToInt (((float) screenRect.getY()      / (float) getHeight()) * ih),
                 juce::roundToInt (((float) screenRect.getWidth()  / (float) getWidth())  * iw),
                 juce::roundToInt (((float) screenRect.getHeight() / (float) getHeight()) * ih));
}

// Same reveal but with a soft alpha ramp along the rect border, so the slight
// global tone drift between the two plates never shows as a hard rectangle.
void VocalBlendEditor::maskFromOnFeathered (juce::Graphics& g, juce::Rectangle<int> screenRect,
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
void VocalBlendEditor::drawRingWedge (juce::Graphics& g, juce::Slider& s, float cxFrac, float cyFrac,
                                      float domeRFrac, float solidRFrac, float maxRFrac)
{
    const float W = (float) getWidth();
    const juce::Point<float> c (cxFrac * W, cyFrac * (float) getHeight());
    const float domeR  = domeRFrac  * W;
    const float solidR = solidRFrac * W;
    const float R      = maxRFrac   * W;

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

    const float feather = full ? 0.0f : juce::jmin (0.22f, (a1 - a0) * 0.5f);
    const float aEnd = full ? a0 + juce::MathConstants<float>::twoPi : a1;

    wedge (a0, aEnd - feather, domeR, solidR, 1.0f);
    constexpr int aSteps = 10;
    for (int i = 0; i < aSteps; ++i)
        wedge (aEnd - feather * (1.0f - (float) i / aSteps),
               aEnd - feather * (1.0f - (float) (i + 1) / aSteps),
               domeR, solidR,
               1.0f - ((float) i + 0.5f) / aSteps);

    const float sf = full ? 0.0f : juce::jmin (0.14f, (a1 - a0) * 0.25f);
    constexpr int rSteps = 4;
    for (int i = 0; i < rSteps; ++i)
    {
        const float alpha = 0.85f * (1.0f - ((float) i + 0.5f) / rSteps);
        const float rIn  = solidR + (R - solidR) * (float) i / rSteps;
        const float rOut = solidR + (R - solidR) * (float) (i + 1) / rSteps;
        wedge (a0 + sf, aEnd - feather * 0.5f, rIn, rOut, alpha);
        wedge (a0,             a0 + sf * 0.5f, rIn, rOut, alpha * 0.33f);
        wedge (a0 + sf * 0.5f, a0 + sf,        rIn, rOut, alpha * 0.66f);
    }
}

void VocalBlendEditor::paintPlate (juce::Graphics& g)
{
    using namespace plategeo;

    g.drawImage (chassisImg, getLocalBounds().toFloat(),
                 juce::RectanglePlacement::stretchToFit);

    const int feather = juce::roundToInt ((float) getWidth() * 0.008f);

    // ---- selected mode segment lights up
    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    maskFromOnFeathered (g, plateFracRect (modeX0[mode], modeY0, modeX1[mode], modeY1), feather);

    // ---- LIMIT capsule
    if (limitBtn.getToggleState())
        maskFromOnFeathered (g, plateFracRect (limX0, limY0, limX1, limY1), feather);

    // ---- knob neon ring wedges
    drawRingWedge (g, blendKnob.slider, blendCx, blendCy, bigDomeR, bigSolidR, bigMaxR);
    drawRingWedge (g, glueKnob.slider,  glueCx,  glueCy,  bigDomeR, bigSolidR, bigMaxR);
    drawRingWedge (g, warmthKnob.slider, warmCx, warmCy, smallDomeR, smallSolidR, smallMaxR);
    drawRingWedge (g, airKnob.slider,    airCx,  airCy,  smallDomeR, smallSolidR, smallMaxR);
    drawRingWedge (g, widthKnob.slider,  widCx,  widCy,  smallDomeR, smallSolidR, smallMaxR);
    drawRingWedge (g, outputKnob.slider, outCx,  outCy,  smallDomeR, smallSolidR, smallMaxR);

    // ---- live value read-outs (captions are baked; values are dynamic ink)
    auto value = [&] (const LabeledKnob& k, float fx0, float fy0, float fx1, float fy1, float fontH)
    {
        g.setColour (theme::ink);
        g.setFont (theme::font (fontH, false));
        g.drawText (k.value.getText(), plateFracRect (fx0, fy0, fx1, fy1),
                    juce::Justification::centred);
    };
    value (blendKnob, blendValX0, bigValY0, blendValX1, bigValY1, 16.0f);
    value (glueKnob,  glueValX0,  bigValY0, glueValX1,  bigValY1, 16.0f);
    value (warmthKnob, warmCx - valHalfW, smallValY0, warmCx + valHalfW, smallValY1, 13.5f);
    value (airKnob,    airCx - valHalfW,  smallValY0, airCx + valHalfW,  smallValY1, 13.5f);
    value (widthKnob,  widCx - valHalfW,  smallValY0, widCx + valHalfW,  smallValY1, 13.5f);
    value (outputKnob, outCx - valHalfW,  smallValY0, outCx + valHalfW,  smallValY1, 13.5f);

    // ---- glue gain-reduction readout (accent, under the LIMIT pill)
    {
        g.setColour (theme::accent.withAlpha (shownGr > 0.05f ? 1.0f : 0.35f));
        g.setFont (theme::font (14.0f, false));
        g.drawText ("-" + juce::String (shownGr, 1) + " dB",
                    plateFracRect (grX0, grY0, grX1, grY1),
                    juce::Justification::centred);
    }

    // ---- preset name inside the dropdown pill
    g.setColour (theme::ink);
    g.setFont (theme::font (14.0f, false));
    g.drawText (presetBox.getText(), plateFracRect (boxX0 + px (30.0f), boxY0, boxTxtX1, boxY1),
                juce::Justification::centred);
}

void VocalBlendEditor::layoutPlate()
{
    using namespace plategeo;
    auto fr = [this] (float fx0, float fy0, float fx1, float fy1)
    {
        return plateFracRect (fx0, fy0, fx1, fy1);
    };
    const float W = (float) getWidth(), H = (float) getHeight();

    auto domeSquare = [&] (float cx, float cy, float diaFrac)
    {
        const float side = diaFrac * W * 1.06f;
        return juce::Rectangle<float> (cx * W - side * 0.5f, cy * H - side * 0.5f,
                                       side, side).toNearestInt();
    };
    blendKnob.setBounds (domeSquare (blendCx, blendCy, bigDomeDia));
    glueKnob.setBounds  (domeSquare (glueCx,  glueCy,  bigDomeDia));
    warmthKnob.setBounds (domeSquare (warmCx, warmCy, smallDomeDia));
    airKnob.setBounds    (domeSquare (airCx,  airCy,  smallDomeDia));
    widthKnob.setBounds  (domeSquare (widCx,  widCy,  smallDomeDia));
    outputKnob.setBounds (domeSquare (outCx,  outCy,  smallDomeDia));

    for (int i = 0; i < 3; ++i)
        modeBtns[(size_t) i].setBounds (fr (modeX0[i], modeY0, modeX1[i], modeY1));

    limitBtn.setBounds (fr (limX0, limY0, limX1, limY1));

    presetBox.setBounds (fr (boxX0, boxY0, boxX1, boxY1));
    prevBtn.setBounds (fr (arrX0, arrY0, arrMid, arrY1));
    nextBtn.setBounds (fr (arrMid, arrY0, arrX1, arrY1));

    arrowBox = {};
    brand.setBounds (0, 0, 0, 0);
    brandSub.setBounds (0, 0, 0, 0);
}

//==============================================================================
void VocalBlendEditor::timerCallback()
{
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);

    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    for (int i = 0; i < 3; ++i)
        modeBtns[(size_t) i].setToggleState (i == mode, juce::dontSendNotification);

    blendKnob.setValueText (pctText (proc.apvts.getRawParameterValue ("blend")->load()));
    glueKnob.setValueText (pctText (proc.apvts.getRawParameterValue ("glue")->load()));
    warmthKnob.setValueText (pctText (proc.apvts.getRawParameterValue ("warmth")->load()));
    airKnob.setValueText (pctText (proc.apvts.getRawParameterValue ("air")->load()));
    widthKnob.setValueText (pctText (proc.apvts.getRawParameterValue ("width")->load()));
    outputKnob.setValueText (dbText (proc.apvts.getRawParameterValue ("output")->load()));

    shownGr = proc.getGainReductionDb();

    repaint();   // GR readout + (in plate mode) lit masks, wedges, value texts
}

//==============================================================================
void VocalBlendEditor::paint (juce::Graphics& g)
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

    // two-tone wordmark: ink "vocal" + accent "blend" with an accent underline
    {
        auto wm = theme::font (26.0f, true);
        g.setFont (wm);
        const float vw = juce::GlyphArrangement::getStringWidth (wm, "vocal");
        g.setColour (theme::ink);
        g.drawText ("vocal", 28, 22, 240, 34, juce::Justification::left);
        g.setColour (theme::accent);
        g.drawText ("blend", 28 + (int) vw, 22, 240, 34, juce::Justification::left);
        g.setColour (theme::accent);
        g.fillRoundedRectangle (29.0f, 51.0f, 22.0f, 2.5f, 1.25f);
    }
    theme::spacedText (g, "MASTER VOCAL GLUE", brandSub.getBounds().toFloat(),
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

    // glue gain-reduction readout under the LIMIT pill
    {
        auto r = juce::Rectangle<int> (386, 260, 128, 30);
        g.setColour (theme::accent.withAlpha (shownGr > 0.05f ? 1.0f : 0.35f));
        g.setFont (theme::font (14.0f, false));
        g.drawText ("-" + juce::String (shownGr, 1) + " dB", r, juce::Justification::centred);
    }

    // hairline divider between the hero row and the small-knob row
    g.setColour (theme::cardLine);
    g.drawLine (60.0f, 388.0f, 840.0f, 388.0f, 1.2f);
}

//==============================================================================
void VocalBlendEditor::resized()
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

    // ---- mode segmented pill (top-centre) ----
    {
        const int w = 112, h = 36, gap = 6;
        int x = (getWidth() - (w * 3 + gap * 2)) / 2;
        for (int i = 0; i < 3; ++i)
        {
            modeBtns[(size_t) i].setBounds (x, 100, w, h);
            x += w + gap;
        }
    }

    // ---- hero row: BLEND | limit column | GLUE ----
    blendKnob.setBounds (140, 154, 180, 224);
    glueKnob.setBounds  (580, 154, 180, 224);

    limitBtn.setBounds (386, 216, 128, 36);

    // ---- second row of small knobs ----
    {
        const int w = 120, h = 136, y = 402;
        const int gap = (getWidth() - w * 4) / 5;
        int x = gap;
        for (auto* k : { &warmthKnob, &airKnob, &widthKnob, &outputKnob })
        {
            k->setBounds (x, y, w, h);
            x += w + gap;
        }
    }
}
