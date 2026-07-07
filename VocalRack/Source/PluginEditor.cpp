#include "PluginEditor.h"
#include "ui/Theme.h"

//==============================================================================
// Baked-plate geometry, measured on the 2336x1744 rack chassis plates
// (ON-vs-OFF diff for power buttons / meter fills, pink radial profiles for
// ring centres and radii, OFF-plate luminance for grooves and caption rows).
namespace plategeo
{
    constexpr float PX = 2336.0f, PY = 1744.0f;
    constexpr float px (float v) { return v / PX; }
    constexpr float py (float v) { return v / PY; }

    // 19 knob ring centres, canvas px, in editor knob order:
    // gateThresh, gateRel, essAmt, essFreq, eqHpf, eqMud, eqPres, eqAir,
    // compAmt, heatDrive, heatTone, airAmt, dlySend, dlyTime, dlyFb,
    // verbSend, verbSize, clipAmt, outGain
    // (least-squares circle fits of the hot ring cores on the ON plate — the
    // old values were ~9px low from asymmetric-bloom bias)
    constexpr float knobCx[19] = {  616.0f,  960.8f,  615.8f,  960.3f,  615.5f,
                                    923.2f, 1214.6f, 1505.4f,  615.6f,  615.4f,
                                    922.7f,  615.2f,  613.6f,  906.7f, 1195.9f,
                                    612.7f,  904.9f,  610.0f, 2039.1f };
    constexpr float knobCy[19] = {  278.4f,  278.4f,  440.5f,  440.7f,  602.1f,
                                    602.3f,  602.3f,  602.2f,  763.1f,  922.2f,
                                    922.3f, 1076.1f, 1223.8f, 1223.8f, 1223.8f,
                                   1375.7f, 1375.8f, 1527.6f, 1419.4f };
    // which module row each knob belongs to (-1 = output card, never dimmed)
    constexpr int knobModule[19] = { 0,0, 1,1, 2,2,2,2, 3, 4,4, 5, 6,6,6, 7,7, 8, -1 };

    // small rings — hot core spans r 32..44, seat groove at r 39-40, dome edge
    // in the reference at r ~35, so the dome (dia 70) reaches the ring groove
    // and the arc band matches the baked core exactly (r 32..44)
    constexpr float smallDomeDia = px (70.0f);
    constexpr float smallDomeR = px (32.0f), smallSolidR = px (44.0f), smallMaxR = px (54.0f);
    // gain ring — hot core r 60..75, dome edge r ~58
    constexpr float gainDomeDia = px (115.0f);
    constexpr float gainDomeR = px (60.0f), gainSolidR = px (75.0f), gainMaxR = px (81.0f);

    // value read-outs sit right of each ring, under the baked caption row
    // (captions occupy cy-33..cy-15; caption column starts at cx+85)
    constexpr float valDx0 = px (85.0f), valDx1 = px (330.0f);
    constexpr float valDy0 = py (-4.0f), valDy1 = py (40.0f);
    // gain value: centred under the baked GAIN caption (pink readout rows in
    // the reference sit at y 1558-1579)
    constexpr float gainValX0 = px (1925.0f), gainValX1 = px (2150.0f);
    constexpr float gainValY0 = py (1535.0f), gainValY1 = py (1600.0f);

    // module power buttons: rect centres from the ON-vs-OFF diff bounds so the
    // reveal covers the button core AND its downward glow skirt
    constexpr float pwrCx = px (187.5f);
    constexpr float pwrCy[9] = { py (284.5f), py (445.0f), py (607.5f),
                                 py (767.5f), py (926.0f), py (1079.5f),
                                 py (1228.0f), py (1378.5f), py (1530.0f) };
    constexpr float pwrHalfW = px (56.0f), pwrHalfH = py (60.0f);

    // slim GR / CLIP pill meters: lit fill bounds from the ON plate
    // (order: gate GR, de-ess GR, comp GR, clip)
    constexpr float pillX0 = px (1594.0f), pillX1 = px (1743.0f);
    constexpr float pillCy[4] = { py (279.0f), py (439.0f), py (761.0f), py (1530.5f) };
    constexpr float pillHalfH = py (12.0f);

    // IN / OUT meter columns (fill bottom-up; lit x 1956-2003 / 2079-2125)
    constexpr float inX0 = px (1954.0f),  inX1 = px (2005.0f);
    constexpr float outX0 = px (2077.0f), outX1 = px (2127.0f);
    constexpr float meterY0 = py (315.0f), meterY1 = py (1255.0f);

    // preset capsule: chevron steppers, text window, save button
    constexpr float capY0 = py (86.0f), capY1 = py (162.0f);
    constexpr float prevX0 = px (1495.0f), prevX1 = px (1595.0f);
    constexpr float nextX0 = px (2005.0f), nextX1 = px (2092.0f);
    constexpr float saveX0 = px (2092.0f), saveX1 = px (2180.0f);

    // module row cards (repaint regions on power toggles)
    constexpr float rowX0 = px (88.0f), rowX1 = px (1790.0f);
    constexpr float rowHalfH = py (74.0f);
}

//==============================================================================
VocalRackEditor::VocalRackEditor (VocalRackProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    chassisImg   = skin::image ("rack-chassis@2x.png");
    chassisOnImg = skin::image ("rack-chassis-on@2x.png");
    plateBaked = chassisImg.isValid() && chassisOnImg.isValid();

    // ---- preset selector: factory programs + user .vepreset files ----
    presetBox.onBeforePopup = [this] { rebuildPresetItems(); };
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id == kImportItemId || id == kExportItemId)
        {
            // action items: put the combo text back, then open the chooser
            presetBox.setText (shownPresetText, juce::dontSendNotification);
            if (id == kImportItemId) importPresetDialog();
            else                     exportPresetDialog();
        }
        else if (id >= kUserPresetBaseId)
        {
            const int idx = id - kUserPresetBaseId;
            if (juce::isPositiveAndBelow (idx, userPresetFiles.size()))
                loadUserPreset (userPresetFiles[idx]);
            shownPresetText = presetBox.getText();
        }
        else if (id > 0)
        {
            shownProgram = id - 1;
            proc.setCurrentProgram (id - 1);
            shownPresetText = presetBox.getText();
        }
    };
    addAndMakeVisible (presetBox);
    rebuildPresetItems();
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    shownProgram = proc.getCurrentProgram();
    shownPresetText = presetBox.getText();

    prevBtn.onClick = [this] { stepProgram (-1); };
    nextBtn.onClick = [this] { stepProgram ( 1); };
    saveBtn.onClick = [this] { promptSavePreset(); };
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);
    addAndMakeVisible (saveBtn);

    // ---- knobs + attachments ----
    configureKnob (gateThreshK, "gateThresh",  gateThreshAtt);
    configureKnob (gateRelK,    "gateRelease", gateRelAtt);
    configureKnob (essAmtK,     "essAmount",   essAmtAtt);
    configureKnob (essFreqK,    "essFreq",     essFreqAtt);
    configureKnob (eqHpfK,      "eqHpf",       eqHpfAtt);
    configureKnob (eqMudK,      "eqMud",       eqMudAtt);
    configureKnob (eqPresK,     "eqPresence",  eqPresAtt);
    configureKnob (eqAirK,      "eqAir",       eqAirAtt);
    configureKnob (compAmtK,    "compAmount",  compAmtAtt);
    configureKnob (heatDriveK,  "heatDrive",   heatDriveAtt);
    configureKnob (heatToneK,   "heatTone",    heatToneAtt);
    configureKnob (airAmtK,     "airAmount",   airAmtAtt);
    configureKnob (dlySendK,    "dlySend",     dlySendAtt);
    configureKnob (dlyTimeK,    "dlyTime",     dlyTimeAtt);
    configureKnob (dlyFbK,      "dlyFeedback", dlyFbAtt);
    configureKnob (verbSendK,   "verbSend",    verbSendAtt);
    configureKnob (verbSizeK,   "verbSize",    verbSizeAtt);
    configureKnob (clipAmtK,    "clipAmount",  clipAmtAtt);
    configureKnob (outK,        "outGain",     outAtt);

    for (auto* v : { &gateThreshV, &gateRelV, &essAmtV, &essFreqV, &eqHpfV, &eqMudV,
                     &eqPresV, &eqAirV, &compAmtV, &heatDriveV, &heatToneV, &airAmtV,
                     &dlySendV, &dlyTimeV, &dlyFbV, &verbSendV, &verbSizeV,
                     &clipAmtV })
        initValue (*v);
    initValue (outV, 14.0f);

    // ---- module power toggles (index order matches the rack rows) ----
    static const char* powerIds[9] = { "gateOn", "essOn", "eqOn", "compOn",
                                       "heatOn", "airOn", "dlyOn", "verbOn", "clipOn" };
    for (int i = 0; i < 9; ++i)
    {
        addAndMakeVisible (powers[(size_t) i]);
        powerAtts[(size_t) i] = std::make_unique<ButtonAtt> (proc.apvts, powerIds[i],
                                                             powers[(size_t) i]);
    }

    // ---- activity meters + IO meters ----
    for (auto* m : { &gateBar, &essBar, &compBar, &clipBar })
        addAndMakeVisible (*m);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    // ---- module row descriptors ----
    modules[0] = { "GATE",   { &gateThreshK, &gateRelK },              { &gateThreshV, &gateRelV },              { "THRESH", "RELEASE" },            &gateBar, "GR",   {} };
    modules[1] = { "DE-ESS", { &essAmtK, &essFreqK },                  { &essAmtV, &essFreqV },                  { "AMOUNT", "FREQ" },               &essBar,  "GR",   {} };
    modules[2] = { "EQ",     { &eqHpfK, &eqMudK, &eqPresK, &eqAirK },  { &eqHpfV, &eqMudV, &eqPresV, &eqAirV },  { "HPF", "MUD", "PRESENCE", "AIR" }, nullptr,  {},    {} };
    modules[3] = { "COMP",   { &compAmtK },                            { &compAmtV },                            { "AMOUNT" },                       &compBar, "GR",   {} };
    modules[4] = { "HEAT",   { &heatDriveK, &heatToneK },              { &heatDriveV, &heatToneV },              { "DRIVE", "TONE" },                nullptr,  {},    {} };
    modules[5] = { "AIR",    { &airAmtK },                             { &airAmtV },                             { "AMOUNT" },                       nullptr,  {},    {} };
    modules[6] = { "DELAY",  { &dlySendK, &dlyTimeK, &dlyFbK },        { &dlySendV, &dlyTimeV, &dlyFbV },        { "SEND", "TIME", "FEEDBACK" },     nullptr,  {},    {} };
    modules[7] = { "REVERB", { &verbSendK, &verbSizeK },               { &verbSendV, &verbSizeV },               { "SEND", "SIZE" },                 nullptr,  {},    {} };
    modules[8] = { "CLIP",   { &clipAmtK },                            { &clipAmtV },                            { "AMOUNT" },                       &clipBar, "CLIP", {} };

    if (plateBaked)
        setupPlateMode();

    startTimerHz (30);
    // plate mode: the window shows ONLY the plate (cropped at the chrome edge),
    // and must match the crop's aspect exactly or circular dome sprites sit in
    // vertically-stretched (elliptical) baked grooves
    if (plateBaked)
        setSize (980, juce::roundToInt (980.0f * (float) plateCrop.getHeight()
                                                / (float) plateCrop.getWidth()));
    else
        setSize (980, 764);

    // License overlay sits on top of everything; it shows itself until activated.
    addChildComponent (licenseOverlay);
    licenseOverlay.setBounds (getLocalBounds());
}

VocalRackEditor::~VocalRackEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void VocalRackEditor::configureKnob (RackKnob& k, const juce::String& id,
                                     std::unique_ptr<SliderAtt>& att)
{
    k.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (k);
    att = std::make_unique<SliderAtt> (proc.apvts, id, k);
}

void VocalRackEditor::initValue (juce::Label& l, float height)
{
    l.setJustificationType (juce::Justification::centredLeft);
    l.setColour (juce::Label::textColourId, theme::accent);
    l.setFont (theme::font (height, true));
    addAndMakeVisible (l);
}

//==============================================================================
// Baked-plate mode: static art comes from the OFF chassis plate; lit states
// (power discs, GR pills, IN/OUT columns, knob rings) are revealed by
// blitting the same regions from the pixel-registered ON plate. Knobs draw
// rotating chrome dome sprites; captions are baked, value read-outs live.
#define VG_RACK_KNOB_LIST \
    RackKnob* ks[19] = { &gateThreshK, &gateRelK, &essAmtK, &essFreqK, \
                         &eqHpfK, &eqMudK, &eqPresK, &eqAirK, &compAmtK, \
                         &heatDriveK, &heatToneK, &airAmtK, \
                         &dlySendK, &dlyTimeK, &dlyFbK, \
                         &verbSendK, &verbSizeK, &clipAmtK, &outK }

#define VG_RACK_VALUE_LIST \
    juce::Label* vls[19] = { &gateThreshV, &gateRelV, &essAmtV, &essFreqV, \
                             &eqHpfV, &eqMudV, &eqPresV, &eqAirV, &compAmtV, \
                             &heatDriveV, &heatToneV, &airAmtV, \
                             &dlySendV, &dlyTimeV, &dlyFbV, \
                             &verbSendV, &verbSizeV, &clipAmtV, &outV }

void VocalRackEditor::setupPlateMode()
{
    lnf.plate = true;
    plateCrop = skin::plateBounds (chassisImg);

    const auto domeLarge = skin::cropToDome (skin::image ("grit-knob-large@2x.png"),
                                             0.1999f, 0.3533f, 0.199f);
    const auto domeSmall = skin::cropToDome (skin::image ("grit-knob-small@2x.png"),
                                             0.4993f, 0.4648f, 0.615f);

    VG_RACK_KNOB_LIST;
    for (int i = 0; i < 18; ++i)
        ks[i]->setPlateSprite (domeSmall, 1.0f / 1.6f);   // hit area 1.6x the dome
    outK.setPlateSprite (domeLarge, 1.0f / 1.25f);

    for (auto& pb : powers)
        pb.plate = true;
    prevBtn.plate = nextBtn.plate = saveBtn.plate = true;

    // recessed grooves / channels are baked; fills are masked from the ON plate
    for (auto* m : { &gateBar, &essBar, &compBar, &clipBar })
        m->setVisible (false);
    inMeter.setVisible (false);
    outMeter.setVisible (false);
}

// plategeo fractions are of the FULL generated canvas; the window shows only
// the plateCrop region, so map full-canvas fraction -> cropped screen px.
juce::Rectangle<int> VocalRackEditor::plateFracRect (float fx0, float fy0, float fx1, float fy1) const
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
void VocalRackEditor::maskFromOn (juce::Graphics& g, juce::Rectangle<int> screenRect)
{
    g.drawImage (plateOnScaled,
                 screenRect.getX(), screenRect.getY(), screenRect.getWidth(), screenRect.getHeight(),
                 screenRect.getX(), screenRect.getY(), screenRect.getWidth(), screenRect.getHeight());
}

// Same reveal but with a soft alpha ramp along the rect border, so the slight
// global tone drift between the two plates never shows as a hard rectangle.
void VocalRackEditor::maskFromOnFeathered (juce::Graphics& g, juce::Rectangle<int> screenRect,
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

// Paint a knob's lit neon ring as a procedural arc clipped to the value sweep
// (6 o'clock -> 6 o'clock). The rack's ON plate bakes each ring frozen at the
// reference render's knob position, so revealing it can never track the value
// — instead the arc is drawn in code: hot-pink solid band in the ring groove,
// radially faded outer bloom, feathered leading edge (never a hard cut).
void VocalRackEditor::drawRingWedge (juce::Graphics& g, juce::Slider& s, float cxFrac, float cyFrac,
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

    auto wedge = [&] (float from, float to, float rIn, float rOut, juce::Colour col)
    {
        if (to - from <= 0.0005f || rOut - rIn <= 0.5f) return;
        juce::Path p;
        p.addPieSegment (c.x - rOut, c.y - rOut, rOut * 2.0f, rOut * 2.0f, from, to, rIn / rOut);
        g.setColour (col);
        g.fillPath (p);
    };

    // both ends of the arc are feathered so there is never a hard radial cut
    const float span = a1 - a0;
    const float fOut = full ? 0.0f : juce::jmin (0.22f, span * 0.40f);
    const float fIn  = full ? 0.0f : juce::jmin (0.10f, span * 0.20f);
    const float aEnd = full ? a0 + juce::MathConstants<float>::twoPi : a1;

    // hot core: accent with a slightly brighter inner half (tube-like)
    const float midR = (domeR + solidR) * 0.5f;
    wedge (a0 + fIn, aEnd - fOut, domeR, solidR, theme::accent);
    wedge (a0 + fIn, aEnd - fOut, domeR, midR,   theme::accentHi.withAlpha (0.55f));

    constexpr int aSteps = 10;
    for (int i = 0; i < aSteps; ++i)
    {
        const float t0 = (float) i / aSteps, t1 = (float) (i + 1) / aSteps;
        wedge (aEnd - fOut * (1.0f - t0), aEnd - fOut * (1.0f - t1),
               domeR, solidR, theme::accent.withAlpha (1.0f - (t0 + t1) * 0.5f));
        wedge (a0 + fIn * t0, a0 + fIn * t1,
               domeR, solidR, theme::accent.withAlpha ((t0 + t1) * 0.5f));
    }

    // outer bloom band, faded radially and softened at both angular ends
    constexpr int rSteps = 4;
    for (int i = 0; i < rSteps; ++i)
    {
        const float alpha = 0.40f * (1.0f - ((float) i + 0.5f) / rSteps);
        const float rIn  = solidR + (R - solidR) * (float) i / rSteps;
        const float rOut = solidR + (R - solidR) * (float) (i + 1) / rSteps;
        wedge (a0 + fIn, aEnd - fOut * 0.5f, rIn, rOut, theme::accent.withAlpha (alpha));
        wedge (a0,             a0 + fIn * 0.5f, rIn, rOut, theme::accent.withAlpha (alpha * 0.33f));
        wedge (a0 + fIn * 0.5f, a0 + fIn,       rIn, rOut, theme::accent.withAlpha (alpha * 0.66f));
    }
}

void VocalRackEditor::paintPlate (juce::Graphics& g)
{
    using namespace plategeo;

    g.drawImageAt (plateScaled, 0, 0);   // cached 1:1 blit — no per-frame rescale

    const int feather = juce::roundToInt ((float) getWidth() * 0.008f);

    // ---- module power discs
    for (int i = 0; i < 9; ++i)
        if (powers[(size_t) i].getToggleState())
            maskFromOnFeathered (g, plateFracRect (pwrCx - pwrHalfW, pwrCy[i] - pwrHalfH,
                                                   pwrCx + pwrHalfW, pwrCy[i] + pwrHalfH),
                                 feather);

    // ---- slim GR / CLIP pill meters (fill left-to-right)
    for (int j = 0; j < 4; ++j)
        if (grFrac[(size_t) j] > 0.01f)
            maskFromOn (g, plateFracRect (pillX0,
                                          pillCy[j] - pillHalfH,
                                          pillX0 + grFrac[(size_t) j] * (pillX1 - pillX0),
                                          pillCy[j] + pillHalfH));

    // ---- IN / OUT columns (fill bottom-up)
    if (ioFrac[0] > 0.004f)
        maskFromOn (g, plateFracRect (inX0, meterY1 - ioFrac[0] * (meterY1 - meterY0),
                                      inX1, meterY1));
    if (ioFrac[1] > 0.004f)
        maskFromOn (g, plateFracRect (outX0, meterY1 - ioFrac[1] * (meterY1 - meterY0),
                                      outX1, meterY1));

    // ---- knob neon ring wedges (a powered-off module keeps its ring dark)
    VG_RACK_KNOB_LIST;
    for (int i = 0; i < 18; ++i)
    {
        const int mod = knobModule[i];
        if (mod >= 0 && ! powers[(size_t) mod].getToggleState())
            continue;
        drawRingWedge (g, *ks[i], px (knobCx[i]), py (knobCy[i]),
                       smallDomeR, smallSolidR, smallMaxR);
    }
    drawRingWedge (g, outK, px (knobCx[18]), py (knobCy[18]),
                   gainDomeR, gainSolidR, gainMaxR);
}

void VocalRackEditor::layoutPlate()
{
    using namespace plategeo;

    // rebuild the scaled plate caches for the new size (1:1 blits per frame)
    plateScaled   = skin::renderPlate (chassisImg,   plateCrop, getWidth(), getHeight());
    plateOnScaled = skin::renderPlate (chassisOnImg, plateCrop, getWidth(), getHeight());

    const float iw = (float) chassisImg.getWidth(), ih = (float) chassisImg.getHeight();
    const float sx = (float) getWidth()  / (float) plateCrop.getWidth();
    const float sy = (float) getHeight() / (float) plateCrop.getHeight();

    // cx/cy in canvas px; hitScale grows the bounds past the drawn dome so
    // tiny knobs stay grabbable (RackKnob's drawFrac shrinks the sprite back)
    auto domeSquare = [&] (float cx, float cy, float diaFrac, float hitScale)
    {
        const float side = diaFrac * iw * sx * 1.06f * hitScale;
        const float pxc = (cx - (float) plateCrop.getX()) * sx;
        const float pyc = (cy - (float) plateCrop.getY()) * sy;
        return juce::Rectangle<float> (pxc - side * 0.5f, pyc - side * 0.5f,
                                       side, side).toNearestInt();
    };

    VG_RACK_KNOB_LIST;
    VG_RACK_VALUE_LIST;
    for (int i = 0; i < 18; ++i)
    {
        ks[i]->setBounds (domeSquare (knobCx[i], knobCy[i], smallDomeDia, 1.6f));
        vls[i]->setBounds (plateFracRect (px (knobCx[i]) + valDx0, py (knobCy[i]) + valDy0,
                                          px (knobCx[i]) + valDx1, py (knobCy[i]) + valDy1));
    }
    outK.setBounds (domeSquare (knobCx[18], knobCy[18], gainDomeDia, 1.25f));
    outV.setBounds (plateFracRect (gainValX0, gainValY0, gainValX1, gainValY1));
    outV.setJustificationType (juce::Justification::centred);

    for (int i = 0; i < 9; ++i)
        powers[(size_t) i].setBounds (plateFracRect (pwrCx - pwrHalfW, pwrCy[i] - pwrHalfH,
                                                     pwrCx + pwrHalfW, pwrCy[i] + pwrHalfH));

    presetBox.setBounds (plateFracRect (prevX1, capY0, nextX0, capY1));
    prevBtn.setBounds (plateFracRect (prevX0, capY0, prevX1, capY1));
    nextBtn.setBounds (plateFracRect (nextX0, capY0, nextX1, capY1));
    saveBtn.setBounds (plateFracRect (saveX0, capY0, saveX1, capY1));

    // repaint regions for the power-toggle dim loop
    for (int r = 0; r < 9; ++r)
        modules[(size_t) r].rect = plateFracRect (rowX0, pwrCy[r] - rowHalfH,
                                                  rowX1, pwrCy[r] + rowHalfH);

    brandArea = {};
    presetPill = {};
    outCard = {};
    inMeterCapRect = outMeterCapRect = {};
}

//==============================================================================
void VocalRackEditor::stepProgram (int delta)
{
    const int n = proc.getNumPrograms();
    if (n <= 0) return;
    const int idx = (proc.getCurrentProgram() + delta + n) % n;
    shownProgram = -1;   // force the combo text back to the factory name
    proc.setCurrentProgram (idx);
}

//==============================================================================
// User presets — plain XML dumps of the APVTS state, purely editor-side.
juce::File VocalRackEditor::presetDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    dir = dir.getChildFile ("Application Support");
   #endif
    return dir.getChildFile ("VocalEssential")
              .getChildFile ("VocalRack")
              .getChildFile ("Presets");
}

void VocalRackEditor::rebuildPresetItems()
{
    userPresetFiles = presetDirectory().findChildFiles (juce::File::findFiles, false,
                                                        "*.vepreset");
    userPresetFiles.sort();

    const auto shownText = presetBox.getText();
    presetBox.clear (juce::dontSendNotification);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);

    if (! userPresetFiles.isEmpty())
    {
        presetBox.addSeparator();
        for (int i = 0; i < userPresetFiles.size(); ++i)
            presetBox.addItem (userPresetFiles[i].getFileNameWithoutExtension(),
                               kUserPresetBaseId + i);
    }

    // sharing: bring a .vepreset in from anywhere / send the current state out
    presetBox.addSeparator();
    presetBox.addItem ("Import Preset...", kImportItemId);
    presetBox.addItem ("Export Preset...", kExportItemId);

    presetBox.setText (shownText, juce::dontSendNotification);
}

void VocalRackEditor::importPresetDialog()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Import VocalRack preset",
        juce::File::getSpecialLocation (juce::File::userDesktopDirectory),
        "*.vepreset");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto src = fc.getResult();
            if (src == juce::File{} || ! src.existsAsFile())
                return;

            // copy into the preset folder so it shows up in the list, then load
            auto dir = presetDirectory();
            dir.createDirectory();
            const auto dest = dir.getChildFile (src.getFileName());
            if (src != dest)
                src.copyFileTo (dest);

            rebuildPresetItems();
            const int idx = userPresetFiles.indexOf (dest);
            if (idx >= 0)
            {
                loadUserPreset (dest);
                presetBox.setSelectedId (kUserPresetBaseId + idx, juce::dontSendNotification);
                shownPresetText = presetBox.getText();
            }
        });
}

void VocalRackEditor::exportPresetDialog()
{
    const auto name = juce::File::createLegalFileName (
        shownPresetText.isNotEmpty() ? shownPresetText : "VocalRack Preset");

    fileChooser = std::make_unique<juce::FileChooser> (
        "Export VocalRack preset",
        juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
            .getChildFile (name + ".vepreset"),
        "*.vepreset");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto dest = fc.getResult();
            if (dest == juce::File{})
                return;
            if (! dest.hasFileExtension ("vepreset"))
                dest = dest.withFileExtension ("vepreset");

            if (auto xml = proc.apvts.copyState().createXml())
                xml->writeTo (dest, {});
        });
}

void VocalRackEditor::promptSavePreset()
{
    auto* w = new juce::AlertWindow ("Save Preset",
                                     "Name your preset:",
                                     juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", presetBox.getText());
    w->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, w] (int result)
        {
            if (result == 1)
            {
                const auto name = w->getTextEditorContents ("name").trim();
                if (name.isNotEmpty())
                    saveUserPreset (name);
            }
        }), true /* delete when dismissed */);

    // Put the caret straight into the name field with everything selected so
    // the user can just type over the suggestion. Deferred: the window only
    // becomes key (and can hand out keyboard focus) after enterModalState has
    // actually shown it.
    if (auto* ed = w->getTextEditor ("name"))
        ed->setSelectAllWhenFocused (true);

    juce::Component::SafePointer<juce::AlertWindow> safe (w);
    juce::Timer::callAfterDelay (250, [safe]
    {
        if (safe != nullptr)
            if (auto* ed = safe->getTextEditor ("name"))
                ed->grabKeyboardFocus();
    });
}

void VocalRackEditor::saveUserPreset (const juce::String& name)
{
    auto dir = presetDirectory();
    dir.createDirectory();   // creates intermediate directories as needed

    const auto file = dir.getChildFile (juce::File::createLegalFileName (name)
                                        + ".vepreset");
    if (auto xml = proc.apvts.copyState().createXml())
        xml->writeTo (file, {});   // overwrites on name collision

    rebuildPresetItems();
    const int idx = userPresetFiles.indexOf (file);
    if (idx >= 0)
        presetBox.setSelectedId (kUserPresetBaseId + idx, juce::dontSendNotification);
    shownPresetText = presetBox.getText();
}

void VocalRackEditor::loadUserPreset (const juce::File& f)
{
    if (auto xml = juce::XmlDocument::parse (f))
        if (xml->hasTagName (proc.apvts.state.getType()))
            proc.apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
void VocalRackEditor::timerCallback()
{
    // ---- IO + activity meters ----
    if (! plateBaked)
    {
        inMeter.setDb  (proc.inDb.load());
        outMeter.setDb (proc.outDb.load());
        gateBar.setDb (proc.gateGrDb.load());
        essBar.setDb  (proc.essGrDb.load());
        compBar.setDb (proc.compGrDb.load());
        clipBar.setDb (proc.clipDb.load());
    }
    else
    {
        using namespace plategeo;

        // pill meters: same ballistics as ActivityBar (fast attack, 20%/tick release)
        const float grDb[4]  = { proc.gateGrDb.load(), proc.essGrDb.load(),
                                 proc.compGrDb.load(), proc.clipDb.load() };
        const float grMax[4] = { 25.0f, 8.0f, 20.0f, 12.0f };
        for (size_t j = 0; j < 4; ++j)
        {
            const float t = juce::jlimit (0.0f, 1.0f, grDb[j] / grMax[j]);
            float next = t > grFrac[j] ? t : grFrac[j] * 0.80f + t * 0.20f;
            if (next < 1.0e-3f) next = 0.0f;
            grFrac[j] = next;
            if (std::abs (next - shownGrFrac[j]) > 0.002f)
            {
                shownGrFrac[j] = next;
                repaint (plateFracRect (pillX0, pillCy[j] - pillHalfH,
                                        pillX1, pillCy[j] + pillHalfH).expanded (2));
            }
        }

        // IN/OUT columns: same mapping as VMeter (-60..0 dBFS, 14%/tick release)
        const float ioDb[2] = { proc.inDb.load(), proc.outDb.load() };
        for (size_t j = 0; j < 2; ++j)
        {
            const float t = juce::jlimit (0.0f, 1.0f, (ioDb[j] + 60.0f) / 60.0f);
            float next = t > ioFrac[j] ? t : ioFrac[j] * 0.86f + t * 0.14f;
            if (next < 1.0e-3f) next = 0.0f;
            ioFrac[j] = next;
            if (std::abs (next - shownIoFrac[j]) > 0.002f)
            {
                shownIoFrac[j] = next;
                const float x0 = j == 0 ? inX0 : outX0, x1 = j == 0 ? inX1 : outX1;
                repaint (plateFracRect (x0, meterY0, x1, meterY1).expanded (2));
            }
        }

        // knob ring wedges + gain: repaint the ring region on value change
        VG_RACK_KNOB_LIST;
        const float ar = (float) chassisImg.getWidth() / (float) chassisImg.getHeight();
        for (size_t i = 0; i < 19; ++i)
        {
            const double v = ks[i]->getValue();
            if (v == shownKnobVals[i])
                continue;
            shownKnobVals[i] = v;
            const float maxR = i == 18 ? gainMaxR : smallMaxR;
            repaint (plateFracRect (px (knobCx[i]) - maxR, py (knobCy[i]) - maxR * ar,
                                    px (knobCx[i]) + maxR, py (knobCy[i]) + maxR * ar)
                         .expanded (2));
        }
    }

    // ---- value read-outs ----
    auto pv = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    auto db1     = [] (float v)  { return juce::String (v, 1) + " dB"; };
    auto db1sign = [] (float v)  { return (v >= 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; };
    auto pct     = [] (float v)  { return juce::String (juce::roundToInt (v)) + "%"; };

    gateThreshV.setText (db1 (pv ("gateThresh")), juce::dontSendNotification);
    gateRelV.setText (juce::String (juce::roundToInt (pv ("gateRelease"))) + " ms",
                      juce::dontSendNotification);
    essAmtV.setText (pct (pv ("essAmount")), juce::dontSendNotification);
    essFreqV.setText (juce::String (pv ("essFreq") / 1000.0f, 1) + " kHz",
                      juce::dontSendNotification);
    eqHpfV.setText (juce::String (juce::roundToInt (pv ("eqHpf"))) + " Hz",
                    juce::dontSendNotification);
    eqMudV.setText (db1 (pv ("eqMud")), juce::dontSendNotification);
    eqPresV.setText (db1sign (pv ("eqPresence")), juce::dontSendNotification);
    eqAirV.setText (db1sign (pv ("eqAir")), juce::dontSendNotification);
    compAmtV.setText (pct (pv ("compAmount")), juce::dontSendNotification);
    heatDriveV.setText (pct (pv ("heatDrive")), juce::dontSendNotification);
    {
        const int tone = juce::roundToInt (pv ("heatTone"));
        heatToneV.setText ((tone > 0 ? "+" : "") + juce::String (tone),
                           juce::dontSendNotification);
    }
    airAmtV.setText (pct (pv ("airAmount")), juce::dontSendNotification);
    dlySendV.setText (pct (pv ("dlySend")), juce::dontSendNotification);
    dlyTimeV.setText (juce::String (juce::roundToInt (pv ("dlyTime"))) + " ms",
                      juce::dontSendNotification);
    dlyFbV.setText (pct (pv ("dlyFeedback")), juce::dontSendNotification);
    verbSendV.setText (pct (pv ("verbSend")), juce::dontSendNotification);
    verbSizeV.setText (juce::String (juce::roundToInt (pv ("verbSize"))),
                       juce::dontSendNotification);
    clipAmtV.setText (pct (pv ("clipAmount")), juce::dontSendNotification);
    outV.setText (db1sign (pv ("outGain")), juce::dontSendNotification);

    // Track host-driven program changes; a loaded user preset keeps its name
    // in the box (the program index doesn't move) until the program changes.
    if (const int prog = proc.getCurrentProgram(); prog != shownProgram)
    {
        shownProgram = prog;
        presetBox.setSelectedId (prog + 1, juce::dontSendNotification);
        shownPresetText = presetBox.getText();
    }

    // ---- dim disabled module rows to ~40% ----
    for (size_t i = 0; i < modules.size(); ++i)
    {
        const bool on = powers[i].getToggleState();
        if (on == shownOn[i])
            continue;
        shownOn[i] = on;

        const float a = on ? 1.0f : 0.4f;
        auto& m = modules[i];
        for (auto* k : m.knobs)  k->setAlpha (a);
        for (auto* v : m.values) v->setAlpha (a);
        if (m.meter) m.meter->setAlpha (a);
        repaint (m.rect.expanded (4));
    }
}

//==============================================================================
void VocalRackEditor::paint (juce::Graphics& g)
{
    if (plateBaked)
    {
        paintPlate (g);
        return;
    }

    theme::backdrop (g, getLocalBounds());

    auto card = [&] (juce::Rectangle<int> r, float radius, bool lifted)
    {
        auto rf = r.toFloat();
        if (lifted)
            theme::elevate (g, rf, radius, 0.8f);
        g.setColour (lifted ? theme::card : theme::card.interpolatedWith (theme::bg, 0.55f));
        g.fillRoundedRectangle (rf, radius);
        if (lifted)
            theme::topHighlight (g, rf, radius);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (rf, radius, 1.0f);
    };

    // ---- header: two-tone wordmark + tagline ----
    {
        auto bb = brandArea;
        auto wm = theme::font (27.0f, true);
        g.setFont (wm);
        const float vw = juce::GlyphArrangement::getStringWidth (wm, "vocal");
        auto wordRow = bb.removeFromTop (32);
        g.setColour (theme::ink);
        g.drawText ("vocal", wordRow, juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.drawText ("rack", wordRow.withTrimmedLeft ((int) vw), juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.fillRoundedRectangle ((float) brandArea.getX(), (float) brandArea.getY() + 33.0f,
                                22.0f, 2.5f, 1.25f);
        theme::spacedText (g, "VOCAL CHAIN", bb.withTrimmedTop (6).toFloat(),
                           theme::inkSoft, 9.5f, 2.6f, false, juce::Justification::left);
    }

    // preset capsule
    card (presetPill, presetPill.getHeight() * 0.5f, true);

    // ---- module rows ----
    for (const auto& m : modules)
    {
        const bool on = ! m.knobs.empty()
                        && powers[(size_t) (&m - modules.data())].getToggleState();
        const float textA = on ? 1.0f : 0.4f;

        card (m.rect, 14.0f, on);

        // module name, small caps on the left
        theme::spacedText (g, m.name,
                           juce::Rectangle<float> ((float) m.rect.getX() + 56.0f,
                                                   (float) m.rect.getCentreY() - 9.0f,
                                                   96.0f, 18.0f),
                           theme::ink.withAlpha (textA), 11.5f, 2.0f, true,
                           juce::Justification::left);

        // per-knob captions (above each value read-out, right of the knob)
        for (size_t i = 0; i < m.knobs.size(); ++i)
        {
            auto kb = m.knobs[i]->getBounds();
            theme::spacedText (g, m.captions[i],
                               juce::Rectangle<float> ((float) kb.getRight() + 2.0f,
                                                       (float) kb.getCentreY() - 16.0f,
                                                       60.0f, 12.0f),
                               theme::inkSoft.withAlpha (textA), 9.0f, 1.5f, false,
                               juce::Justification::left);
        }

        // activity meter caption
        if (m.meter != nullptr)
        {
            auto mb = m.meter->getBounds();
            theme::spacedText (g, m.meterCaption,
                               juce::Rectangle<float> ((float) mb.getX() - 40.0f,
                                                       (float) mb.getCentreY() - 6.0f,
                                                       36.0f, 12.0f),
                               theme::inkFaint.withAlpha (textA), 9.0f, 1.5f, false,
                               juce::Justification::right);
        }
    }

    // ---- output card ----
    card (outCard, 16.0f, true);
    theme::spacedText (g, "OUTPUT",
                       juce::Rectangle<float> ((float) outCard.getX(),
                                               (float) outCard.getY() + 14.0f,
                                               (float) outCard.getWidth(), 16.0f),
                       theme::ink, 11.5f, 2.0f, true, juce::Justification::centred);

    theme::spacedText (g, "IN", inMeterCapRect.toFloat(), theme::inkSoft, 9.0f, 1.5f,
                       false, juce::Justification::centred);
    theme::spacedText (g, "OUT", outMeterCapRect.toFloat(), theme::inkSoft, 9.0f, 1.5f,
                       false, juce::Justification::centred);

    theme::spacedText (g, "GAIN",
                       juce::Rectangle<float> ((float) outK.getX() - 20.0f,
                                               (float) outK.getBottom() + 2.0f,
                                               (float) outK.getWidth() + 40.0f, 12.0f),
                       theme::inkSoft, 9.5f, 1.8f, false, juce::Justification::centred);
}

//==============================================================================
void VocalRackEditor::resized()
{
    licenseOverlay.setBounds (getLocalBounds());

    if (plateBaked)
    {
        layoutPlate();
        return;
    }

    auto content = getLocalBounds().reduced (18);

    // ---- header: wordmark left, preset capsule right ----
    auto headerRow = content.removeFromTop (50);
    brandArea = headerRow.removeFromLeft (240);
    {
        auto pres = headerRow.removeFromRight (290).withSizeKeepingCentre (290, 36);
        presetPill = pres;
        auto inner = pres.reduced (5);
        prevBtn.setBounds (inner.removeFromLeft (26));
        saveBtn.setBounds (inner.removeFromRight (26));
        inner.removeFromRight (2);
        nextBtn.setBounds (inner.removeFromRight (26));
        presetBox.setBounds (inner);
    }
    content.removeFromTop (10);

    // ---- right: output card ----
    outCard = content.removeFromRight (170);
    content.removeFromRight (14);

    {
        auto c = outCard.reduced (16);
        c.removeFromTop (30);                                    // OUTPUT header

        // bottom stack: gain knob + caption + value
        auto valueRow = c.removeFromBottom (22);
        outV.setBounds (valueRow.withSizeKeepingCentre (70, 20));
        outV.setJustificationType (juce::Justification::centred);
        c.removeFromBottom (16);                                 // GAIN caption strip
        auto knobArea = c.removeFromBottom (78);
        outK.setBounds (knobArea.withSizeKeepingCentre (76, 76));
        c.removeFromBottom (8);

        // meters fill what remains: two channels + captions underneath
        auto capRow = c.removeFromBottom (16);
        c.removeFromBottom (4);
        const int mw = 18, gap = 34;
        const int cx = c.getCentreX();
        inMeter.setBounds  (cx - gap / 2 - mw, c.getY() + 4, mw, c.getHeight() - 8);
        outMeter.setBounds (cx + gap / 2,      c.getY() + 4, mw, c.getHeight() - 8);
        inMeterCapRect  = juce::Rectangle<int> (inMeter.getX() - 10,  capRow.getY(), mw + 20, 14);
        outMeterCapRect = juce::Rectangle<int> (outMeter.getX() - 10, capRow.getY(), mw + 20, 14);
    }

    // ---- module rows ----
    const int n = (int) modules.size();
    const int gap = 6;
    const int rowH = (content.getHeight() - gap * (n - 1)) / n;

    for (int r = 0; r < n; ++r)
    {
        auto& m = modules[(size_t) r];
        m.rect = { content.getX(), content.getY() + r * (rowH + gap),
                   content.getWidth(), rowH };
        const int cy = m.rect.getCentreY();

        powers[(size_t) r].setBounds (m.rect.getX() + 14, cy - 14, 28, 28);

        // knob clusters: knob + (caption over value) to its right
        const int knobD    = juce::jmin (56, rowH - 8);
        const int clusterW = 118;
        int x = m.rect.getX() + 156;
        for (size_t i = 0; i < m.knobs.size(); ++i)
        {
            m.knobs[i]->setBounds (x, cy - knobD / 2, knobD, knobD);
            m.values[i]->setBounds (x + knobD + 2, cy - 2, 58, 16);
            x += clusterW;
        }

        if (m.meter != nullptr)
            m.meter->setBounds (m.rect.getRight() - 16 - 64, cy - 4, 64, 8);
    }
}
