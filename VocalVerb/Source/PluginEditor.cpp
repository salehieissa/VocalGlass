#include "PluginEditor.h"
#include "../../common/ui/Skin.h"

namespace
{
    juce::String pctStr  (double v) { return juce::String (juce::roundToInt (v)) + "%"; }
    juce::String hzStr   (double v) { return juce::String (juce::roundToInt (v)) + " Hz"; }
    juce::String msStr   (double v) { return juce::String (juce::roundToInt (v)) + " ms"; }
    juce::String dbStr   (double v) { return juce::String (juce::roundToInt (v)) + " dB"; }
    juce::String secStr  (double v) { return juce::String (v, 2) + " s"; }
    juce::String multStr (double v) { return juce::String (v, 2) + "x"; }
    juce::String rateStr (double v) { return juce::String (v, 2) + " Hz"; }
}

//==============================================================================
// Baked-plate geometry, measured on the 2048x1360 chassis plates (ON-vs-OFF
// diff components, radial pink ring profiles). Values are raw image pixels
// converted to canvas fractions at draw time.
namespace plategeo
{
    constexpr float px (float v) { return v / 2048.0f; }
    constexpr float py (float v) { return v / 1360.0f; }

    // knob rings: centre + inner (rIn), solid outer (rSolid) and bloom (rMax)
    // radii of the lit band, in raw px
    struct Ring { float cx, cy, rIn, rSolid, rMax; };
    constexpr Ring mixR      { 204.3f,  460.8f,  33.5f,  53.0f,  66.0f };
    constexpr Ring preR      { 202.7f,  830.9f,  35.0f,  55.0f,  68.0f };
    constexpr Ring decayR    { 519.1f,  659.9f, 133.0f, 165.0f, 178.0f };
    constexpr Ring hiFreqR   { 846.8f,  501.9f,  43.5f,  65.0f,  78.0f };
    constexpr Ring hiShelfR  { 1027.4f, 501.9f,  43.5f,  65.0f,  78.0f };
    constexpr Ring bassFreqR { 847.0f,  861.1f,  43.5f,  65.5f,  78.5f };
    constexpr Ring bassMultR { 1027.8f, 861.5f,  43.5f,  65.5f,  78.5f };
    constexpr Ring sizeR     { 1253.2f, 498.4f,  39.0f,  60.0f,  73.0f };
    constexpr Ring attackR   { 1251.5f, 860.9f,  37.5f,  58.0f,  71.0f };
    constexpr Ring earlyR    { 1460.0f, 498.3f,  36.0f,  57.0f,  70.0f };
    constexpr Ring lateR     { 1460.5f, 861.4f,  36.0f,  57.0f,  70.0f };
    constexpr Ring rateR     { 1655.3f, 482.2f,  33.0f,  52.0f,  65.0f };
    constexpr Ring depthR    { 1655.9f, 879.1f,  33.5f,  53.5f,  66.5f };
    constexpr Ring hiCutR    { 1851.9f, 498.1f,  36.0f,  56.5f,  69.5f };
    constexpr Ring loCutR    { 1852.2f, 865.3f,  34.5f,  55.0f,  68.0f };

    // top-right pills
    constexpr float bypX0 = px (1596.0f), bypX1 = px (1740.0f);
    constexpr float lnkX0 = px (1765.0f), lnkX1 = px (1859.0f);
    constexpr float dotX0 = px (1868.0f), dotX1 = px (1928.0f);
    constexpr float topY0 = py (114.0f),  topY1 = py (184.0f);

    // sync pills
    constexpr float preSyX0 = px (135.0f),  preSyX1 = px (269.0f);
    constexpr float preSyY0 = py (939.0f),  preSyY1 = py (991.0f);
    constexpr float modSyX0 = px (1597.0f), modSyX1 = px (1715.0f);
    constexpr float modSyY0 = py (623.0f),  modSyY1 = py (675.0f);

    // bottom bar: preset arrows + combo rows (captions/chevrons baked)
    constexpr float prvX0 = px (1712.0f), prvX1 = px (1801.0f);
    constexpr float nxtX0 = px (1823.0f), nxtX1 = px (1910.0f);
    constexpr float arrY0 = py (1123.0f), arrY1 = py (1218.0f);
    constexpr float barY0 = py (1135.0f), barY1 = py (1205.0f);
    constexpr float modeBoxX0 = px (290.0f),  modeBoxX1 = px (605.0f);
    constexpr float colBoxX0  = px (790.0f),  colBoxX1  = px (1095.0f);
    constexpr float preBoxX0  = px (1290.0f), preBoxX1  = px (1600.0f);
    constexpr float modeTxtX0 = px (316.0f),  modeTxtX1 = px (560.0f);
    constexpr float colTxtX0  = px (810.0f),  colTxtX1  = px (1050.0f);
    constexpr float preTxtX0  = px (1313.0f), preTxtX1  = px (1550.0f);
}

//==============================================================================
VocalVerbEditor::VocalVerbEditor (VocalVerbProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);

    chassisImg   = skin::image ("verb-chassis@2x.png");
    chassisOnImg = skin::image ("verb-chassis-on@2x.png");
    plateBaked = chassisImg.isValid() && chassisOnImg.isValid();

    // ---- branding ----
    // The wordmark + subtitle are rendered in paint() (two-tone wordmark, spaced
    // subtitle). These labels remain as invisible layout anchors so geometry is
    // unchanged.
    brand.setText ({}, juce::dontSendNotification);
    brand.setFont (theme::font (34.0f, true));
    addAndMakeVisible (brand);

    brandSub.setText ({}, juce::dontSendNotification);
    brandSub.setFont (theme::font (15.0f, false));
    addAndMakeVisible (brandSub);

    // ---- top-right controls ----
    bypassBtn.setClickingTogglesState (true);
    addAndMakeVisible (bypassBtn);
    bypassAtt = std::make_unique<ButtonAtt> (proc.apvts, "bypass", bypassBtn);

    addAndMakeVisible (linkBtn);
    addAndMakeVisible (dotsBtn);
    dotsBtn.onClick = [this]
    {
        juce::PopupMenu m;
        for (int i = 0; i < proc.getNumPrograms(); ++i)
            m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (dotsBtn),
                         [this] (int r) { if (r > 0) proc.setCurrentProgram (r - 1); });
    };

    // ---- knobs + attachments + formatters ----
    attach (mixKnob,      "mix").setFormatter (pctStr);
    attach (predelayKnob, "predelay").setFormatter (msStr);
    attach (decayKnob,    "decay").setFormatter (secStr);
    attach (hiFreqKnob,   "dampHighFreq").setFormatter (hzStr);
    attach (hiShelfKnob,  "dampHighShelf").setFormatter (dbStr);
    attach (bassFreqKnob, "bassFreq").setFormatter (hzStr);
    attach (bassMultKnob, "bassMult").setFormatter (multStr);
    attach (sizeKnob,     "size").setFormatter (pctStr);
    attach (attackKnob,   "attack").setFormatter (pctStr);
    attach (earlyKnob,    "diffEarly").setFormatter (pctStr);
    attach (lateKnob,     "diffLate").setFormatter (pctStr);
    attach (rateKnob,     "modRate").setFormatter (rateStr);
    attach (depthKnob,    "modDepth").setFormatter (pctStr);
    attach (hiCutKnob,    "highCut").setFormatter (hzStr);
    attach (loCutKnob,    "lowCut").setFormatter (hzStr);

    // ---- host-tempo sync toggles + division selectors ----
    auto setupSync = [this] (Bouncy<juce::TextButton>& btn, juce::ComboBox& box,
                             const juce::StringArray& divs,
                             const char* syncId, const char* divId,
                             std::unique_ptr<ButtonAtt>& btnAtt,
                             std::unique_ptr<ComboAtt>& boxAtt)
    {
        btn.setClickingTogglesState (true);
        btn.setColour (juce::TextButton::textColourOffId, theme::inkSoft);
        btn.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        addAndMakeVisible (btn);
        btnAtt = std::make_unique<ButtonAtt> (proc.apvts, syncId, btn);

        box.addItemList (divs, 1);
        box.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (box);
        boxAtt = std::make_unique<ComboAtt> (proc.apvts, divId, box);
    };

    setupSync (modSyncBtn, modDivBox,
               { "1/1", "1/2", "1/4", "1/4.", "1/4T", "1/8", "1/8.", "1/8T", "1/16", "1/16T" },
               "modSync", "modDiv", modSyncAtt, modDivAtt);
    setupSync (preSyncBtn, preDivBox,
               { "1/64", "1/32", "1/16", "1/16.", "1/8T", "1/8", "1/4" },
               "preSync", "preDiv", preSyncAtt, preDivAtt);

    // ---- bottom bar ----
    auto setupLabel = [this] (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (theme::font (15.0f, false));
        l.setColour (juce::Label::textColourId, theme::inkSoft);
        l.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (l);
    };
    setupLabel (modeLabel,   "mode:");
    setupLabel (colorLabel,  "color:");
    setupLabel (presetLabel, "presets:");

    modeBox.addItemList ({ "concert hall", "plate", "room", "chamber", "ambience" }, 1);
    colorBox.addItemList ({ "1970s", "modern", "vintage", "dark", "bright" }, 1);
    for (auto* b : { &modeBox, &colorBox, &presetBox })
    {
        b->setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (*b);
    }
    modeAtt  = std::make_unique<ComboAtt> (proc.apvts, "mode",  modeBox);
    colorAtt = std::make_unique<ComboAtt> (proc.apvts, "color", colorBox);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0) proc.setCurrentProgram (id - 1);
    };

    prevBtn.setClickingTogglesState (false);
    nextBtn.setClickingTogglesState (false);
    prevBtn.onClick = [this] { stepProgram (-1); };
    nextBtn.onClick = [this] { stepProgram ( 1); };
    for (auto* b : { &prevBtn, &nextBtn })
    {
        b->setColour (juce::TextButton::textColourOffId, theme::accent);
        addAndMakeVisible (*b);
    }

    if (plateBaked)
        setupPlateMode();

    startTimerHz (24);
    setSize (1024, 640);

    // License overlay sits on top of everything; it shows itself until activated.
    addChildComponent (licenseOverlay);
    licenseOverlay.setBounds (getLocalBounds());
}

VocalVerbEditor::~VocalVerbEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
Knob& VocalVerbEditor::attach (Knob& k, const char* id)
{
    addAndMakeVisible (k);
    if ((size_t) attIndex < knobAtts.size())
        knobAtts[(size_t) attIndex++] =
            std::make_unique<SliderAtt> (proc.apvts, id, k.getSlider());
    k.refresh();
    return k;
}

void VocalVerbEditor::stepProgram (int delta)
{
    const int n = proc.getNumPrograms();
    if (n <= 0) return;
    const int idx = (proc.getCurrentProgram() + delta + n) % n;
    proc.setCurrentProgram (idx);
}

//==============================================================================
void VocalVerbEditor::timerCallback()
{
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);

    // When synced, show the tempo division in place of the Hz / ms readout.
    const bool modOn = proc.apvts.getRawParameterValue ("modSync")->load() > 0.5f;
    rateKnob.setOverrideText (modOn ? modDivBox.getText() : juce::String());
    modDivBox.setEnabled (modOn);

    const bool preOn = proc.apvts.getRawParameterValue ("preSync")->load() > 0.5f;
    predelayKnob.setOverrideText (preOn ? preDivBox.getText() : juce::String());
    preDivBox.setEnabled (preOn);

    if (plateBaked)
        repaint();   // lit masks + ring wedges + value texts live in paintPlate
}

//==============================================================================
// Baked-plate mode: static art comes from the OFF chassis plate; lit states
// are revealed by blitting the same regions from the pixel-registered ON
// plate. Each knob is a rotating chrome dome sprite sitting inside its ring
// seat; the value arc is an annular wedge revealed from the ON plate and live
// value text sits beneath each ring (below the hero ring, for decay).
void VocalVerbEditor::setupPlateMode()
{
    laf.plate = true;
    laf.domeLarge = skin::cropToDome (skin::image ("grit-knob-large@2x.png"),
                                      0.1999f, 0.3533f, 0.199f);
    laf.domeSmall = skin::cropToDome (skin::image ("grit-knob-small@2x.png"),
                                      0.4993f, 0.4648f, 0.615f);

    decayKnob.setPlate (true, "dome-large");
    for (auto* k : { &mixKnob, &predelayKnob, &hiFreqKnob, &hiShelfKnob,
                     &bassFreqKnob, &bassMultKnob, &sizeKnob, &attackKnob, &earlyKnob,
                     &lateKnob, &rateKnob, &depthKnob, &hiCutKnob, &loCutKnob })
        k->setPlate (true, "dome-small");

    // buttons + combos become invisible hit areas
    bypassBtn.setComponentID ("hit");
    preSyncBtn.setComponentID ("hit");
    modSyncBtn.setComponentID ("hit");
    prevBtn.setComponentID ("hit");
    nextBtn.setComponentID ("hit");
    linkBtn.plate = true;
    dotsBtn.plate = true;
    for (auto* b : { &modeBox, &colorBox, &presetBox, &preDivBox, &modDivBox })
        b->setComponentID ("hit");

    // captions baked into the chassis
    for (auto* l : { &brand, &brandSub, &modeLabel, &colorLabel, &presetLabel })
        l->setVisible (false);
}

juce::Rectangle<int> VocalVerbEditor::plateFracRect (float fx0, float fy0, float fx1, float fy1) const
{
    const float W = (float) getWidth(), H = (float) getHeight();
    return juce::Rectangle<float> (fx0 * W, fy0 * H, (fx1 - fx0) * W, (fy1 - fy0) * H)
               .toNearestInt();
}

// Blit the matching region of the lit plate over the base plate — pixel
// registration is guaranteed because both images share the same canvas.
void VocalVerbEditor::maskFromOn (juce::Graphics& g, juce::Rectangle<int> screenRect)
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
void VocalVerbEditor::maskFromOnFeathered (juce::Graphics& g, juce::Rectangle<int> screenRect,
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
void VocalVerbEditor::drawRingWedge (juce::Graphics& g, juce::Slider& s, float cxFrac, float cyFrac,
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

void VocalVerbEditor::paintPlate (juce::Graphics& g)
{
    using namespace plategeo;

    g.drawImage (chassisImg, getLocalBounds().toFloat(),
                 juce::RectanglePlacement::stretchToFit);

    const int feather = juce::roundToInt ((float) getWidth() * 0.008f);

    // ---- lit pills
    if (bypassBtn.getToggleState())
        maskFromOnFeathered (g, plateFracRect (bypX0, topY0, bypX1, topY1), feather);
    if (preSyncBtn.getToggleState())
        maskFromOnFeathered (g, plateFracRect (preSyX0, preSyY0, preSyX1, preSyY1), feather);
    if (modSyncBtn.getToggleState())
        maskFromOnFeathered (g, plateFracRect (modSyX0, modSyY0, modSyX1, modSyY1), feather);
    if (prevBtn.isDown())
        maskFromOnFeathered (g, plateFracRect (prvX0, arrY0, prvX1, arrY1), feather);
    if (nextBtn.isDown())
        maskFromOnFeathered (g, plateFracRect (nxtX0, arrY0, nxtX1, arrY1), feather);

    // ---- knob ring wedges + live value texts
    struct Item { Knob* k; const Ring* r; };
    const Item items[] = {
        { &mixKnob, &mixR }, { &predelayKnob, &preR }, { &decayKnob, &decayR },
        { &hiFreqKnob, &hiFreqR }, { &hiShelfKnob, &hiShelfR },
        { &bassFreqKnob, &bassFreqR }, { &bassMultKnob, &bassMultR },
        { &sizeKnob, &sizeR }, { &attackKnob, &attackR },
        { &earlyKnob, &earlyR }, { &lateKnob, &lateR },
        { &rateKnob, &rateR }, { &depthKnob, &depthR },
        { &hiCutKnob, &hiCutR }, { &loCutKnob, &loCutR } };

    for (const auto& it : items)
        drawRingWedge (g, it.k->getSlider(), px (it.r->cx), py (it.r->cy),
                       px (it.r->rIn), px (it.r->rSolid), px (it.r->rMax));

    for (const auto& it : items)
    {
        const bool isDecay = (it.k == &decayKnob);
        juce::Rectangle<int> vr;
        if (isDecay)   // bold readout below-left of the hero knob (ref: 462..597 x 992..1029)
            vr = plateFracRect (px (420.0f), py (982.0f), px (640.0f), py (1040.0f));
        else           // value just below the ring
            vr = plateFracRect (px (it.r->cx - 82.0f), py (it.r->cy + it.r->rMax + 4.0f),
                                px (it.r->cx + 82.0f), py (it.r->cy + it.r->rMax + 40.0f));

        g.setColour (theme::ink);
        g.setFont (theme::font (isDecay ? 24.0f : 12.0f, isDecay));
        g.drawText (it.k->getValueText(), vr, juce::Justification::centred);
    }

    // ---- bottom bar selected values, matched to the baked caption baselines
    // (captions "mode:/color:/presets:" sit at y 1152..1172 on the plate; the
    // reference draws the pink values at 1149..1171 starting 316/810/1313)
    g.setColour (theme::accent);
    g.setFont (theme::font (13.5f, true));
    const float valY0 = py (1137.0f), valY1 = py (1183.0f);
    g.drawText (modeBox.getText(),   plateFracRect (modeTxtX0, valY0, modeTxtX1, valY1),
                juce::Justification::centredLeft);
    g.drawText (colorBox.getText(),  plateFracRect (colTxtX0, valY0, colTxtX1, valY1),
                juce::Justification::centredLeft);
    g.drawText (presetBox.getText(), plateFracRect (preTxtX0, valY0, preTxtX1, valY1),
                juce::Justification::centredLeft);
}

void VocalVerbEditor::layoutPlate()
{
    using namespace plategeo;
    auto fr = [this] (float fx0, float fy0, float fx1, float fy1)
    {
        return plateFracRect (fx0, fy0, fx1, fy1);
    };

    // knob squares are dome-sized. Matching the reference, the dome overlaps
    // most of the ring band — its edge lands just inside the band's outer
    // radius so the neon appears to hug the dome (dome edge ≈ 0.93 * rSolid,
    // measured on the reference: decay dome r 153/165, small domes ~60/65)
    auto ringSquare = [&] (Knob& k, const Ring& r)
    {
        const float d = r.rSolid * 0.93f;
        k.setBounds (fr (px (r.cx - d), py (r.cy - d),
                         px (r.cx + d), py (r.cy + d)));
    };
    ringSquare (mixKnob, mixR);           ringSquare (predelayKnob, preR);
    ringSquare (decayKnob, decayR);
    ringSquare (hiFreqKnob, hiFreqR);     ringSquare (hiShelfKnob, hiShelfR);
    ringSquare (bassFreqKnob, bassFreqR); ringSquare (bassMultKnob, bassMultR);
    ringSquare (sizeKnob, sizeR);         ringSquare (attackKnob, attackR);
    ringSquare (earlyKnob, earlyR);       ringSquare (lateKnob, lateR);
    ringSquare (rateKnob, rateR);         ringSquare (depthKnob, depthR);
    ringSquare (hiCutKnob, hiCutR);       ringSquare (loCutKnob, loCutR);

    bypassBtn.setBounds (fr (bypX0, topY0, bypX1, topY1));
    linkBtn.setBounds   (fr (lnkX0, topY0, lnkX1, topY1));
    dotsBtn.setBounds   (fr (dotX0, topY0, dotX1, topY1));

    preSyncBtn.setBounds (fr (preSyX0, preSyY0, preSyX1, preSyY1));
    modSyncBtn.setBounds (fr (modSyX0, modSyY0, modSyX1, modSyY1));

    // division pickers sit over the value read-outs of their knobs
    preDivBox.setBounds (fr (px (preR.cx - 82.0f), py (preR.cy + preR.rMax + 2.0f),
                             px (preR.cx + 82.0f), py (preR.cy + preR.rMax + 42.0f)));
    modDivBox.setBounds (fr (px (rateR.cx - 82.0f), py (rateR.cy + rateR.rMax + 2.0f),
                             px (rateR.cx + 82.0f), py (rateR.cy + rateR.rMax + 42.0f)));

    prevBtn.setBounds (fr (prvX0, arrY0, prvX1, arrY1));
    nextBtn.setBounds (fr (nxtX0, arrY0, nxtX1, arrY1));

    modeBox.setBounds   (fr (modeBoxX0, barY0, modeBoxX1, barY1));
    colorBox.setBounds  (fr (colBoxX0, barY0, colBoxX1, barY1));
    presetBox.setBounds (fr (preBoxX0, barY0, preBoxX1, barY1));

    for (auto* l : { &brand, &brandSub, &modeLabel, &colorLabel, &presetLabel })
        l->setBounds (0, 0, 0, 0);

    leftCard = dampCard = shapeCard = diffCard = modCard = eqCard = {};
    bottomBar = arrowBox = {};
}

//==============================================================================
void VocalVerbEditor::paint (juce::Graphics& g)
{
    if (plateBaked)
    {
        paintPlate (g);
        return;
    }

    // Warm off-white gradient backdrop + soft top light.
    theme::backdrop (g, getLocalBounds());

    // Floating card: layered elevation shadow, near-white fill, top highlight
    // and a crisp hairline border.
    auto card = [&g] (juce::Rectangle<int> r, float radius, const juce::String& title)
    {
        if (r.isEmpty()) return;
        auto rf = r.toFloat();
        theme::elevate (g, rf, radius);
        g.setColour (theme::card);
        g.fillRoundedRectangle (rf, radius);
        theme::topHighlight (g, rf, radius);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (rf, radius, 1.0f);

        if (title.isNotEmpty())
            theme::spacedText (g, title, r.withHeight (40).reduced (8, 6).toFloat(),
                               theme::ink, 11.5f, 2.2f, true, juce::Justification::centred);
    };

    // ---- wordmark: two-tone "vocal" + "verb" with an accent underline tick ----
    {
        auto wm = theme::font (34.0f, true);
        g.setFont (wm);
        const float vw = juce::GlyphArrangement::getStringWidth (wm, "vocal");
        auto wb = brand.getBounds();
        g.setColour (theme::ink);
        g.drawText ("vocal", wb, juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.drawText ("verb", wb.withTrimmedLeft ((int) vw), juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.fillRoundedRectangle ((float) wb.getX(), (float) wb.getBottom() - 5.0f, 24.0f, 2.5f, 1.25f);
    }
    theme::spacedText (g, "VOCAL REVERB", brandSub.getBounds().toFloat(),
                       theme::inkSoft, 9.5f, 2.6f, false, juce::Justification::centredLeft);

    card (leftCard,  16.0f, {});
    card (dampCard,  16.0f, "damping");
    card (shapeCard, 16.0f, "shape");
    card (diffCard,  16.0f, "diff");
    card (modCard,   16.0f, "mod");
    card (eqCard,    16.0f, "eq");

    // hairline divider in the left mix/predelay card
    if (! leftCard.isEmpty())
    {
        g.setColour (theme::cardLine);
        g.drawHorizontalLine (leftDividerY, (float) leftCard.getX() + 12.0f,
                              (float) leftCard.getRight() - 12.0f);
    }

    // bottom bar card
    card (bottomBar, 18.0f, {});

    // arrow box inside the bottom bar: clean white pill, no shadow
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
}

//==============================================================================
void VocalVerbEditor::resized()
{
    licenseOverlay.setBounds (getLocalBounds());

    if (plateBaked)
    {
        layoutPlate();
        return;
    }

    auto area = getLocalBounds();

    // ---- top bar ----
    auto top = area.removeFromTop (104);
    auto topIn = top.reduced (28, 0);
    brand.setBounds (topIn.getX() - 2, 26, 240, 50);
    brandSub.setBounds (brand.getRight() - 6, 41, 160, 24);

    const int bh = 34, by = 42;
    dotsBtn.setBounds (topIn.getRight() - 24, by, 24, bh);
    linkBtn.setBounds (dotsBtn.getX() - 60, by, 50, bh);
    bypassBtn.setBounds (linkBtn.getX() - 96, by, 86, bh);

    // ---- bottom bar ----
    area.removeFromBottom (14);
    auto bottom = area.removeFromBottom (82);
    bottomBar = bottom.reduced (18, 6);

    arrowBox = juce::Rectangle<int> (bottomBar.getRight() - 132, bottomBar.getCentreY() - 22,
                                     112, 44);
    prevBtn.setBounds (arrowBox.getX() + 5, arrowBox.getY() + 4, arrowBox.getWidth() / 2 - 7,
                       arrowBox.getHeight() - 8);
    nextBtn.setBounds (arrowBox.getCentreX() + 2, arrowBox.getY() + 4, arrowBox.getWidth() / 2 - 7,
                       arrowBox.getHeight() - 8);

    // mode / color / presets groups across the remaining width
    auto content = bottomBar.withTrimmedRight (150).reduced (24, 0);
    const int segW = content.getWidth() / 3;
    auto placeGroup = [] (juce::Label& lbl, juce::ComboBox& box, juce::Rectangle<int> seg)
    {
        seg = seg.withSizeKeepingCentre (seg.getWidth(), 32);
        lbl.setBounds (seg.removeFromLeft (78));
        seg.removeFromLeft (6);
        box.setBounds (seg.removeFromLeft (juce::jmin (160, seg.getWidth())));
    };
    placeGroup (modeLabel,   modeBox,   content.removeFromLeft (segW));
    placeGroup (colorLabel,  colorBox,  content.removeFromLeft (segW));
    placeGroup (presetLabel, presetBox, content);

    // ---- body cards ----
    area.removeFromTop (4);
    auto body = area.reduced (18, 8);

    const int gap = 13;
    int x = body.getX();
    auto col = [&] (int w) -> juce::Rectangle<int>
    {
        juce::Rectangle<int> r (x, body.getY(), w, body.getHeight());
        x += w + gap;
        return r;
    };

    leftCard          = col (104);
    auto decayArea    = col (210);
    dampCard          = col (212);
    shapeCard         = col (102);
    diffCard          = col (92);
    modCard           = col (92);
    eqCard            = col (98);

    // left card: mix on top, predelay on bottom (with sync row), divider between
    {
        auto in = leftCard.reduced (8, 14);
        auto topHalf = in.removeFromTop (in.getHeight() / 2);
        leftDividerY = topHalf.getBottom() + 4;
        in.removeFromTop (8);
        mixKnob.setBounds (topHalf);

        // predelay knob + a compact sync row beneath it
        const int syncH = 56;
        auto preArea = in;
        predelayKnob.setBounds (preArea.removeFromTop (juce::jmax (40, preArea.getHeight() - syncH)));
        auto sc = preArea.reduced (2, 2);
        preSyncBtn.setBounds (sc.removeFromTop (24));
        sc.removeFromTop (4);
        preDivBox.setBounds (sc.removeFromTop (24));
    }

    // decay: big knob centred in its column
    decayKnob.setBounds (decayArea.reduced (2, 6));

    // damping: 2x2 grid under the header
    {
        auto in = dampCard.reduced (10);
        in.removeFromTop (34);
        auto rowTop = in.removeFromTop (in.getHeight() / 2);
        auto rowBot = in;
        const int cw = rowTop.getWidth() / 2;
        hiFreqKnob.setBounds  (rowTop.removeFromLeft (cw));
        hiShelfKnob.setBounds (rowTop);
        bassFreqKnob.setBounds (rowBot.removeFromLeft (cw));
        bassMultKnob.setBounds (rowBot);
    }

    auto stackTwo = [] (juce::Rectangle<int> card, Knob& a, Knob& b)
    {
        auto in = card.reduced (8);
        in.removeFromTop (32);
        a.setBounds (in.removeFromTop (in.getHeight() / 2));
        b.setBounds (in);
    };
    stackTwo (shapeCard, sizeKnob,  attackKnob);
    stackTwo (diffCard,  earlyKnob, lateKnob);
    stackTwo (eqCard,    hiCutKnob, loCutKnob);

    // mod card: rate knob, a compact sync row, then depth knob
    {
        auto in = modCard.reduced (8);
        in.removeFromTop (32);
        const int syncH = 56;
        auto rateArea = in.removeFromTop ((in.getHeight() - syncH) / 2);
        auto syncCol  = in.removeFromTop (syncH);
        rateKnob.setBounds (rateArea);
        depthKnob.setBounds (in);

        auto sc = syncCol.reduced (4, 2);
        modSyncBtn.setBounds (sc.removeFromTop (24));
        sc.removeFromTop (4);
        modDivBox.setBounds (sc.removeFromTop (24));
    }
}
