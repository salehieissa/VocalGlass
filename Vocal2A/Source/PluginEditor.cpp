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
    // Match the baked plate's opaque aspect (1213x764 -> 1.588:1) so the art
    // maps 1:1 with no stretch; otherwise keep the legacy size.
    if (chassisImg.isValid()) setSize (1024, 645);
    else                      setSize (1024, 650);

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

    repaint();
}

//==============================================================================
// Opaque faceplate box inside the 1254x1254 plate art (measured from alpha).
namespace { constexpr float OX0 = 20.0f, OY0 = 250.0f, OX1 = 1233.0f, OY1 = 1014.0f; }

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
// Reveal the matching region of the lit plate over the base. Both plates share
// the exact same geometry, so the source region maps 1:1 by fraction.
void Vocal2AEditor::maskFromOn (juce::Graphics& g, juce::Rectangle<int> R)
{
    if (! chassisOnImg.isValid() || R.isEmpty()) return;
    auto pr = plateRect();
    const float PW = OX1 - OX0, PH = OY1 - OY0;
    const float sx = OX0 + ((float) R.getX() - pr.getX()) / pr.getWidth()  * PW;
    const float sy = OY0 + ((float) R.getY() - pr.getY()) / pr.getHeight() * PH;
    const float sw = (float) R.getWidth()  / pr.getWidth()  * PW;
    const float sh = (float) R.getHeight() / pr.getHeight() * PH;
    g.drawImage (chassisOnImg, R.getX(), R.getY(), R.getWidth(), R.getHeight(),
                 juce::roundToInt (sx), juce::roundToInt (sy),
                 juce::roundToInt (sw), juce::roundToInt (sh), false);
}

// The lit knob halo comes from the on-plate. `full` reveals the whole ring
// (the mockup's small knobs glow all the way around); otherwise it's clipped
// to a pie wedge from the start angle to the live value like a value ring.
void Vocal2AEditor::drawKnobHalo (juce::Graphics& g, VintageKnob& k, float ringRfracW, bool full)
{
    if (! chassisOnImg.isValid()) return;
    const auto c = k.getBounds().toFloat().getCentre();
    const float ringR = ringRfracW * plateRect().getWidth();
    const float R = ringR * 1.32f;                 // pie/halo outer radius

    const int side = juce::roundToInt (R * 2.0f);
    juce::Rectangle<int> box (juce::roundToInt (c.x) - side / 2,
                              juce::roundToInt (c.y) - side / 2, side, side);

    // The dome masks the glow: clip out the disc under the knob so the halo
    // only lands outside the metal, never tinting the chrome rim. The hole
    // radius tracks the sprite's measured visible-dome fraction.
    const float domeR = k.getBounds().toFloat().getWidth() * 0.5f * k.domeDiaFrac() * 0.98f;

    const float a0 = VintageKnob::startAngle();
    const float a1 = k.valueAngle();
    // At full value the entire baked ring lights, including the bottom gap
    // outside the pointer sweep ("goes to 100").
    const bool ringFull = full || a1 >= VintageKnob::endAngle() - 0.01f;
    if (! ringFull && a1 <= a0 + 0.001f) return;

    juce::Path clip;
    if (ringFull)
    {
        // full annulus: outer disc with the dome disc as an even-odd hole
        clip.addEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);
        clip.addEllipse (c.x - domeR, c.y - domeR, domeR * 2.0f, domeR * 2.0f);
        clip.setUsingNonZeroWinding (false);
    }
    else
    {
        // annular wedge: the pie segment's inner-circle proportion carves the
        // dome hole without ever adding area outside the wedge
        clip.addPieSegment (c.x - R, c.y - R, R * 2.0f, R * 2.0f, a0, a1, domeR / R);
    }

    g.saveState();
    g.reduceClipRegion (clip);
    maskFromOn (g, box);
    g.restoreState();
}

//==============================================================================
void Vocal2AEditor::paintPlate (juce::Graphics& g)
{
    // charcoal backdrop (shows only in the plate's rounded corners)
    {
        auto b = getLocalBounds().toFloat();
        juce::ColourGradient vg (juce::Colour (0xff2b2d31), 0.0f, b.getY(),
                                 juce::Colour (0xff141518), 0.0f, b.getBottom(), false);
        vg.addColour (0.5, juce::Colour (0xff202226));
        g.setGradientFill (vg);
        g.fillRect (b);
    }

    auto pr = plateRect();

    // ---- base (off) plate: draw the opaque faceplate region into the window ----
    g.drawImage (chassisImg, pr.getX(), pr.getY(), pr.getWidth(), pr.getHeight(),
                 (int) OX0, (int) OY0, (int) (OX1 - OX0), (int) (OY1 - OY0), false);

    // ---- masked lit regions from the on-plate ----
    const int vuSrc  = (int) proc.apvts.getRawParameterValue ("vuSource")->load();
    const int analog = (int) proc.apvts.getRawParameterValue ("analog")->load();
    // Rects already run seam-to-seam, so no expansion: growing them would
    // reveal the neighbouring pill's lit edge (the ON plate has ALL pills lit).
    if (vuSrc  >= 0 && vuSrc  < 3) maskFromOn (g, vuBtnR[(size_t) vuSrc]);
    if (analog >= 0 && analog < 3) maskFromOn (g, analogBtnR[(size_t) analog]);

    drawKnobHalo (g, gainKnob, 0.099f);
    drawKnobHalo (g, peakKnob, 0.099f);
    for (auto* k : { &hiFreqKnob, &attackKnob, &releaseKnob, &mixKnob, &trimKnob })
        drawKnobHalo (g, *k, 0.041f);   // value-masked; full ring lights at max

    // ---- auto-makeup LED (code jewel over the blank baked mount) ----
    {
        const bool lit = autoSwitch.getToggleState();
        auto lb = ledBounds.toFloat();
        const auto c = lb.getCentre();
        const float rr = lb.getWidth() * 0.5f;
        if (lit)
        {
            juce::ColourGradient bloom (juce::Colour (0xffff5a7a).withAlpha (0.75f), c.x, c.y,
                                        juce::Colours::transparentBlack, c.x, c.y + rr * 2.4f, true);
            g.setGradientFill (bloom);
            g.fillEllipse (lb.expanded (rr * 1.6f));
            g.setColour (juce::Colour (0xffff2b4d));
            g.fillEllipse (lb);
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.fillEllipse (c.x - rr * 0.35f, c.y - rr * 0.5f, rr * 0.5f, rr * 0.5f);
        }
        else
        {
            g.setColour (juce::Colour (0xff6a2630));
            g.fillEllipse (lb);
        }
    }
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
        auto pr = plateRect();
        auto knobRect = [&] (float cx, float cy, float diamFracW)
        {
            const float d = diamFracW * pr.getWidth();
            const auto c = plateXY (cx, cy);
            return juce::Rectangle<float> (c.x - d * 0.5f, c.y - d * 0.5f, d, d).toNearestInt();
        };
        auto boxWH = [&] (float cx, float cy, float wFracW, float hFracW)
        {
            const float w = wFracW * pr.getWidth(), h = hFracW * pr.getWidth();
            const auto c = plateXY (cx, cy);
            return juce::Rectangle<float> (c.x - w * 0.5f, c.y - h * 0.5f, w, h).toNearestInt();
        };

        // big knobs (dome only; ring seat + halo are on the plate). Centres are
        // the measured lit-halo circle centres from the ON/OFF plate diff, so
        // dome, seat and halo all share one centre.
        // New sprites have more transparent margin: dome is 0.765 (large) /
        // 0.518 (small) of the canvas, so bounds grow to keep the same dome size.
        gainKnob.setBounds (knobRect (0.1810f, 0.4156f, 0.203f));
        peakKnob.setBounds (knobRect (0.8075f, 0.4156f, 0.203f));
        gainVal.setBounds (boxWH (0.1810f, 0.214f, 0.13f, 0.055f));
        peakVal.setBounds (boxWH (0.8075f, 0.214f, 0.13f, 0.055f));

        // small utility knobs (measured halo centres)
        const std::array<std::pair<float,float>,5> sc {{
            {0.3689f,0.8331f},{0.4897f,0.8338f},{0.6109f,0.8338f},{0.7333f,0.8318f},{0.8549f,0.8338f} }};
        std::array<VintageKnob*,5> sk { &hiFreqKnob,&attackKnob,&releaseKnob,&mixKnob,&trimKnob };
        std::array<juce::Label*,5>  scap { &hiFreqCap,&attackCap,&releaseCap,&mixCap,&trimCap };
        for (int i = 0; i < 5; ++i)
        {
            sk[(size_t) i]->setBounds (knobRect (sc[(size_t) i].first, sc[(size_t) i].second, 0.120f));
            // value readout: centred just under the knob, above the tray bevel
            scap[(size_t) i]->setBounds (boxWH (sc[(size_t) i].first, 0.920f, 0.12f, 0.030f));
            scap[(size_t) i]->setJustificationType (juce::Justification::centred);
        }

        // selector buttons (invisible hit areas; visuals baked/masked).
        // Edges are the measured pill boundaries + seam valleys from the plate
        // diff, so each mask covers exactly its own lit pill and nothing else.
        auto bandRects = [&] (const std::array<float,4>& xe, float y0, float y1,
                              std::array<juce::Rectangle<int>,3>& out)
        {
            for (int i = 0; i < 3; ++i)
                out[(size_t) i] = plateFracRect (xe[(size_t) i], y0,
                                                 xe[(size_t) i + 1] - xe[(size_t) i], y1 - y0);
        };
        bandRects ({ 0.6777f, 0.7609f, 0.8557f, 0.9398f }, 0.0902f, 0.1571f, vuBtnR);
        bandRects ({ 0.0709f, 0.1418f, 0.2127f, 0.2819f }, 0.7957f, 0.8666f, analogBtnR);
        for (int i = 0; i < 3; ++i) vuButtons[(size_t) i].setBounds (vuBtnR[(size_t) i]);
        for (int i = 0; i < 3; ++i) analogButtons[(size_t) i].setBounds (analogBtnR[(size_t) i]);

        // bat toggles + code LED, centred on the measured label text row
        // (compress/limit glyphs span y 0.632-0.652, centre 0.642). mode sits in
        // the gap between "compress" (ends 0.1484) and "limit" (starts 0.2152);
        // auto sits just right of "auto makeup" (ends 0.7988) with the LED beside.
        modeSwitch.setBounds (boxWH (0.1818f, 0.6420f, 0.080f, 0.044f));
        autoSwitch.setBounds (boxWH (0.8400f, 0.6420f, 0.080f, 0.044f));
        ledBounds = boxWH (0.8850f, 0.6420f, 0.020f, 0.020f);

        // VU needle over the baked meter face (hub near the bottom-centre of the
        // cream face; tip reaches the printed scale arc)
        vuCard = plateFracRect (0.350f, 0.300f, 0.340f, 0.300f);
        vu.setBounds (vuCard);
        vu.pivotFx = 0.491f; vu.pivotFy = 0.850f; vu.tipLenFx = 0.350f;

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
