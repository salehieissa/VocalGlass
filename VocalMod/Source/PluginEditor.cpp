#include "PluginEditor.h"

namespace
{
    const std::array<juce::String, 3> kModeWords { "CHORUS", "FLANGER", "PHASER" };

    juce::String hzText (float hz)
    {
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " kHz"
                             : juce::String (juce::roundToInt (hz)) + " Hz";
    }

    juce::String signedPct (float v)
    {
        const int i = juce::roundToInt (v);
        return (i > 0 ? "+" : "") + juce::String (i) + "%";
    }
}

//==============================================================================
// Baked-plate geometry, measured on the 2048x1360 chassis plates (ON-vs-OFF
// diff components, radial pink ring profiles). Raw image pixels converted to
// canvas fractions.
namespace plategeo
{
    constexpr float px (float v) { return v / 2048.0f; }
    constexpr float py (float v) { return v / 1360.0f; }

    // hero rings (rate / depth): solid band 134..158, bloom to 170
    constexpr float rateCx = px (520.5f),  rateCy = py (573.7f);
    constexpr float depCx  = px (1508.1f), depCy  = py (571.4f);
    constexpr float bigDomeDia = px (264.0f);
    constexpr float bigDomeR = px (130.0f), bigSolidR = px (159.0f), bigMaxR = px (171.0f);
    constexpr float rateValX0 = px (390.0f),  rateValX1 = px (650.0f);
    constexpr float depValX0  = px (1378.0f), depValX1  = px (1638.0f);
    constexpr float bigValY0 = py (736.0f), bigValY1 = py (792.0f);

    // small rings (feedback / mix / width / tone): solid 58..71, bloom 78
    constexpr float fbCx = px (352.1f),  fbCy = py (1013.4f);
    constexpr float mixCx = px (796.9f), mixCy = py (1013.4f);
    constexpr float widCx = px (1246.8f), widCy = py (1013.5f);
    constexpr float tonCx = px (1694.0f), tonCy = py (1013.5f);
    constexpr float smallDomeDia = px (114.0f);
    constexpr float smallDomeR = px (56.0f), smallSolidR = px (71.0f), smallMaxR = px (79.0f);
    constexpr float smallValY0 = py (1104.0f), smallValY1 = py (1156.0f);
    constexpr float valHalfW = px (120.0f);

    // mode segment pills (CHORUS / FLANGER / PHASER)
    constexpr float modeX0[3] = { px (653.0f), px (915.0f), px (1166.0f) };
    constexpr float modeX1[3] = { px (897.0f), px (1147.0f), px (1383.0f) };
    constexpr float modeY0 = py (275.0f), modeY1 = py (361.0f);

    // SYNC pill + division dropdown pill
    constexpr float syncX0 = px (887.0f), syncX1 = px (1157.0f);
    constexpr float syncY0 = py (515.0f), syncY1 = py (596.0f);
    constexpr float divX0 = px (887.0f), divX1 = px (1157.0f);
    constexpr float divY0 = py (609.0f), divY1 = py (684.0f);
    constexpr float divTxtX0 = px (920.0f), divTxtX1 = px (1100.0f);

    // preset dropdown pill + arrow steppers (top-right)
    constexpr float boxX0 = px (1344.0f), boxX1 = px (1649.0f);
    constexpr float boxY0 = py (136.0f),  boxY1 = py (197.0f);
    constexpr float boxTxtX1 = px (1600.0f);
    constexpr float arrX0 = px (1679.0f), arrMid = px (1766.5f), arrX1 = px (1854.0f);
    constexpr float arrY0 = py (136.0f),  arrY1 = py (203.0f);
}

//==============================================================================
VocalModEditor::VocalModEditor (VocalModProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);

    chassisImg   = skin::image ("mod-chassis@2x.png");
    chassisOnImg = skin::image ("mod-chassis-on@2x.png");
    plateBaked = chassisImg.isValid() && chassisOnImg.isValid();

    // ---- branding ----
    // The wordmark is painted as a two-tone "vocal"+"mod" in paint(); the
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
    addKnob (rateKnob,     "rate",     0);
    addKnob (depthKnob,    "depth",    1);
    addKnob (feedbackKnob, "feedback", 2);
    addKnob (mixKnob,      "mix",      3);
    addKnob (widthKnob,    "width",    4);
    addKnob (toneKnob,     "tone",     5);

    // ---- sync capsule + division dropdown ----
    syncBtn.setClickingTogglesState (true);
    addAndMakeVisible (syncBtn);
    syncAtt = std::make_unique<ButtonAtt> (proc.apvts, "sync", syncBtn);

    divBox.addItemList ({ "1/1", "1/2", "1/4", "1/8", "1/16", "1/4T", "1/8T", "1/4D", "1/8D" }, 1);
    divBox.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (divBox);
    divAtt = std::make_unique<ComboAtt> (proc.apvts, "syncDiv", divBox);

    if (plateBaked)
        setupPlateMode();

    startTimerHz (30);
    setSize (900, 560);

    // License overlay sits on top of everything; it shows itself until activated.
    addChildComponent (licenseOverlay);
    licenseOverlay.setBounds (getLocalBounds());
}

VocalModEditor::~VocalModEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void VocalModEditor::setChoiceParam (const char* id, int value)
{
    if (auto* param = proc.apvts.getParameter (id))
        param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 ((float) value));
}

void VocalModEditor::stepProgram (int delta)
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
void VocalModEditor::setupPlateMode()
{
    laf.plate = true;
    laf.domeLarge = skin::cropToDome (skin::image ("grit-knob-large@2x.png"),
                                      0.1999f, 0.3533f, 0.199f);
    laf.domeSmall = skin::cropToDome (skin::image ("grit-knob-small@2x.png"),
                                      0.4993f, 0.4648f, 0.615f);

    rateKnob.setPlate (true, "dome-large");
    depthKnob.setPlate (true, "dome-large");
    for (auto* k : { &feedbackKnob, &mixKnob, &widthKnob, &toneKnob })
        k->setPlate (true, "dome-small");

    // buttons/combos become invisible hit areas; lit states masked in paintPlate
    for (auto& b : modeBtns) b.setComponentID ("hit");
    syncBtn.setComponentID ("hit");
    prevBtn.setComponentID ("hit");
    nextBtn.setComponentID ("hit");
    presetBox.setComponentID ("hit");
    divBox.setComponentID ("hit");

    brand.setVisible (false);
    brandSub.setVisible (false);
}

juce::Rectangle<int> VocalModEditor::plateFracRect (float fx0, float fy0, float fx1, float fy1) const
{
    const float W = (float) getWidth(), H = (float) getHeight();
    return juce::Rectangle<float> (fx0 * W, fy0 * H, (fx1 - fx0) * W, (fy1 - fy0) * H)
               .toNearestInt();
}

// Blit the matching region of the lit plate over the base plate — pixel
// registration is guaranteed because both images share the same canvas.
void VocalModEditor::maskFromOn (juce::Graphics& g, juce::Rectangle<int> screenRect)
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
void VocalModEditor::maskFromOnFeathered (juce::Graphics& g, juce::Rectangle<int> screenRect,
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
void VocalModEditor::drawRingWedge (juce::Graphics& g, juce::Slider& s, float cxFrac, float cyFrac,
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

void VocalModEditor::paintPlate (juce::Graphics& g)
{
    using namespace plategeo;

    g.drawImage (chassisImg, getLocalBounds().toFloat(),
                 juce::RectanglePlacement::stretchToFit);

    const int feather = juce::roundToInt ((float) getWidth() * 0.008f);

    // ---- selected mode segment lights up
    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    maskFromOnFeathered (g, plateFracRect (modeX0[mode], modeY0, modeX1[mode], modeY1), feather);

    // ---- SYNC capsule
    if (syncBtn.getToggleState())
        maskFromOnFeathered (g, plateFracRect (syncX0, syncY0, syncX1, syncY1), feather);

    // ---- knob neon ring wedges
    drawRingWedge (g, rateKnob.slider,  rateCx, rateCy, bigDomeR, bigSolidR, bigMaxR);
    drawRingWedge (g, depthKnob.slider, depCx,  depCy,  bigDomeR, bigSolidR, bigMaxR);
    drawRingWedge (g, feedbackKnob.slider, fbCx,  fbCy,  smallDomeR, smallSolidR, smallMaxR);
    drawRingWedge (g, mixKnob.slider,      mixCx, mixCy, smallDomeR, smallSolidR, smallMaxR);
    drawRingWedge (g, widthKnob.slider,    widCx, widCy, smallDomeR, smallSolidR, smallMaxR);
    drawRingWedge (g, toneKnob.slider,     tonCx, tonCy, smallDomeR, smallSolidR, smallMaxR);

    // ---- live value read-outs (captions are baked; values are dynamic ink)
    auto value = [&] (const LabeledKnob& k, float fx0, float fy0, float fx1, float fy1, float fontH)
    {
        g.setColour (theme::ink);
        g.setFont (theme::font (fontH, false));
        g.drawText (k.value.getText(), plateFracRect (fx0, fy0, fx1, fy1),
                    juce::Justification::centred);
    };
    value (rateKnob,  rateValX0, bigValY0, rateValX1, bigValY1, 16.0f);
    value (depthKnob, depValX0,  bigValY0, depValX1,  bigValY1, 16.0f);
    value (feedbackKnob, fbCx - valHalfW,  smallValY0, fbCx + valHalfW,  smallValY1, 13.5f);
    value (mixKnob,      mixCx - valHalfW, smallValY0, mixCx + valHalfW, smallValY1, 13.5f);
    value (widthKnob,    widCx - valHalfW, smallValY0, widCx + valHalfW, smallValY1, 13.5f);
    value (toneKnob,     tonCx - valHalfW, smallValY0, tonCx + valHalfW, smallValY1, 13.5f);

    // ---- division text inside its baked pill (accent, dimmed when sync off)
    {
        const bool syncOn = syncBtn.getToggleState();
        g.setColour (theme::accent.withAlpha (syncOn ? 1.0f : 0.35f));
        g.setFont (theme::font (14.0f, false));
        g.drawText (divBox.getText(), plateFracRect (divTxtX0, divY0, divTxtX1, divY1),
                    juce::Justification::centred);
    }

    // ---- preset name inside the dropdown pill
    g.setColour (theme::ink);
    g.setFont (theme::font (14.0f, false));
    g.drawText (presetBox.getText(), plateFracRect (boxX0 + px (30.0f), boxY0, boxTxtX1, boxY1),
                juce::Justification::centred);
}

void VocalModEditor::layoutPlate()
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
    rateKnob.setBounds  (domeSquare (rateCx, rateCy, bigDomeDia));
    depthKnob.setBounds (domeSquare (depCx,  depCy,  bigDomeDia));
    feedbackKnob.setBounds (domeSquare (fbCx,  fbCy,  smallDomeDia));
    mixKnob.setBounds      (domeSquare (mixCx, mixCy, smallDomeDia));
    widthKnob.setBounds    (domeSquare (widCx, widCy, smallDomeDia));
    toneKnob.setBounds     (domeSquare (tonCx, tonCy, smallDomeDia));

    for (int i = 0; i < 3; ++i)
        modeBtns[(size_t) i].setBounds (fr (modeX0[i], modeY0, modeX1[i], modeY1));

    syncBtn.setBounds (fr (syncX0, syncY0, syncX1, syncY1));
    divBox.setBounds  (fr (divX0, divY0, divX1, divY1));

    presetBox.setBounds (fr (boxX0, boxY0, boxX1, boxY1));
    prevBtn.setBounds (fr (arrX0, arrY0, arrMid, arrY1));
    nextBtn.setBounds (fr (arrMid, arrY0, arrX1, arrY1));

    arrowBox = {};
    brand.setBounds (0, 0, 0, 0);
    brandSub.setBounds (0, 0, 0, 0);
}

//==============================================================================
void VocalModEditor::timerCallback()
{
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);

    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    for (int i = 0; i < 3; ++i)
        modeBtns[(size_t) i].setToggleState (i == mode, juce::dontSendNotification);

    // rate read-out: division text when synced, free Hz otherwise
    const bool syncOn = proc.apvts.getRawParameterValue ("sync")->load() > 0.5f;
    divBox.setEnabled (syncOn);
    rateKnob.setValueText (syncOn ? proc.getDivisionText()
                                  : juce::String (proc.apvts.getRawParameterValue ("rate")->load(), 2) + " Hz");

    depthKnob.setValueText (juce::String (juce::roundToInt (proc.apvts.getRawParameterValue ("depth")->load())) + "%");
    feedbackKnob.setValueText (signedPct (proc.apvts.getRawParameterValue ("feedback")->load()));
    mixKnob.setValueText (juce::String (juce::roundToInt (proc.apvts.getRawParameterValue ("mix")->load())) + "%");
    widthKnob.setValueText (juce::String (juce::roundToInt (proc.apvts.getRawParameterValue ("width")->load())) + "%");
    toneKnob.setValueText (hzText (proc.apvts.getRawParameterValue ("tone")->load()));

    if (plateBaked)
        repaint();   // lit masks + ring wedges + value texts live in paintPlate
}

//==============================================================================
void VocalModEditor::paint (juce::Graphics& g)
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

    // two-tone wordmark: ink "vocal" + accent "mod" with an accent underline
    {
        auto wm = theme::font (26.0f, true);
        g.setFont (wm);
        const float vw = juce::GlyphArrangement::getStringWidth (wm, "vocal");
        g.setColour (theme::ink);
        g.drawText ("vocal", 28, 22, 240, 34, juce::Justification::left);
        g.setColour (theme::accent);
        g.drawText ("mod", 28 + (int) vw, 22, 240, 34, juce::Justification::left);
        g.setColour (theme::accent);
        g.fillRoundedRectangle (29.0f, 51.0f, 22.0f, 2.5f, 1.25f);
    }
    theme::spacedText (g, "VOCAL MODULATION", brandSub.getBounds().toFloat(),
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

    // hairline divider between the hero row and the small-knob row
    g.setColour (theme::cardLine);
    g.drawLine (60.0f, 388.0f, 840.0f, 388.0f, 1.2f);
}

//==============================================================================
void VocalModEditor::resized()
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

    // ---- hero row: RATE | sync column | DEPTH ----
    rateKnob.setBounds  (140, 154, 180, 224);
    depthKnob.setBounds (580, 154, 180, 224);

    syncBtn.setBounds (386, 216, 128, 36);
    divBox.setBounds  (386, 260, 128, 34);

    // ---- second row of small knobs ----
    {
        const int w = 120, h = 136, y = 402;
        const int gap = (getWidth() - w * 4) / 5;
        int x = gap;
        for (auto* k : { &feedbackKnob, &mixKnob, &widthKnob, &toneKnob })
        {
            k->setBounds (x, y, w, h);
            x += w + gap;
        }
    }
}
