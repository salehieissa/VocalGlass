#include "PluginEditor.h"
#include "ui/Theme.h"

//==============================================================================
// Baked-plate geometry, measured on the 2048x1360 chassis plates (ON-vs-OFF
// diff components, radial pink ring profiles, white-pill threshold scans).
// Values are raw image pixels converted to canvas fractions.
namespace plategeo
{
    constexpr float px (float v) { return v / 2048.0f; }
    constexpr float py (float v) { return v / 1360.0f; }

    // GR meter groove (lit bar bbox from the ON plate) + value pill
    constexpr float barX0 = px (228.0f), barX1 = px (277.0f);
    constexpr float barY0 = py (319.0f), barY1 = py (1063.0f);
    constexpr float grValX0 = px (190.0f), grValX1 = px (391.0f);
    constexpr float grValY0 = py (1101.0f), grValY1 = py (1151.0f);

    // hero threshold ring: solid band 168..202, bloom to 212
    constexpr float thrCx = px (866.0f), thrCy = py (483.0f);
    constexpr float thrDomeDia = px (332.0f);
    constexpr float thrDomeR = px (164.0f), thrSolidR = px (203.0f), thrMaxR = px (213.0f);
    constexpr float thrValX0 = px (753.0f), thrValX1 = px (993.0f);
    constexpr float thrValY0 = py (753.0f), thrValY1 = py (806.0f);

    // range ring: solid 80..98, bloom 108
    constexpr float rngCx = px (866.0f), rngCy = py (967.0f);
    constexpr float rngDomeDia = px (156.0f);
    constexpr float rngDomeR = px (77.0f), rngSolidR = px (99.0f), rngMaxR = px (109.0f);
    constexpr float rngValX0 = px (767.0f), rngValX1 = px (962.0f);
    constexpr float rngValY0 = py (1105.0f), rngValY1 = py (1155.0f);

    // envelope 2x2 rings: solid 52..62, bloom 70
    constexpr float atkCx = px (1396.9f), atkCy = py (396.3f);
    constexpr float hldCx = px (1710.7f), hldCy = py (398.6f);
    constexpr float relCx = px (1396.9f), relCy = py (717.7f);
    constexpr float hysCx = px (1710.5f), hysCy = py (718.8f);
    constexpr float envDomeDia = px (100.0f);
    constexpr float envDomeR = px (50.0f), envSolidR = px (62.0f), envMaxR = px (70.0f);
    constexpr float atkValX0 = px (1314.0f), atkValX1 = px (1488.0f);
    constexpr float hldValX0 = px (1631.0f), hldValX1 = px (1795.0f);
    constexpr float topValY0 = py (541.0f),  topValY1 = py (588.0f);
    constexpr float relValX0 = px (1314.0f), relValX1 = px (1488.0f);
    constexpr float hysValX0 = px (1630.0f), hysValX1 = px (1804.0f);
    constexpr float botValY0 = py (864.0f),  botValY1 = py (914.0f);

    // SC HPF ring: solid 26..33, bloom 39
    constexpr float hpfCx = px (1394.4f), hpfCy = py (1011.3f);
    constexpr float hpfDomeDia = px (50.0f);
    constexpr float hpfDomeR = px (24.0f), hpfSolidR = px (33.0f), hpfMaxR = px (39.0f);
    constexpr float hpfValX0 = px (1314.0f), hpfValX1 = px (1483.0f);
    constexpr float hpfValY0 = py (1103.0f), hpfValY1 = py (1150.0f);

    // SC LISTEN capsule (lit bbox)
    constexpr float scX0 = px (1586.0f), scX1 = px (1831.0f);
    constexpr float scY0 = py (1015.0f), scY1 = py (1076.0f);

    // preset pill: circled arrow steppers + name area
    constexpr float prevX0 = px (1355.0f), prevX1 = px (1428.0f);
    constexpr float nextX0 = px (1797.0f), nextX1 = px (1870.0f);
    constexpr float stepY0 = py (99.0f),   stepY1 = py (171.0f);
    constexpr float nameX0 = px (1440.0f), nameX1 = px (1790.0f);
    constexpr float nameY0 = py (100.0f),  nameY1 = py (170.0f);
}

//==============================================================================
VocalGateEditor::VocalGateEditor (VocalGateProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    chassisImg   = skin::image ("gate-chassis@2x.png");
    chassisOnImg = skin::image ("gate-chassis-on@2x.png");
    plateBaked = chassisImg.isValid() && chassisOnImg.isValid();

    // ---- branding ----
    // The wordmark + tagline are painted directly (two-tone + letter-spaced) in
    // paint(); these labels are kept (laid out in resized) purely to anchor the
    // painted text geometry, so they stay hidden.
    brand.setText ("vocalgate", juce::dontSendNotification);
    brand.setFont (theme::font (26.0f, true));
    brand.setColour (juce::Label::textColourId, theme::ink);
    addChildComponent (brand);

    brandSub.setText ("VOCAL GATE", juce::dontSendNotification);
    brandSub.setFont (theme::font (11.0f, false));
    brandSub.setColour (juce::Label::textColourId, theme::inkSoft);
    addChildComponent (brandSub);

    // ---- preset selector ----
    presetName.setJustificationType (juce::Justification::centred);
    presetName.setFont (theme::font (15.0f, false));
    presetName.setColour (juce::Label::textColourId, theme::ink);
    addAndMakeVisible (presetName);

    prevBtn.onClick = [this] { stepProgram (-1); };
    nextBtn.onClick = [this] { stepProgram ( 1); };
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);

    // ---- gain-reduction meter ----
    addAndMakeVisible (grMeter);
    initValue (grValue, 14.0f);

    // ---- knobs + attachments ----
    configureKnob (threshKnob,  "threshold",  threshAtt);
    configureKnob (rangeKnob,   "range",      rangeAtt);
    configureKnob (attackKnob,  "attack",     attackAtt);
    configureKnob (holdKnob,    "hold",       holdAtt);
    configureKnob (releaseKnob, "release",    releaseAtt);
    configureKnob (hystKnob,    "hysteresis", hystAtt);
    configureKnob (hpfKnob,     "scHpf",      hpfAtt);

    initValue (threshValue, 16.0f);
    initValue (rangeValue, 14.0f);
    initValue (attackValue);
    initValue (holdValue);
    initValue (releaseValue);
    initValue (hystValue);
    initValue (hpfValue);

    // ---- SC listen capsule toggle ----
    scListenBtn.setClickingTogglesState (true);
    addAndMakeVisible (scListenBtn);
    scListenAtt = std::make_unique<ButtonAtt> (proc.apvts, "scListen", scListenBtn);

    if (plateBaked)
        setupPlateMode();

    startTimerHz (30);
    setSize (900, 560);

    // License overlay sits on top of everything; it shows itself until activated.
    addChildComponent (licenseOverlay);
    licenseOverlay.setBounds (getLocalBounds());
}

VocalGateEditor::~VocalGateEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void VocalGateEditor::configureKnob (GateKnob& k, const juce::String& id,
                                     std::unique_ptr<SliderAtt>& att)
{
    k.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (k);
    att = std::make_unique<SliderAtt> (proc.apvts, id, k);
}

void VocalGateEditor::initValue (juce::Label& l, float height)
{
    l.setJustificationType (juce::Justification::centred);
    l.setColour (juce::Label::textColourId, theme::accent);
    l.setFont (theme::font (height, true));
    addAndMakeVisible (l);
}

//==============================================================================
void VocalGateEditor::stepProgram (int delta)
{
    const int n = proc.getNumPrograms();
    if (n <= 0) return;
    const int idx = (proc.getCurrentProgram() + delta + n) % n;
    proc.setCurrentProgram (idx);
}

//==============================================================================
// Baked-plate mode: static art comes from the OFF chassis plate; lit states
// are revealed by blitting the same regions from the pixel-registered ON
// plate. Knobs draw rotating chrome dome sprites; captions are baked; value
// read-outs stay as live labels positioned over the baked pills.
void VocalGateEditor::setupPlateMode()
{
    lnf.plate = true;

    const auto domeLarge = skin::cropToDome (skin::image ("grit-knob-large@2x.png"),
                                             0.1999f, 0.3533f, 0.199f);
    const auto domeSmall = skin::cropToDome (skin::image ("grit-knob-small@2x.png"),
                                             0.4993f, 0.4648f, 0.615f);

    threshKnob.setPlateSprite (domeLarge);
    rangeKnob.setPlateSprite (domeLarge);
    for (auto* k : { &attackKnob, &holdKnob, &releaseKnob, &hystKnob, &hpfKnob })
        k->setPlateSprite (domeSmall);

    grMeter.plate = true;
    prevBtn.plate = true;
    nextBtn.plate = true;
    scListenBtn.setComponentID ("hit");
}

juce::Rectangle<int> VocalGateEditor::plateFracRect (float fx0, float fy0, float fx1, float fy1) const
{
    const float W = (float) getWidth(), H = (float) getHeight();
    return juce::Rectangle<float> (fx0 * W, fy0 * H, (fx1 - fx0) * W, (fy1 - fy0) * H)
               .toNearestInt();
}

// Blit the matching region of the lit plate over the base plate — pixel
// registration is guaranteed because both images share the same canvas.
void VocalGateEditor::maskFromOn (juce::Graphics& g, juce::Rectangle<int> screenRect)
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
void VocalGateEditor::maskFromOnFeathered (juce::Graphics& g, juce::Rectangle<int> screenRect,
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
void VocalGateEditor::drawRingWedge (juce::Graphics& g, juce::Slider& s, float cxFrac, float cyFrac,
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

void VocalGateEditor::paintPlate (juce::Graphics& g)
{
    using namespace plategeo;

    g.drawImage (chassisImg, getLocalBounds().toFloat(),
                 juce::RectanglePlacement::stretchToFit);

    const int feather = juce::roundToInt ((float) getWidth() * 0.008f);

    // ---- GR meter: reveal the lit groove from the top down
    {
        const float frac = grMeter.getFraction();
        if (frac > 0.004f)
        {
            auto bar = plateFracRect (barX0, barY0, barX1, barY1);
            g.saveState();
            g.reduceClipRegion (bar.withHeight (juce::roundToInt ((float) bar.getHeight() * frac)));
            maskFromOn (g, bar);
            g.restoreState();
        }
    }

    // ---- SC LISTEN capsule lights up
    if (scListenBtn.getToggleState())
        maskFromOnFeathered (g, plateFracRect (scX0, scY0, scX1, scY1), feather);

    // ---- knob neon ring wedges
    drawRingWedge (g, threshKnob, thrCx, thrCy, thrDomeR, thrSolidR, thrMaxR);
    drawRingWedge (g, rangeKnob,  rngCx, rngCy, rngDomeR, rngSolidR, rngMaxR);
    drawRingWedge (g, attackKnob,  atkCx, atkCy, envDomeR, envSolidR, envMaxR);
    drawRingWedge (g, holdKnob,    hldCx, hldCy, envDomeR, envSolidR, envMaxR);
    drawRingWedge (g, releaseKnob, relCx, relCy, envDomeR, envSolidR, envMaxR);
    drawRingWedge (g, hystKnob,    hysCx, hysCy, envDomeR, envSolidR, envMaxR);
    drawRingWedge (g, hpfKnob,     hpfCx, hpfCy, hpfDomeR, hpfSolidR, hpfMaxR);
}

void VocalGateEditor::layoutPlate()
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
    threshKnob.setBounds  (domeSquare (thrCx, thrCy, thrDomeDia));
    rangeKnob.setBounds   (domeSquare (rngCx, rngCy, rngDomeDia));
    attackKnob.setBounds  (domeSquare (atkCx, atkCy, envDomeDia));
    holdKnob.setBounds    (domeSquare (hldCx, hldCy, envDomeDia));
    releaseKnob.setBounds (domeSquare (relCx, relCy, envDomeDia));
    hystKnob.setBounds    (domeSquare (hysCx, hysCy, envDomeDia));
    hpfKnob.setBounds     (domeSquare (hpfCx, hpfCy, hpfDomeDia));

    grMeter.setBounds (fr (barX0, barY0, barX1, barY1));
    scListenBtn.setBounds (fr (scX0, scY0, scX1, scY1));

    prevBtn.setBounds (fr (prevX0, stepY0, prevX1, stepY1));
    nextBtn.setBounds (fr (nextX0, stepY0, nextX1, stepY1));
    presetName.setBounds (fr (nameX0, nameY0, nameX1, nameY1));

    // live value labels sit over the baked white pills
    grValue.setBounds     (fr (grValX0, grValY0, grValX1, grValY1));
    threshValue.setBounds (fr (thrValX0, thrValY0, thrValX1, thrValY1));
    rangeValue.setBounds  (fr (rngValX0, rngValY0, rngValX1, rngValY1));
    attackValue.setBounds (fr (atkValX0, topValY0, atkValX1, topValY1));
    holdValue.setBounds   (fr (hldValX0, topValY0, hldValX1, topValY1));
    releaseValue.setBounds (fr (relValX0, botValY0, relValX1, botValY1));
    hystValue.setBounds   (fr (hysValX0, botValY0, hysValX1, botValY1));
    hpfValue.setBounds    (fr (hpfValX0, hpfValY0, hpfValX1, hpfValY1));

    brand.setBounds (0, 0, 0, 0);
    brandSub.setBounds (0, 0, 0, 0);
    leftCard = centerCard = rightCard = presetPill = {};
}

//==============================================================================
void VocalGateEditor::timerCallback()
{
    const float range = proc.apvts.getRawParameterValue ("range")->load();

    grMeter.setGr (proc.grDb.load(), range);
    grValue.setText (grMeter.getGrDb() < 0.05f
                         ? juce::String ("0.0 dB")
                         : juce::String (-grMeter.getGrDb(), 1) + " dB",
                     juce::dontSendNotification);

    const float thr = proc.apvts.getRawParameterValue ("threshold")->load();
    threshValue.setText (juce::String (thr, 1) + " dB", juce::dontSendNotification);
    rangeValue.setText (juce::String (-range, 1) + " dB", juce::dontSendNotification);

    const float atk = proc.apvts.getRawParameterValue ("attack")->load();
    attackValue.setText (juce::String (atk, atk < 10.0f ? 2 : 1) + " ms", juce::dontSendNotification);

    holdValue.setText (juce::String (juce::roundToInt (
                           proc.apvts.getRawParameterValue ("hold")->load())) + " ms",
                       juce::dontSendNotification);
    releaseValue.setText (juce::String (juce::roundToInt (
                              proc.apvts.getRawParameterValue ("release")->load())) + " ms",
                          juce::dontSendNotification);
    hystValue.setText (juce::String (
                           proc.apvts.getRawParameterValue ("hysteresis")->load(), 1) + " dB",
                       juce::dontSendNotification);
    hpfValue.setText (juce::String (juce::roundToInt (
                          proc.apvts.getRawParameterValue ("scHpf")->load())) + " Hz",
                      juce::dontSendNotification);

    presetName.setText (proc.getProgramName (proc.getCurrentProgram()),
                        juce::dontSendNotification);

    if (plateBaked)
        repaint();   // meter fill + ring wedges + lit masks live in paintPlate
}

//==============================================================================
void VocalGateEditor::paint (juce::Graphics& g)
{
    if (plateBaked)
    {
        paintPlate (g);
        return;
    }

    theme::backdrop (g, getLocalBounds());

    auto card = [&] (juce::Rectangle<int> r, float radius = 20.0f)
    {
        auto rf = r.toFloat();
        theme::elevate (g, rf, radius);
        g.setColour (theme::card);
        g.fillRoundedRectangle (rf, radius);
        theme::topHighlight (g, rf, radius);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (rf, radius, 1.0f);
    };

    // Two-tone wordmark + accent underline (drawn at the brand label's anchor).
    {
        auto bb = brand.getBounds();
        auto wm = theme::font (26.0f, true);
        g.setFont (wm);
        const float vw = juce::GlyphArrangement::getStringWidth (wm, "vocal");
        g.setColour (theme::ink);
        g.drawText ("vocal", bb, juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.drawText ("gate", bb.withTrimmedLeft ((int) vw), juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.fillRoundedRectangle ((float) bb.getX(), (float) bb.getY() + 26.0f, 22.0f, 2.5f, 1.25f);
    }
    theme::spacedText (g, "VOCAL GATE", brandSub.getBounds().toFloat(),
                       theme::inkSoft, 9.5f, 2.6f, false, juce::Justification::left);

    card (leftCard);
    card (centerCard);
    card (rightCard);

    // preset capsule
    card (presetPill, presetPill.getHeight() * 0.5f);

    // Section headers — letter-spaced uppercase labels
    auto header = [&] (juce::Rectangle<int> c, const juce::String& t)
    {
        theme::spacedText (g, t, c.reduced (14, 12).removeFromTop (18).toFloat(),
                           theme::ink, 11.5f, 2.0f, true, juce::Justification::centred);
    };
    header (leftCard,  "Gate");
    header (rightCard, "Envelope");

    // Captions under controls — small letter-spaced text below each knob
    auto caption = [&] (const juce::Component& c, const juce::String& t)
    {
        theme::spacedText (g, t,
                           juce::Rectangle<float> ((float) c.getX(), (float) c.getBottom() - 2.0f,
                                                   (float) c.getWidth(), 14.0f),
                           theme::inkSoft, 10.0f, 1.8f, false, juce::Justification::centred);
    };
    caption (threshKnob,  "Threshold");
    caption (rangeKnob,   "Range");
    caption (attackKnob,  "Attack");
    caption (holdKnob,    "Hold");
    caption (releaseKnob, "Release");
    caption (hystKnob,    "Hysteresis");
    caption (hpfKnob,     "SC HPF");

    // Value pills (drawn behind the labels, which are child components)
    auto valuePill = [&] (juce::Component& c)
    {
        auto r = c.getBounds().toFloat();
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
    };
    valuePill (grValue);
    valuePill (threshValue);
    valuePill (rangeValue);
    valuePill (attackValue);
    valuePill (holdValue);
    valuePill (releaseValue);
    valuePill (hystValue);
    valuePill (hpfValue);
}

//==============================================================================
void VocalGateEditor::resized()
{
    licenseOverlay.setBounds (getLocalBounds());

    if (plateBaked)
    {
        layoutPlate();
        return;
    }

    auto content = getLocalBounds().reduced (18);

    auto centred = [] (juce::Rectangle<int> area, int w, int h)
    {
        return juce::Rectangle<int> (area.getCentreX() - w / 2, area.getCentreY() - h / 2, w, h);
    };

    // --- header: wordmark left, preset capsule right ---
    auto headerRow = content.removeFromTop (52);
    {
        auto brandArea = headerRow.removeFromLeft (220);
        brand.setBounds (brandArea.removeFromTop (32));
        brandSub.setBounds (brandArea.removeFromTop (16));

        auto pres = headerRow.removeFromRight (270).withSizeKeepingCentre (270, 38);
        presetPill = pres;
        auto inner = pres.reduced (5);
        prevBtn.setBounds (inner.removeFromLeft (28));
        nextBtn.setBounds (inner.removeFromRight (28));
        presetName.setBounds (inner);
    }
    content.removeFromTop (12);

    // --- three cards: gate meter | threshold + range | envelope grid ---
    leftCard = content.removeFromLeft (172);
    content.removeFromLeft (14);
    rightCard = content.removeFromRight (330);
    content.removeFromRight (14);
    centerCard = content;

    // Left card: GR meter with ruler, value readout at the bottom
    {
        auto c = leftCard.reduced (16);
        c.removeFromTop (30);                                  // header
        auto valueRow = c.removeFromBottom (26);
        c.removeFromBottom (10);
        grMeter.setBounds (c.withTrimmedLeft ((c.getWidth() - 76) / 2));
        grValue.setBounds (centred (valueRow, 82, 24));
    }

    // Centre card: hero THRESHOLD knob, RANGE knob below
    {
        auto c = centerCard.reduced (16);
        c.removeFromTop (6);

        auto heroArea = c.removeFromTop (232);
        threshKnob.setBounds (centred (heroArea, 220, 220));
        c.removeFromTop (14);                                  // caption strip
        threshValue.setBounds (centred (c.removeFromTop (28), 96, 26));
        c.removeFromTop (6);

        auto rangeArea = c.removeFromTop (118);
        rangeKnob.setBounds (centred (rangeArea, 110, 110));
        c.removeFromTop (14);                                  // caption strip
        rangeValue.setBounds (centred (c.removeFromTop (26), 84, 24));
    }

    // Right card: 2x2 grid (attack/hold/release/hysteresis) + bottom row
    {
        auto c = rightCard.reduced (16);
        c.removeFromTop (30);                                  // header

        auto bottomRow = c.removeFromBottom (96);
        c.removeFromBottom (8);

        auto grid = c;
        const int cellW = grid.getWidth() / 2;
        const int cellH = grid.getHeight() / 2;

        auto cell = [&] (int col, int row)
        {
            return juce::Rectangle<int> (grid.getX() + col * cellW,
                                         grid.getY() + row * cellH, cellW, cellH);
        };

        auto placeSmall = [&] (GateKnob& k, juce::Label& v, juce::Rectangle<int> cellR)
        {
            auto cc = cellR.reduced (6);
            auto valueRow = cc.removeFromBottom (22);
            cc.removeFromBottom (14);                          // caption strip
            k.setBounds (centred (cc, juce::jmin (cc.getWidth(), cc.getHeight(), 92),
                                      juce::jmin (cc.getWidth(), cc.getHeight(), 92)));
            v.setBounds (centred (valueRow, 74, 20));
        };
        placeSmall (attackKnob,  attackValue,  cell (0, 0));
        placeSmall (holdKnob,    holdValue,    cell (1, 0));
        placeSmall (releaseKnob, releaseValue, cell (0, 1));
        placeSmall (hystKnob,    hystValue,    cell (1, 1));

        // bottom row: SC HPF knob + value on the left, SC LISTEN pill right
        auto hpfArea = bottomRow.removeFromLeft (cellW);
        {
            auto cc = hpfArea.reduced (6);
            auto valueRow = cc.removeFromBottom (22);
            cc.removeFromBottom (14);                          // caption strip
            hpfKnob.setBounds (centred (cc, juce::jmin (cc.getWidth(), cc.getHeight(), 58),
                                            juce::jmin (cc.getWidth(), cc.getHeight(), 58)));
            hpfValue.setBounds (centred (valueRow, 74, 20));
        }
        scListenBtn.setBounds (centred (bottomRow, 122, 34));
    }
}
