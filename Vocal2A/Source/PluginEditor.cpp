#include "PluginEditor.h"

namespace
{
    const std::array<juce::String, 3> kVuWords     { "input", "GR", "output" };
    const std::array<juce::String, 3> kAnalogWords { "50Hz", "60Hz", "off" };
}

//==============================================================================
Vocal2AEditor::Vocal2AEditor (Vocal2AProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);

    // ---- branding (two-tone wordmark + tagline are painted in paint()) ----
    juce::ignoreUnused (brand, brandSub);

    // ---- VU source selector ----
    vuTitle.setText ("VU display", juce::dontSendNotification);
    vuTitle.setFont (theme::font (15.0f, false));
    vuTitle.setColour (juce::Label::textColourId, theme::ink);
    vuTitle.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (vuTitle);

    const bool baked = chassisImg.isValid();

    for (int i = 0; i < 3; ++i)
    {
        auto& b = vuButtons[(size_t) i];
        b.setButtonText (baked ? "" : kVuWords[(size_t) i]);  // label is baked into the plate
        b.setClickingTogglesState (false);
        b.setComponentID ("flat");
        b.onClick = [this, i] { setChoice ("vuSource", i); };
        addAndMakeVisible (b);
    }

    // ---- big knobs ----
    gainKnob.setPlateMode (baked);
    peakKnob.setPlateMode (baked);
    addAndMakeVisible (gainKnob);
    addAndMakeVisible (peakKnob);
    gainAtt = std::make_unique<SliderAtt> (proc.apvts, "gain", gainKnob);
    peakAtt = std::make_unique<SliderAtt> (proc.apvts, "peakReduction", peakKnob);

    // live numbers above the big knobs (their "gain"/"peak reduction" text is baked)
    auto setupVal = [this] (juce::Label& l)
    {
        l.setFont (theme::font (27.0f, true));
        l.setColour (juce::Label::textColourId, theme::ink);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };
    setupVal (gainVal);
    setupVal (peakVal);

    auto setupCap = [this] (juce::Label& l, const juce::String& t, float size, bool bold)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (theme::font (size, bold));
        l.setColour (juce::Label::textColourId, theme::ink);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };

    setupCap (gainCap, "gain", 17.0f, false);
    setupCap (peakCap, "peak reduction", 17.0f, false);

    // ---- toggles ----
    modeSwitch.setClickingTogglesState (true);
    modeSwitch.onClick = [this] { setChoice ("mode", modeSwitch.getToggleState() ? 1 : 0); };
    addAndMakeVisible (modeSwitch);

    addAndMakeVisible (autoSwitch);
    autoAtt = std::make_unique<ButtonAtt> (proc.apvts, "autoMakeup", autoSwitch);

    setupCap (modeCapL, "compress", 12.5f, false);
    setupCap (modeCapR, "limit",    12.5f, false);
    setupCap (autoCap,  "auto makeup", 12.5f, false);
    modeCapL.setJustificationType (juce::Justification::centredRight);
    modeCapR.setJustificationType (juce::Justification::centredLeft);
    autoCap.setJustificationType (juce::Justification::centredRight);
    for (auto* l : { &modeCapL, &modeCapR, &autoCap })
        l->setColour (juce::Label::textColourId, theme::inkSoft);

    // ---- VU meter ----
    addAndMakeVisible (vu);

    // ---- bottom strip ----
    setupCap (analogLabel, "analog", 15.0f, false);
    analogLabel.setJustificationType (juce::Justification::centredLeft);

    for (int i = 0; i < 3; ++i)
    {
        auto& b = analogButtons[(size_t) i];
        b.setButtonText (baked ? "" : kAnalogWords[(size_t) i]);  // label is baked
        b.setClickingTogglesState (false);
        b.setComponentID ("seg");
        b.onClick = [this, i] { setChoice ("analog", i); };
        addAndMakeVisible (b);
    }

    setupCap (hiFreqLabel, "hi freq", 15.0f, false);
    hiFreqLabel.setJustificationType (juce::Justification::centredLeft);
    setupCap (attackLabel,  "attack", 15.0f, false);
    setupCap (releaseLabel, "release", 15.0f, false);
    setupCap (mixLabel, "mix", 15.0f, false);
    setupCap (trimLabel, "trim", 15.0f, false);

    setupCap (hiFreqCap, "flat", 12.0f, false);
    setupCap (attackCap,  "10 ms", 12.0f, false);
    setupCap (releaseCap, "1.2 s", 12.0f, false);
    setupCap (mixCap, "100%", 12.0f, false);
    setupCap (trimCap, "0.0 dB", 12.0f, false);
    // On the baked plate the readouts print in black like the other panel text;
    // the vector fallback keeps the softer grey.
    for (auto* c : { &hiFreqCap, &attackCap, &releaseCap, &mixCap, &trimCap })
        c->setColour (juce::Label::textColourId, baked ? theme::ink : theme::inkSoft);

    for (auto* k : { &hiFreqKnob, &attackKnob, &releaseKnob, &mixKnob, &trimKnob })
    {
        k->setBigValueVisible (false);
        k->setPlateMode (baked);
        addAndMakeVisible (*k);
    }
    hiFreqAtt  = std::make_unique<SliderAtt> (proc.apvts, "hiFreq", hiFreqKnob);
    attackAtt  = std::make_unique<SliderAtt> (proc.apvts, "attack", attackKnob);
    releaseAtt = std::make_unique<SliderAtt> (proc.apvts, "release", releaseKnob);
    mixAtt     = std::make_unique<SliderAtt> (proc.apvts, "mix", mixKnob);
    trimAtt    = std::make_unique<SliderAtt> (proc.apvts, "trim", trimKnob);

    startTimerHz (30);
    // Crop-to-chrome: the window is exactly the plate's opaque region, so the
    // hardware fills it edge-to-edge with no backdrop. Height follows the crop.
    if (chassisImg.isValid())
    {
        plateCrop = skin::plateBounds (chassisImg);
        setSize (1024, juce::roundToInt (1024.0f * (float) plateCrop.getHeight()
                                                  / (float) plateCrop.getWidth()));
    }
    else
        setSize (1024, 650);

    // License overlay sits on top of everything; it shows itself until activated.
    addChildComponent (licenseOverlay);
    licenseOverlay.setBounds (getLocalBounds());
}

Vocal2AEditor::~Vocal2AEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void Vocal2AEditor::setChoice (const juce::String& paramID, int index)
{
    if (auto* param = proc.apvts.getParameter (paramID))
        param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 ((float) index));
}

//==============================================================================
// Measured geometry of the glass/chrome/neon plates (trimmed 2030x1336 canvas):
// ON/OFF diff for pill bands, halo centres and the LED; radial pink profiles
// for ring radii; OFF-plate luminance for seat grooves, toggles and text rows.
namespace plategeo
{
    constexpr float PX = 2030.0f, PY = 1336.0f;
    constexpr float px (float v) { return v / PX; }
    constexpr float py (float v) { return v / PY; }

    // big knobs (halo centres from the plate diff)
    constexpr float gainCx = px (352.0f), peakCx = px (1658.0f), bigCy = py (546.0f);
    constexpr float bigDomeR = px (160.0f), bigSolidR = px (200.0f), bigMaxR = px (232.0f);
    constexpr float bigDomeBox = px (444.0f);   // dome = 0.765 of the sprite canvas

    // small utility knobs
    constexpr float smCx[5] = { px (750.5f), px (999.0f), px (1248.5f), px (1503.0f), px (1749.0f) };
    constexpr float smCy = py (1102.0f);
    constexpr float smallDomeR = px (54.0f), smallSolidR = px (92.0f), smallMaxR = px (112.0f);
    constexpr float smallDomeBox = px (224.0f); // dome = 0.518 of the sprite canvas

    // selector pill bands (edges are the measured seam valleys, seam-to-seam)
    constexpr float vuX[4] = { px (1360.0f), px (1565.0f), px (1729.0f), px (1929.0f) };
    constexpr float vuY0 = py (111.0f), vuY1 = py (210.0f);
    constexpr float anX[4] = { px (116.0f), px (266.0f), px (426.0f), px (575.0f) };
    constexpr float anY0 = py (1049.0f), anY1 = py (1155.0f);

    // bat toggles sit in the measured caption gaps; LED jewel is baked
    constexpr float modeCx = px (362.0f), autoCx = px (1715.0f), togCy = py (843.0f);
    constexpr float togW = px (140.0f), togH = px (80.0f);
    constexpr float ledX0 = px (1783.0f), ledX1 = px (1839.0f);
    constexpr float ledY0 = py (828.0f),  ledY1 = py (884.0f);

    // VU meter face interior + the always-on pink under-glow strip at its foot
    constexpr float vuFX0 = px (690.0f), vuFX1 = px (1330.0f);
    constexpr float vuFY0 = py (395.0f), vuFY1 = py (782.0f);
    constexpr float stripX0 = px (705.0f), stripX1 = px (1320.0f);
    constexpr float stripY0 = py (758.0f), stripY1 = py (792.0f);
    // needle pivot = centre of the circle fitted through the printed numbers
    constexpr float vuPivotX = px (1013.7f), vuPivotY = py (993.5f);
    constexpr float vuTipLen = px (523.0f);

    // live readouts (clean glass: above the big knobs, under the small ones)
    constexpr float bigValW = px (200.0f), bigValY0 = py (222.0f), bigValY1 = py (288.0f);
    constexpr float smValW  = px (170.0f), smValY0  = py (1188.0f), smValY1 = py (1238.0f);
}

//==============================================================================
void Vocal2AEditor::timerCallback()
{
    const int vuSource = (int) proc.apvts.getRawParameterValue ("vuSource")->load();
    const int analog   = (int) proc.apvts.getRawParameterValue ("analog")->load();
    const int mode     = (int) proc.apvts.getRawParameterValue ("mode")->load();

    for (int i = 0; i < 3; ++i)
        vuButtons[(size_t) i].setToggleState (i == vuSource, juce::dontSendNotification);
    for (int i = 0; i < 3; ++i)
        analogButtons[(size_t) i].setToggleState (i == analog, juce::dontSendNotification);

    modeSwitch.setToggleState (mode == OptoLeveler::Limit, juce::dontSendNotification);

    // big-knob live numbers (their static labels are baked into the plate)
    gainVal.setText (juce::String (juce::roundToInt (proc.apvts.getRawParameterValue ("gain")->load())),
                     juce::dontSendNotification);
    peakVal.setText (juce::String (juce::roundToInt (proc.apvts.getRawParameterValue ("peakReduction")->load())),
                     juce::dontSendNotification);

    // small knob captions
    const float hiFreq = proc.apvts.getRawParameterValue ("hiFreq")->load();
    hiFreqCap.setText (hiFreq <= 1.0f ? "flat"
                       : hiFreq >= 99.0f ? "bright"
                       : juce::String (juce::roundToInt (hiFreq)), juce::dontSendNotification);

    const float attack = proc.apvts.getRawParameterValue ("attack")->load();
    attackCap.setText (juce::String (attack, attack < 10.0f ? 1 : 0) + " ms",
                       juce::dontSendNotification);

    const float release = proc.apvts.getRawParameterValue ("release")->load();
    releaseCap.setText (release >= 1000.0f
                            ? juce::String (release / 1000.0f, 2) + " s"
                            : juce::String (juce::roundToInt (release)) + " ms",
                        juce::dontSendNotification);

    const float mix = proc.apvts.getRawParameterValue ("mix")->load();
    mixCap.setText (juce::String (juce::roundToInt (mix)) + "%", juce::dontSendNotification);

    const float trim = proc.apvts.getRawParameterValue ("trim")->load();
    trimCap.setText (juce::String (trim, 1) + " dB", juce::dontSendNotification);

    // drive the VU needle from the selected source (scale value -20..+3)
    float scaleValue = 0.0f;
    if (vuSource == 1) // GR: rest at 0, swings left as reduction grows
        scaleValue = -proc.engine.grReductionDb.load();
    else if (vuSource == 0) // input level, 0 VU ~ -18 dBFS
        scaleValue = proc.engine.inputDb.load() + 18.0f;
    else                    // output level
        scaleValue = proc.engine.outputDb.load() + 18.0f;

    vu.setLevel (scaleValue);

    if (! chassisImg.isValid())
    {
        repaint();
        return;
    }

    // ---- dirty-region repaints: only invalidate what changed this tick ----
    // (the VU needle repaints itself through the meter component's own timer)
    using namespace plategeo;
    struct KnobRegion { VintageKnob* k; float cx, cy, maxR; };
    const KnobRegion regions[] = {
        { &gainKnob,    gainCx, bigCy, bigMaxR },
        { &peakKnob,    peakCx, bigCy, bigMaxR },
        { &hiFreqKnob,  smCx[0], smCy, smallMaxR },
        { &attackKnob,  smCx[1], smCy, smallMaxR },
        { &releaseKnob, smCx[2], smCy, smallMaxR },
        { &mixKnob,     smCx[3], smCy, smallMaxR },
        { &trimKnob,    smCx[4], smCy, smallMaxR },
    };
    // radius fractions are of image WIDTH; vertical extents need the aspect factor
    const float ar = (float) chassisImg.getWidth() / (float) chassisImg.getHeight();
    for (size_t i = 0; i < 7; ++i)
    {
        const double v = regions[i].k->getValue();
        if (v != shownKnob[i])
        {
            shownKnob[i] = v;
            repaint (plateFracRect (regions[i].cx - regions[i].maxR,
                                    regions[i].cy - regions[i].maxR * ar,
                                    regions[i].maxR * 2.0f,
                                    regions[i].maxR * 2.0f * ar).expanded (2));
        }
    }

    if (vuSource != shownVuSrc)
    {
        shownVuSrc = vuSource;
        repaint (plateFracRect (vuX[0], vuY0, vuX[3] - vuX[0], vuY1 - vuY0).expanded (2));
    }
    if (analog != shownAnalog)
    {
        shownAnalog = analog;
        repaint (plateFracRect (anX[0], anY0, anX[3] - anX[0], anY1 - anY0).expanded (2));
    }

    const bool autoOn = autoSwitch.getToggleState();
    if (autoOn != shownAuto)
    {
        shownAuto = autoOn;
        repaint (plateFracRect (ledX0, ledY0, ledX1 - ledX0, ledY1 - ledY0).expanded (4));
    }
}

//==============================================================================
void Vocal2AEditor::paint (juce::Graphics& g)
{
    if (chassisImg.isValid()) { paintPlate (g); return; }

    // ---- dark charcoal backdrop so the glass plate floats with real depth ----
    {
        auto b = getLocalBounds().toFloat();
        juce::ColourGradient vg (juce::Colour (0xff2b2d31), 0.0f, b.getY(),
                                 juce::Colour (0xff141518), 0.0f, b.getBottom(), false);
        vg.addColour (0.5, juce::Colour (0xff202226));
        g.setGradientFill (vg);
        g.fillRect (b);

        // corner vignette for a moody studio-hardware feel
        juce::ColourGradient vig (juce::Colours::transparentBlack, b.getCentreX(), b.getCentreY(),
                                  juce::Colours::black.withAlpha (0.45f), b.getX(), b.getBottom(), true);
        g.setGradientFill (vig);
        g.fillRect (b);
    }

    // When a baked chassis is present it carries ALL the static metal (plate,
    // tray, wells + shadows, housings, screws, dividers), so the code chrome for
    // those pieces is skipped. Text + live values + motion stay in code.
    const bool chassis = chassisImg.isValid();

    // ---- floating glass plate, inset from the window so charcoal frames it ----
    {
        auto rf = cardArea.toFloat();

        // soft cast shadow under the plate
        {
            juce::DropShadow ds (juce::Colours::black.withAlpha (0.55f), 34, { 0, 14 });
            juce::Path p; p.addRoundedRectangle (rf.reduced (6.0f), 26.0f);
            ds.drawForPath (g, p);
        }

        if (chassis)
        {
            skin::drawInRect (g, chassisImg, rf);
        }
        else if (panelImg.isValid())
        {
            skin::drawInRect (g, panelImg, rf);
        }
        else if (faceImg.isValid())
        {
            skin::drawInRect (g, faceImg, rf.expanded (rf.getWidth() * 0.06f,
                                                       rf.getHeight() * 0.10f));
        }
        else
        {
            g.setColour (theme::card);
            g.fillRoundedRectangle (rf, 22.0f);
            theme::topHighlight (g, rf, 22.0f);
            g.setColour (theme::cardLine);
            g.drawRoundedRectangle (rf, 22.0f, 1.0f);
        }
    }

    // ---- recessed bottom tray (only when NOT baked into the chassis) ----
    if (! chassis)
    {
        auto tray = trayArea.toFloat();
        const float rc = 16.0f;

        // sunken well: frosted darker glass with an inner top shadow + light lip
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillRoundedRectangle (tray.translated (0.0f, 1.5f), rc); // bottom light lip
        juce::ColourGradient well (juce::Colour (0x14000000), tray.getX(), tray.getY(),
                                   juce::Colour (0x05000000), tray.getX(), tray.getBottom(), false);
        g.setGradientFill (well);
        g.fillRoundedRectangle (tray, rc);
        {
            juce::ColourGradient inner (juce::Colours::black.withAlpha (0.22f), tray.getX(), tray.getY(),
                                        juce::Colours::transparentBlack, tray.getX(), tray.getY() + 16.0f, false);
            g.setGradientFill (inner);
            g.fillRoundedRectangle (tray.withHeight (18.0f), rc);
        }
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.drawRoundedRectangle (tray.reduced (0.5f), rc, 1.0f);
    }

    // Static text is drawn in code UNLESS a baked chassis carries it already.
    if (! chassis)
    {
        // two-tone wordmark ("vocal" ink + "2a" accent) with an accent underline tick
        {
            auto wm = theme::font (30.0f, true);
            g.setFont (wm);
            const float vw = juce::GlyphArrangement::getStringWidth (wm, "vocal");
            g.setColour (theme::ink);
            g.drawText ("vocal", brandBounds, juce::Justification::centredLeft, false);
            g.setColour (theme::accent);
            g.drawText ("2a", brandBounds.withTrimmedLeft ((int) vw),
                        juce::Justification::centredLeft, false);
            g.setColour (theme::accent);
            g.fillRoundedRectangle ((float) brandBounds.getX(),
                                    (float) brandBounds.getBottom() - 6.0f, 22.0f, 2.5f, 1.25f);
        }
        theme::spacedText (g, "VOCAL LEVELER", brandSubBounds.toFloat(),
                           theme::inkSoft, 9.5f, 2.6f, false, juce::Justification::centredLeft);

        // "gain reduction" title on the glass, above the screen
        theme::spacedText (g, "gain reduction", vuTitleArea.toFloat(),
                           theme::ink, 12.5f, 2.4f, true, juce::Justification::centred);
    }

    // VU meter housing. The photoreal VU face carries its own chrome bezel and
    // black frame, so we only lay down a soft cast shadow beneath it to seat it
    // on the glass. If the asset is missing we fall back to a code smoked-glass
    // well so the vector meter still has a recessed screen to swing across.
    const bool vuHasSkin = skin::has ("2a-vu-face@2x.png") && skin::has ("2a-vu-needle@2x.png");
    if (vuHasSkin)
    {
        // The photoreal VU face carries its own chrome bezel + black frame and a
        // baked drop shadow, so it seats on the glass with no extra code chrome.
    }
    else
    {
        auto rf = vuCard.toFloat();
        const float r = 20.0f;

        {
            juce::DropShadow ds (juce::Colours::black.withAlpha (0.30f), 22, { 0, 8 });
            juce::Path p; p.addRoundedRectangle (rf, r);
            ds.drawForPath (g, p);
        }

        g.setGradientFill ({ juce::Colour (0xfff4f6f8), rf.getX(), rf.getY(),
                             juce::Colour (0xff9298a1), rf.getX(), rf.getBottom(), false });
        g.fillRoundedRectangle (rf, r);
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.drawRoundedRectangle (rf.reduced (0.75f), r, 1.0f);

        auto scr = rf.reduced (7.0f);
        const float rs = r - 5.0f;
        g.setGradientFill ({ juce::Colour (0xff23262b), scr.getX(), scr.getY(),
                             juce::Colour (0xff0a0b0d), scr.getX(), scr.getBottom(), false });
        g.fillRoundedRectangle (scr, rs);

        {
            juce::ColourGradient inner (juce::Colours::black.withAlpha (0.55f), scr.getX(), scr.getY(),
                                        juce::Colours::transparentBlack, scr.getX(), scr.getY() + 22.0f, false);
            g.setGradientFill (inner);
            g.fillRoundedRectangle (scr.withHeight (26.0f), rs);
        }

        {
            juce::Path gloss;
            gloss.addRoundedRectangle (scr.getX(), scr.getY(), scr.getWidth(), scr.getHeight() * 0.5f, rs);
            g.saveState();
            g.reduceClipRegion (gloss);
            g.setGradientFill ({ juce::Colours::white.withAlpha (0.10f), scr.getX(), scr.getY(),
                                 juce::Colours::transparentWhite, scr.getRight(), scr.getCentreY(), false });
            g.fillRect (scr);
            g.restoreState();
        }

        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawRoundedRectangle (scr.reduced (0.5f), rs, 1.0f);
    }

    // Corner screws are only code-drawn when the base panel doesn't already bake
    // them in (panel@2x has photoreal screws in all four corners).
    if (! panelImg.isValid() && faceImg.isValid())
        drawScrews (g, getLocalBounds().toFloat().reduced (18.0f), 6.0f, 20.0f);

    // analog segmented container + divider (only when NOT baked into the chassis)
    if (! chassis)
    {
        theme::recess (g, analogPill.toFloat(), 10.0f);
        g.setColour (theme::cardLine);
        g.fillRect (trayDivider);
    }

    // auto-makeup status LED (photoreal jewel if present, else code jewel)
    {
        const bool lit = autoSwitch.getToggleState();
        auto lb = ledBounds.toFloat();
        if (ledOnImg.isValid() && ledOffImg.isValid())
        {
            skin::drawInRect (g, lit ? ledOnImg : ledOffImg, lb.expanded (2.0f));
        }
        else
        {
            const auto c = lb.getCentre();
            const float rr = lb.getWidth() * 0.5f;
            g.setColour (juce::Colour (0xffb6bac2));               // chrome bezel
            g.fillEllipse (lb.expanded (2.2f));
            if (lit)
            {
                juce::ColourGradient bloom (juce::Colour (0xffff5a5a).withAlpha (0.75f), c.x, c.y,
                                            juce::Colours::transparentBlack, c.x, c.y + rr * 2.0f, true);
                g.setGradientFill (bloom);
                g.fillEllipse (lb.expanded (rr * 1.3f));
                g.setColour (juce::Colour (0xffff2b2b));
                g.fillEllipse (lb);
                g.setColour (juce::Colours::white.withAlpha (0.85f));
                g.fillEllipse (c.x - rr * 0.35f, c.y - rr * 0.45f, rr * 0.5f, rr * 0.5f);
            }
            else
            {
                g.setColour (juce::Colour (0xff5a2326));
                g.fillEllipse (lb);
                g.setColour (juce::Colours::white.withAlpha (0.15f));
                g.fillEllipse (c.x - rr * 0.35f, c.y - rr * 0.45f, rr * 0.4f, rr * 0.4f);
            }
        }
    }
}

//==============================================================================
// Blit the matching region of the lit plate over the base plate. Both scaled
// caches share the editor's coordinate space, so this is a 1:1 copy — cheap,
// and registration is exact by construction.
void Vocal2AEditor::maskFromOn (juce::Graphics& g, juce::Rectangle<int> R)
{
    if (! plateOnScaled.isValid() || R.isEmpty()) return;
    g.drawImage (plateOnScaled, R.getX(), R.getY(), R.getWidth(), R.getHeight(),
                 R.getX(), R.getY(), R.getWidth(), R.getHeight());
}

// Same reveal but with a soft alpha ramp along the rect border, so the slight
// global tone drift between the two plates never shows as a hard rectangle.
void Vocal2AEditor::maskFromOnFeathered (juce::Graphics& g, juce::Rectangle<int> R, int featherPx)
{
    constexpr int n = 4;
    const float s = (float) featherPx / (float) n;
    const auto r = R.toFloat();

    g.saveState();
    g.reduceClipRegion (r.reduced ((float) featherPx).toNearestInt());
    maskFromOn (g, R);
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
        maskFromOn (g, R);
        g.restoreState();
    }
}

// Reveal a knob's lit neon ring as an annular wedge clipped to the value sweep
// (6 o'clock -> 6 o'clock), with an angular feather on both ends and a radial
// fade on the outer bloom so no hard cut ever shows.
void Vocal2AEditor::drawRingWedge (juce::Graphics& g, juce::Slider& s, float cxFrac, float cyFrac,
                                   float domeRFrac, float solidRFrac, float maxRFrac)
{
    const auto c = plateXY (cxFrac, cyFrac);
    const float iw = (float) chassisImg.getWidth();
    const float sx = (float) getWidth() / (float) plateCrop.getWidth();
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
        wedge (a0,             a0 + fIn * 0.5f, rIn, rOut, alpha * 0.33f);
        wedge (a0 + fIn * 0.5f, a0 + fIn,       rIn, rOut, alpha * 0.66f);
    }
}

//==============================================================================
void Vocal2AEditor::paintPlate (juce::Graphics& g)
{
    using namespace plategeo;

    g.drawImageAt (plateScaled, 0, 0);   // cached 1:1 blit — no per-frame rescale

    const int feather = juce::roundToInt ((float) getWidth() * 0.008f);

    // ---- selected pills from the lit plate ----
    // Rects run seam-to-seam, so no expansion: growing them would reveal the
    // neighbouring pill's lit edge (the ON plate has ALL pills lit).
    const int vuSrc  = (int) proc.apvts.getRawParameterValue ("vuSource")->load();
    const int analog = (int) proc.apvts.getRawParameterValue ("analog")->load();
    if (vuSrc  >= 0 && vuSrc  < 3) maskFromOn (g, vuBtnR[(size_t) vuSrc]);
    if (analog >= 0 && analog < 3) maskFromOn (g, analogBtnR[(size_t) analog]);

    // ---- knob neon ring wedges ----
    drawRingWedge (g, gainKnob, gainCx, bigCy, bigDomeR, bigSolidR, bigMaxR);
    drawRingWedge (g, peakKnob, peakCx, bigCy, bigDomeR, bigSolidR, bigMaxR);
    const std::array<VintageKnob*, 5> sk { &hiFreqKnob, &attackKnob, &releaseKnob, &mixKnob, &trimKnob };
    for (int i = 0; i < 5; ++i)
        drawRingWedge (g, *sk[(size_t) i], smCx[i], smCy, smallDomeR, smallSolidR, smallMaxR);

    // ---- VU under-glow strip: always lit while the plugin runs ----
    maskFromOnFeathered (g, plateFracRect (stripX0, stripY0, stripX1 - stripX0, stripY1 - stripY0),
                         feather);

    // ---- auto-makeup LED jewel: the lit state is baked into the ON plate ----
    if (autoSwitch.getToggleState())
        maskFromOnFeathered (g, plateFracRect (ledX0, ledY0, ledX1 - ledX0, ledY1 - ledY0),
                             juce::jmax (2, feather / 2));
}

//==============================================================================
void Vocal2AEditor::drawScrews (juce::Graphics& g, juce::Rectangle<float> panel,
                                float inset, float size)
{
    if (! screwImg.isValid()) return;
    const std::array<juce::Point<float>, 4> pts {{
        { panel.getX() + inset,              panel.getY() + inset },
        { panel.getRight() - inset,          panel.getY() + inset },
        { panel.getX() + inset,              panel.getBottom() - inset },
        { panel.getRight() - inset,          panel.getBottom() - inset }
    }};
    for (const auto& p : pts)
        skin::drawInRect (g, screwImg,
                          { p.x - size * 0.5f, p.y - size * 0.5f, size, size });
}

//==============================================================================
void Vocal2AEditor::resized()
{
    licenseOverlay.setBounds (getLocalBounds());

    // ---- baked-plate layout: everything keyed to the measured plate fractions ----
    if (chassisImg.isValid())
    {
        using namespace plategeo;

        // rebuild the scaled plate caches whenever the window geometry changes
        if (plateScaled.getWidth() != getWidth() || plateScaled.getHeight() != getHeight())
        {
            plateScaled = skin::renderPlate (chassisImg, plateCrop, getWidth(), getHeight());
            if (chassisOnImg.isValid())
                plateOnScaled = skin::renderPlate (chassisOnImg, plateCrop, getWidth(), getHeight());
        }

        const float iw = (float) chassisImg.getWidth();
        const float sx = (float) getWidth() / (float) plateCrop.getWidth();
        auto domeSquare = [&] (float cx, float cy, float diaFracW)
        {
            const float d = diaFracW * iw * sx;
            const auto c = plateXY (cx, cy);
            return juce::Rectangle<float> (c.x - d * 0.5f, c.y - d * 0.5f, d, d).toNearestInt();
        };
        auto boxWH = [&] (float cx, float cy, float wFracW, float hFracW)
        {
            const float w = wFracW * iw * sx, h = hFracW * iw * sx;
            const auto c = plateXY (cx, cy);
            return juce::Rectangle<float> (c.x - w * 0.5f, c.y - h * 0.5f, w, h).toNearestInt();
        };

        // big knobs: dome sprites sized so the dome hugs the seat groove
        gainKnob.setBounds (domeSquare (gainCx, bigCy, bigDomeBox));
        peakKnob.setBounds (domeSquare (peakCx, bigCy, bigDomeBox));
        gainVal.setBounds (plateFracRect (gainCx - bigValW * 0.5f, bigValY0, bigValW, bigValY1 - bigValY0));
        peakVal.setBounds (plateFracRect (peakCx - bigValW * 0.5f, bigValY0, bigValW, bigValY1 - bigValY0));

        // small utility knobs + live readouts under them
        const std::array<VintageKnob*, 5> sk { &hiFreqKnob, &attackKnob, &releaseKnob, &mixKnob, &trimKnob };
        const std::array<juce::Label*, 5> scap { &hiFreqCap, &attackCap, &releaseCap, &mixCap, &trimCap };
        for (int i = 0; i < 5; ++i)
        {
            sk[(size_t) i]->setBounds (domeSquare (smCx[i], smCy, smallDomeBox));
            scap[(size_t) i]->setBounds (plateFracRect (smCx[i] - smValW * 0.5f, smValY0,
                                                        smValW, smValY1 - smValY0));
            scap[(size_t) i]->setJustificationType (juce::Justification::centred);
        }

        // selector buttons (invisible hit areas; visuals baked/masked). Edges
        // are the measured seam valleys so each mask covers exactly its pill.
        for (int i = 0; i < 3; ++i)
        {
            vuBtnR[(size_t) i]     = plateFracRect (vuX[i], vuY0, vuX[i + 1] - vuX[i], vuY1 - vuY0);
            analogBtnR[(size_t) i] = plateFracRect (anX[i], anY0, anX[i + 1] - anX[i], anY1 - anY0);
            vuButtons[(size_t) i].setBounds (vuBtnR[(size_t) i]);
            analogButtons[(size_t) i].setBounds (analogBtnR[(size_t) i]);
        }

        // bat toggles in the measured caption gaps (compress|limit, auto makeup|LED)
        modeSwitch.setBounds (boxWH (modeCx, togCy, togW, togH));
        autoSwitch.setBounds (boxWH (autoCx, togCy, togW, togH));
        ledBounds = plateFracRect (ledX0, ledY0, ledX1 - ledX0, ledY1 - ledY0);

        // VU needle over the baked meter face. The pivot is the centre of the
        // circle fitted through the printed scale numbers (it sits below the
        // face, so the component bounds clip the needle at the glass edge).
        vuCard = plateFracRect (vuFX0, vuFY0, vuFX1 - vuFX0, vuFY1 - vuFY0);
        vu.setBounds (vuCard);
        vu.setInterceptsMouseClicks (false, false);
        vu.pivotFx = (vuPivotX - vuFX0) / (vuFX1 - vuFX0);
        vu.pivotFy = (vuPivotY - vuFY0) / (vuFY1 - vuFY0);
        vu.tipLenFx = vuTipLen / (vuFX1 - vuFX0);

        // hide baked static labels; keep live numbers/values visible
        for (auto* l : { &vuTitle, &gainCap, &peakCap, &modeCapL, &modeCapR, &autoCap,
                         &analogLabel, &hiFreqLabel, &attackLabel, &releaseLabel, &mixLabel, &trimLabel })
            l->setVisible (false);
        for (auto* l : { &gainVal, &peakVal, &hiFreqCap, &attackCap, &releaseCap, &mixCap, &trimCap })
            l->setVisible (true);
        return;
    }

    const int W = getWidth();
    cardArea = getLocalBounds().reduced (16);   // floating glass plate

    // Everything is placed with absolute coordinates measured off the approved
    // mockup, kept clear of the panel's baked corner screws (~x66/x958, y66/y584).

    // ---- top bar (sits below the top screws) ----
    brandBounds    = { 104, 50, 210, 46 };      // "vocal2a" wordmark + underline
    brandSubBounds = { 296, 64, 170, 22 };      // "VOCAL LEVELER"

    {   // VU DISPLAY selector, top-right, clear of the right screw
        const int bw = 74, bh = 30, gap = 6;
        const int total = bw * 3 + gap * 2;      // 234
        const int x0 = 936 - total;              // right edge ~936
        vuTitle.setBounds (x0, 48, total, 18);
        vuTitle.setJustificationType (juce::Justification::centred);
        for (int i = 0; i < 3; ++i)
            vuButtons[(size_t) i].setBounds (x0 + i * (bw + gap), 72, bw, bh);
    }

    // ---- hero row: big knobs flanking the VU, captions + toggles beneath ----
    const int lcx = 190, rcx = 834;                     // knob column centres
    const int knobW = 208, knobH = 250, knobTop = 138;  // Large box (top 34 = number)

    gainKnob.setBounds (lcx - knobW / 2, knobTop, knobW, knobH);
    peakKnob.setBounds (rcx - knobW / 2, knobTop, knobW, knobH);

    const int capY = knobTop + knobH - 4;               // caption right under the knob
    gainCap.setBounds (lcx - 90, capY, 180, 24);
    peakCap.setBounds (rcx - 90, capY, 180, 24);

    // flanking bat-lever toggles a row below the caption
    const int togY = capY + 30, togW = 56, togH = 28;
    modeSwitch.setBounds (lcx - togW / 2, togY, togW, togH);
    modeCapL.setBounds (lcx - togW / 2 - 96, togY, 90, togH);
    modeCapR.setBounds (lcx + togW / 2 + 6,  togY, 90, togH);

    const int aX = rcx - togW / 2 + 20;                 // shift right to leave room for LED
    autoSwitch.setBounds (aX, togY, togW, togH);
    autoCap.setBounds (aX - 126, togY, 120, togH);
    ledBounds = { aX + togW + 10, togY + togH / 2 - 8, 16, 16 };

    // ---- centre: "gain reduction" title + framed VU meter ----
    vuTitleArea = { 362, 156, 300, 22 };
    vuCard      = { 348, 178, 328, 236 };
    vu.setBounds (vuCard);

    // ---- recessed bottom tray ----
    trayArea    = { 50, 470, W - 100, 152 };            // bottom ~622
    bottomStrip = trayArea;

    // analog selector on the left, label centred above the pills
    analogPill = { trayArea.getX() + 34, 515, 226, 42 };  // centre ~536
    analogLabel.setBounds (analogPill.getX(), 485, analogPill.getWidth(), 18);
    analogLabel.setJustificationType (juce::Justification::centred);
    {
        const int seg = analogPill.getWidth() / 3;
        for (int i = 0; i < 3; ++i)
            analogButtons[(size_t) i].setBounds (analogPill.getX() + i * seg + 3,
                                                 analogPill.getY() + 3,
                                                 seg - 6, analogPill.getHeight() - 6);
    }

    // vertical divider between the analog group and the utility knobs
    trayDivider = { analogPill.getRight() + 28, trayArea.getY() + 22,
                    1, trayArea.getHeight() - 44 };

    // five utility knobs, evenly spaced across the right of the tray
    {
        const int kSize = 72, knobCy = 536;
        const int areaX = trayDivider.getX() + 24;
        const int areaR = trayArea.getRight() - 20;
        const float step = (float) (areaR - areaX) / 5.0f;

        std::array<juce::Label*, 5> labels { &hiFreqLabel, &attackLabel, &releaseLabel, &mixLabel, &trimLabel };
        std::array<juce::Label*, 5> caps   { &hiFreqCap,   &attackCap,   &releaseCap,   &mixCap,   &trimCap };
        std::array<VintageKnob*, 5> knobs  { &hiFreqKnob,  &attackKnob,  &releaseKnob,  &mixKnob,  &trimKnob };

        for (int i = 0; i < 5; ++i)
        {
            const int cx = areaX + juce::roundToInt (step * ((float) i + 0.5f));
            labels[(size_t) i]->setJustificationType (juce::Justification::centred);
            labels[(size_t) i]->setBounds (cx - 55, 485, 110, 18);
            knobs [(size_t) i]->setBounds (cx - kSize / 2, knobCy - kSize / 2, kSize, kSize);
            caps  [(size_t) i]->setBounds (cx - 55, knobCy + kSize / 2 - 2, 110, 16);
        }
    }

    // When a baked chassis carries the SF Pro labels, hide the code-drawn static
    // labels so text doesn't double up. Live value captions + knob numbers stay.
    const bool chassis = chassisImg.isValid();
    for (auto* l : { &vuTitle, &gainCap, &peakCap, &modeCapL, &modeCapR, &autoCap,
                     &analogLabel, &hiFreqLabel, &attackLabel, &releaseLabel, &mixLabel, &trimLabel })
        l->setVisible (! chassis);
}
