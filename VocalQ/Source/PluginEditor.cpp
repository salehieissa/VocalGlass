#include "PluginEditor.h"

//==============================================================================
namespace
{
    juce::String hzText (double v)
    {
        if (v >= 1000.0) return juce::String (v / 1000.0, v >= 10000.0 ? 0 : 1) + "k";
        return juce::String (juce::roundToInt (v));
    }
    juce::String dbText (double v)  { return (v > 0.0 ? "+" : "") + juce::String (v, 1); }
    juce::String msText (double v)  { return v >= 1000.0 ? juce::String (v / 1000.0, 1) + "s" : juce::String (v, v < 10.0 ? 1 : 0); }
    juce::String qText (double v)   { return juce::String (v, v < 10.0 ? 1 : 0); }
}

//==============================================================================
// Baked-plate geometry. The chassis PNGs are 2048x1360 and the plate fills the
// whole canvas; x/w are fractions of the image width, y/h of its height.
// Everything below is measured from the plates (ON-vs-OFF diff components,
// groove scans on the OFF plate).
namespace plategeo
{
    // seven knobs: ring centres + neon ring radii (fractions of W)
    // Centres come from circle-fitting the seat grooves on the OFF plate.
    constexpr float knobCx[7] = { 0.2302f, 0.3302f, 0.4267f, 0.5220f,
                                  0.6178f, 0.7125f, 0.8077f };
    constexpr float knobCy     = 0.7753f;
    constexpr float ringDomeR  = 0.0254f;
    constexpr float ringSolidR = 0.0356f, ringMaxR = 0.0400f;
    constexpr float knobDomeDia = 0.0581f;          // dome hugs the seat groove

    // white value capsules under the knobs
    constexpr float capY0 = 0.8360f, capY1 = 0.8757f, capHalfW = 0.0293f;

    // band tab row: mask rects run seam-to-seam (never expand!)
    constexpr float tabY0 = 0.5625f, tabY1 = 0.6272f;
    constexpr float tabX[9] = { 0.0508f, 0.1318f, 0.2256f, 0.3193f, 0.4097f,
                                0.4971f, 0.5947f, 0.6831f, 0.7715f };

    // OUT stepper circles + live readout between them
    constexpr float outMinusX0 = 0.8364f, outMinusX1 = 0.8696f;
    constexpr float outPlusX0  = 0.9263f, outPlusX1  = 0.9585f;
    constexpr float outY0 = 0.5691f, outY1 = 0.6191f;
    constexpr float outValX0 = 0.8716f, outValX1 = 0.9253f;

    // header preset stepper (circle-fit of the arrow buttons on the OFF plate:
    // prev centre 1544,86 R28; next centre 1889,85 R27; pill 1570..1840 x 61..114)
    constexpr float prevX0 = 0.7383f, prevX1 = 0.7695f;
    constexpr float nextX0 = 0.9067f, nextX1 = 0.9380f;
    // pill cavity is asymmetric (left rim 26px, right rim 4px) — centre the
    // label on the cavity (1599..1839 @2x), not the outer edges
    constexpr float presX0 = 0.7808f, presX1 = 0.8979f;
    constexpr float headY0 = 0.0390f, headY1 = 0.0860f;

    // left column: power circle + channel pills (seam-to-seam)
    constexpr float powX0 = 0.0688f, powX1 = 0.1011f;
    constexpr float powY0 = 0.6507f, powY1 = 0.6993f;
    constexpr float chanX0 = 0.0459f, chanX1 = 0.1240f;
    constexpr float chanY[4] = { 0.7029f, 0.7687f, 0.8298f, 0.8875f };

    // filter-type icon strip (six squares, seam-to-seam)
    constexpr float typeX0 = 0.1260f, typeX1 = 0.1636f;
    constexpr float typeY[7] = { 0.6500f, 0.6949f, 0.7390f, 0.7868f,
                                 0.8272f, 0.8699f, 0.9088f };

    // solo pill
    constexpr float soloX0 = 0.8677f, soloX1 = 0.9517f;
    constexpr float soloY0 = 0.7551f, soloY1 = 0.8147f;

    // analyzer panel (the EQDisplay component covers exactly this rect)
    constexpr float dispX0 = 0.0415f, dispY0 = 0.1029f;
    constexpr float dispX1 = 0.9570f, dispY1 = 0.5537f;
}

//==============================================================================
VocalQEditor::VocalQEditor (VocalQProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), display (p)
{
    setLookAndFeel (&lnf);

    chassisImg   = skin::image ("q-chassis@2x.png");
    chassisOnImg = skin::image ("q-chassis-on@2x.png");
    const bool baked = chassisImg.isValid() && chassisOnImg.isValid();
    lnf.plate = baked;

    // Wordmark is drawn two-tone in paint(); keep the label for layout anchoring.
    titleLabel.setText ("", juce::dontSendNotification);
    titleLabel.setFont (theme::font (26.0f, true));
    titleLabel.setColour (juce::Label::textColourId, theme::ink);
    addAndMakeVisible (titleLabel);

    subLabel.setText ("DYNAMIC VOCAL EQ", juce::dontSendNotification);
    subLabel.setFont (theme::font (11.0f, true));
    subLabel.setColour (juce::Label::textColourId, theme::inkSoft);
    addAndMakeVisible (subLabel);

    // ---- preset stepper ----
    auto stepper = [this] (Bouncy<juce::TextButton>& b, const juce::String& t)
    {
        b.setButtonText (t);
        addAndMakeVisible (b);
    };
    stepper (presetPrev, juce::String::fromUTF8 ("\u2039"));
    stepper (presetNext, juce::String::fromUTF8 ("\u203a"));
    presetPrev.onClick = [this] { const int n = proc.getNumPrograms();
        proc.setCurrentProgram ((proc.getCurrentProgram() - 1 + n) % n); };
    presetNext.onClick = [this] { const int n = proc.getNumPrograms();
        proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % n); };
    presetName.setJustificationType (juce::Justification::centred);
    presetName.setFont (theme::font (13.0f, true));
    presetName.setColour (juce::Label::textColourId, theme::ink);
    addAndMakeVisible (presetName);

    // ---- display ----
    display.selectedBand = selectedBand;
    display.onSelectBand = [this] (int b) { selectBand (b); };
    addAndMakeVisible (display);

    // ---- band tabs ----
    for (int b = 0; b < VocalQProcessor::kNumBands; ++b)
    {
        auto& t = tabs[(size_t) b];
        t.number = (b >= 1 && b <= 6) ? b : 0;
        t.type   = (int) proc.apvts.getRawParameterValue (VocalQProcessor::bandParamId (b, "type"))->load();
        t.onClick = [this, b] { selectBand (b); };
        addAndMakeVisible (t);
    }

    // ---- out stepper ----
    {
        outMinus.setButtonText (juce::String::fromUTF8 ("\u2212"));
        outPlus.setButtonText ("+");
        addAndMakeVisible (outMinus); addAndMakeVisible (outPlus);
        outName.setText ("OUT", juce::dontSendNotification);
        outName.setFont (theme::font (11.0f, true));
        outName.setColour (juce::Label::textColourId, theme::inkSoft);
        outName.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (outName);
        outVal.setFont (theme::font (15.0f, true));
        outVal.setColour (juce::Label::textColourId, theme::accent);
        outVal.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (outVal);
    }
    outMinus.onClick = [this] { nudgeParam ("out", -0.5f); };
    outPlus.onClick  = [this] { nudgeParam ("out",  0.5f); };

    // ---- bottom panel: power + channel ----
    addAndMakeVisible (powerBtn);

    const char* chanText[3] = { "STEREO", "MID", "SIDES" };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = chanBtns[(size_t) i];
        b.setButtonText (chanText[i]);
        b.setClickingTogglesState (false);
        b.onClick = [this, i] {
            if (auto* pr = proc.apvts.getParameter (VocalQProcessor::bandParamId (selectedBand, "chan")))
                pr->setValueNotifyingHost (pr->getNormalisableRange().convertTo0to1 ((float) i)); };
        addAndMakeVisible (b);
    }

    // ---- type selector ----
    for (int i = 0; i < 6; ++i)
    {
        auto& b = typeBtns[(size_t) i];
        b.type = i;
        b.onClick = [this, i] {
            if (auto* pr = proc.apvts.getParameter (VocalQProcessor::bandParamId (selectedBand, "type")))
                pr->setValueNotifyingHost (pr->getNormalisableRange().convertTo0to1 ((float) i)); };
        addAndMakeVisible (b);
    }

    // ---- knobs ----
    freqK.setup  ("Freq (Hz)", hzText);
    qK.setup     ("Q", qText);
    gainK.setup  ("Gain", dbText);
    rangeK.setup ("Range", dbText);
    threshK.setup ("Threshold", [] (double v) { return juce::String (v, 1); }, true);
    atkK.setup   ("Attack", msText);
    relK.setup   ("Release", msText);
    for (auto* k : { &freqK, &qK, &gainK, &rangeK, &threshK, &atkK, &relK })
        addAndMakeVisible (*k);

    // ---- solo / sidechain ----
    soloBtn.setButtonText ("SOLO");
    soloBtn.setClickingTogglesState (false);
    soloBtn.onClick = [this] {
        const bool nowOn = proc.soloBand.load() != selectedBand;
        proc.soloBand.store (nowOn ? selectedBand : -1); };
    addAndMakeVisible (soloBtn);

    // ---- plate mode: baked chassis carries all static art ----
    if (baked)
    {
        display.setPlateMode (true);

        for (auto& t : tabs)      t.plate = true;
        powerBtn.plate = true;
        for (auto& b : typeBtns)  b.plate = true;

        for (auto* b : { (juce::Component*) &chanBtns[0], (juce::Component*) &chanBtns[1],
                         (juce::Component*) &chanBtns[2], (juce::Component*) &soloBtn,
                         (juce::Component*) &outMinus,    (juce::Component*) &outPlus,
                         (juce::Component*) &presetPrev,  (juce::Component*) &presetNext })
            b->setComponentID ("hit");

        outName.setVisible (false);     // baked
        subLabel.setVisible (false);
        outVal.setFont (theme::font (15.0f, true));

        Knob* knobs[7] = { &freqK, &qK, &gainK, &rangeK, &threshK, &atkK, &relK };
        for (int i = 0; i < 7; ++i)
        {
            knobs[i]->setPlateMode (true);
            auto& s = knobs[i]->getSlider();
            s.setRotaryParameters (juce::MathConstants<float>::pi,
                                   juce::MathConstants<float>::pi * 3.0f, true);
            s.getProperties().set ("domeScale", 0.78);

            auto& v = knobVals[(size_t) i];
            v.setJustificationType (juce::Justification::centred);
            v.setFont (theme::font (14.0f, true));
            v.setColour (juce::Label::textColourId, theme::ink);
            v.setInterceptsMouseClicks (false, false);
            addAndMakeVisible (v);
        }
    }

    bindBand (selectedBand);
    startTimerHz (30);
    setSize (1024, baked ? 680 : 740);

    // License overlay sits on top of everything; it shows itself until activated.
    addChildComponent (licenseOverlay);
    licenseOverlay.setBounds (getLocalBounds());
}

VocalQEditor::~VocalQEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void VocalQEditor::selectBand (int band)
{
    selectedBand = band;
    display.selectedBand = band;
    if (proc.soloBand.load() >= 0) proc.soloBand.store (band);
    bindBand (band);
    display.repaint();
}

void VocalQEditor::bindBand (int b)
{
    // Destroy old attachments FIRST — otherwise the slider snapping to the new
    // band's value would be written back into the previously selected band.
    aFreq.reset(); aQ.reset(); aGain.reset(); aRange.reset();
    aThresh.reset(); aAtk.reset(); aRel.reset(); aPower.reset();

    auto id = [b] (const char* s) { return VocalQProcessor::bandParamId (b, s); };
    aFreq   = std::make_unique<SA> (proc.apvts, id ("freq"),   freqK.getSlider());
    aQ      = std::make_unique<SA> (proc.apvts, id ("q"),      qK.getSlider());
    aGain   = std::make_unique<SA> (proc.apvts, id ("gain"),   gainK.getSlider());
    aRange  = std::make_unique<SA> (proc.apvts, id ("range"),  rangeK.getSlider());
    aThresh = std::make_unique<SA> (proc.apvts, id ("thresh"), threshK.getSlider());
    aAtk    = std::make_unique<SA> (proc.apvts, id ("atk"),    atkK.getSlider());
    aRel    = std::make_unique<SA> (proc.apvts, id ("rel"),    relK.getSlider());
    aPower  = std::make_unique<BA> (proc.apvts, id ("on"),     powerBtn);
}

void VocalQEditor::nudgeParam (const juce::String& pid, float delta)
{
    if (auto* p = proc.apvts.getParameter (pid))
    {
        const auto& r = p->getNormalisableRange();
        const float cur = r.convertFrom0to1 (p->getValue());
        p->setValueNotifyingHost (r.convertTo0to1 (juce::jlimit (r.start, r.end, cur + delta)));
    }
}

//==============================================================================
void VocalQEditor::timerCallback()
{
    display.updateMeters();

    auto raw = [this] (int b, const char* s) {
        return proc.apvts.getRawParameterValue (VocalQProcessor::bandParamId (b, s))->load(); };

    for (int b = 0; b < VocalQProcessor::kNumBands; ++b)
    {
        auto& t = tabs[(size_t) b];
        const bool sel = (b == selectedBand);
        const bool on  = raw (b, "on") > 0.5f;
        const int  ty  = (int) raw (b, "type");
        if (t.selected != sel || t.bandOn != on || t.type != ty)
        {
            t.selected = sel; t.bandOn = on; t.type = ty; t.repaint();
        }
    }

    const int chan = (int) raw (selectedBand, "chan");
    for (int i = 0; i < 3; ++i) chanBtns[(size_t) i].setToggleState (i == chan, juce::dontSendNotification);

    const int type = (int) raw (selectedBand, "type");
    for (int i = 0; i < 6; ++i)
        if (typeBtns[(size_t) i].selected != (i == type)) { typeBtns[(size_t) i].selected = (i == type); typeBtns[(size_t) i].repaint(); }

    soloBtn.setToggleState (proc.soloBand.load() == selectedBand, juce::dontSendNotification);

    outVal.setText (dbText (proc.apvts.getRawParameterValue ("out")->load()), juce::dontSendNotification);
    presetName.setText (proc.getProgramName (proc.getCurrentProgram()), juce::dontSendNotification);

    if (chassisImg.isValid())
    {
        Knob* knobs[7] = { &freqK, &qK, &gainK, &rangeK, &threshK, &atkK, &relK };
        for (int i = 0; i < 7; ++i)
            knobVals[(size_t) i].setText (knobs[i]->valueText(), juce::dontSendNotification);
        repaint();   // lit masks + ring wedges live in paintPlate
    }
}

//==============================================================================
void VocalQEditor::paint (juce::Graphics& g)
{
    if (chassisImg.isValid() && chassisOnImg.isValid())
    {
        paintPlate (g);
        return;
    }

    theme::backdrop (g, getLocalBounds());

    // two-tone wordmark: ink "vocal" + accent "q", with an accent underline tick
    {
        auto wm = theme::font (26.0f, true);
        g.setFont (wm);
        const auto tb = titleLabel.getBounds().toFloat();
        const float vw = juce::GlyphArrangement::getStringWidth (wm, "vocal");
        g.setColour (theme::ink);
        g.drawText ("vocal", tb, juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.drawText ("q", tb.withTrimmedLeft (vw), juce::Justification::centredLeft);
        g.fillRoundedRectangle (tb.getX(), tb.getCentreY() + 14.0f, 20.0f, 2.5f, 1.25f);
    }

    // display card sits raised off the canvas (the display paints its own face)
    if (! displayArea.isEmpty())
        theme::elevate (g, displayArea.toFloat(), 6.0f);

    // bottom panel card — raised, with a top highlight and hairline edge
    theme::elevate (g, panelArea.toFloat(), 14.0f);
    g.setColour (theme::card);
    g.fillRoundedRectangle (panelArea.toFloat(), 14.0f);
    theme::topHighlight (g, panelArea.toFloat(), 14.0f);
    g.setColour (theme::cardLine);
    g.drawRoundedRectangle (panelArea.toFloat(), 14.0f, 1.0f);

    // dotted dividers between panel groups
    if (divX1 > 0)
        for (float yy = (float) panelArea.getY() + 16.0f; yy <= (float) panelArea.getBottom() - 16.0f; yy += 5.4f)
            { g.setColour (theme::cardLine); g.fillEllipse ((float) divX1 - 0.7f, yy, 1.4f, 1.4f); }
    if (divX2 > 0)
        for (float yy = (float) panelArea.getY() + 16.0f; yy <= (float) panelArea.getBottom() - 16.0f; yy += 5.4f)
            { g.setColour (theme::cardLine); g.fillEllipse ((float) divX2 - 0.7f, yy, 1.4f, 1.4f); }
}

//==============================================================================
juce::Rectangle<int> VocalQEditor::plateFracRect (float fx, float fy, float fw, float fh) const
{
    return juce::Rectangle<float> (fx * (float) getWidth(),  fy * (float) getHeight(),
                                   fw * (float) getWidth(),  fh * (float) getHeight())
               .toNearestInt();
}

// Blit the matching region of the lit plate over the base plate — pixel
// registration is guaranteed because both images share the same canvas.
void VocalQEditor::maskFromOn (juce::Graphics& g, juce::Rectangle<int> screenRect)
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
void VocalQEditor::maskFromOnFeathered (juce::Graphics& g, juce::Rectangle<int> screenRect,
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
void VocalQEditor::drawRingWedge (juce::Graphics& g, juce::Slider& s, float cxFrac, float cyFrac,
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

void VocalQEditor::paintPlate (juce::Graphics& g)
{
    using namespace plategeo;

    g.drawImage (chassisImg, getLocalBounds().toFloat(),
                 juce::RectanglePlacement::stretchToFit);

    const int feather = juce::roundToInt ((float) getWidth() * 0.008f);

    // ---- selected band tab lights up (ON plate has every tab lit)
    maskFromOnFeathered (g, plateFracRect (tabX[selectedBand], tabY0,
                                           tabX[selectedBand + 1] - tabX[selectedBand],
                                           tabY1 - tabY0), feather);

    // ---- left column: power / channel / type / solo states
    if (powerBtn.getToggleState())
        maskFromOnFeathered (g, plateFracRect (powX0, powY0, powX1 - powX0, powY1 - powY0), feather);

    for (int i = 0; i < 3; ++i)
        if (chanBtns[(size_t) i].getToggleState())
            maskFromOnFeathered (g, plateFracRect (chanX0, chanY[i], chanX1 - chanX0,
                                                   chanY[i + 1] - chanY[i]), feather);

    for (int i = 0; i < 6; ++i)
        if (typeBtns[(size_t) i].selected)
            maskFromOnFeathered (g, plateFracRect (typeX0, typeY[i], typeX1 - typeX0,
                                                   typeY[i + 1] - typeY[i]), feather);

    if (soloBtn.getToggleState())
        maskFromOnFeathered (g, plateFracRect (soloX0, soloY0, soloX1 - soloX0, soloY1 - soloY0), feather);

    // ---- knob neon ring wedges
    Knob* knobs[7] = { &freqK, &qK, &gainK, &rangeK, &threshK, &atkK, &relK };
    for (int i = 0; i < 7; ++i)
        drawRingWedge (g, knobs[i]->getSlider(), knobCx[i], knobCy,
                       ringDomeR, ringSolidR, ringMaxR);
}

//==============================================================================
void VocalQEditor::resized()
{
    licenseOverlay.setBounds (getLocalBounds());

    if (chassisImg.isValid() && chassisOnImg.isValid())
    {
        using namespace plategeo;
        const float W = (float) getWidth(), H = (float) getHeight();
        auto fr = [&] (float fx0, float fy0, float fx1, float fy1)
        {
            return juce::Rectangle<int> (juce::roundToInt (fx0 * W), juce::roundToInt (fy0 * H),
                                         juce::roundToInt ((fx1 - fx0) * W), juce::roundToInt ((fy1 - fy0) * H));
        };

        // header
        presetPrev.setBounds (fr (prevX0, headY0, prevX1, headY1));
        presetNext.setBounds (fr (nextX0, headY0, nextX1, headY1));
        presetName.setBounds (fr (presX0, 61.0f / 1360.0f, presX1, 114.0f / 1360.0f));
        titleLabel.setBounds (0, 0, 0, 0);
        subLabel.setBounds (0, 0, 0, 0);

        // analyzer
        displayArea = fr (dispX0, dispY0, dispX1, dispY1);
        display.setBounds (displayArea);

        // band tabs + out stepper
        for (int b = 0; b < VocalQProcessor::kNumBands; ++b)
            tabs[(size_t) b].setBounds (fr (tabX[b], tabY0, tabX[b + 1], tabY1));
        outMinus.setBounds (fr (outMinusX0, outY0, outMinusX1, outY1));
        outPlus.setBounds  (fr (outPlusX0,  outY0, outPlusX1,  outY1));
        outVal.setBounds   (fr (outValX0,   outY0, outValX1,   outY1));
        outName.setBounds (0, 0, 0, 0);

        // left column
        powerBtn.setBounds (fr (powX0, powY0, powX1, powY1));
        for (int i = 0; i < 3; ++i)
            chanBtns[(size_t) i].setBounds (fr (chanX0, chanY[i], chanX1, chanY[i + 1]));
        for (int i = 0; i < 6; ++i)
            typeBtns[(size_t) i].setBounds (fr (typeX0, typeY[i], typeX1, typeY[i + 1]));
        soloBtn.setBounds (fr (soloX0, soloY0, soloX1, soloY1));

        // knobs: bounds are a square around the dome; capsules hold the values
        const float domeSide = knobDomeDia * W / 0.78f;   // matches domeScale
        Knob* knobs[7] = { &freqK, &qK, &gainK, &rangeK, &threshK, &atkK, &relK };
        for (int i = 0; i < 7; ++i)
        {
            const float cx = knobCx[i] * W, cy = knobCy * H;
            knobs[i]->setBounds (juce::Rectangle<float> (cx - domeSide * 0.5f, cy - domeSide * 0.5f,
                                                         domeSide, domeSide).toNearestInt());
            knobVals[(size_t) i].setBounds (fr (knobCx[i] - capHalfW, capY0,
                                                knobCx[i] + capHalfW, capY1));
        }

        panelArea = {};
        divX1 = divX2 = 0;
        return;
    }

    auto area = getLocalBounds().reduced (18);

    // ---- header ----
    auto header = area.removeFromTop (40);
    titleLabel.setBounds (header.getX(), header.getY(), 120, 40);
    subLabel.setBounds (header.getX() + 122, header.getY() + 13, 160, 18);
    {
        auto pr = header.removeFromRight (210);
        presetPrev.setBounds (pr.removeFromLeft (34).reduced (2, 6));
        presetNext.setBounds (pr.removeFromRight (34).reduced (2, 6));
        presetName.setBounds (pr.reduced (2, 6));
    }

    area.removeFromTop (10);

    // ---- display ----
    displayArea = area.removeFromTop (360);
    display.setBounds (displayArea);

    area.removeFromTop (12);

    // ---- band tabs + mix/out ----
    auto tabsRow = area.removeFromTop (38);
    {
        auto outBox = tabsRow.removeFromRight (165);
        outName.setBounds (outBox.removeFromLeft (44));
        outMinus.setBounds (outBox.removeFromLeft (32).reduced (2));
        outPlus.setBounds  (outBox.removeFromRight (32).reduced (2));
        outVal.setBounds   (outBox);

        tabsRow.removeFromRight (16);
        const int n = VocalQProcessor::kNumBands;
        const int tw = tabsRow.getWidth() / n;
        for (int b = 0; b < n; ++b)
            tabs[(size_t) b].setBounds (tabsRow.getX() + b * tw, tabsRow.getY(), tw - 6, tabsRow.getHeight());
    }

    area.removeFromTop (12);

    // ---- bottom panel ----
    panelArea = area;
    auto panel = area.reduced (16, 14);

    // left column: power + channel
    auto leftCol = panel.removeFromLeft (74);
    powerBtn.setBounds (leftCol.removeFromTop (34).withSizeKeepingCentre (34, 34));
    leftCol.removeFromTop (6);
    {
        const int ch = leftCol.getHeight() / 3;
        for (int i = 0; i < 3; ++i)
            chanBtns[(size_t) i].setBounds (leftCol.removeFromTop (ch).reduced (0, 3));
    }

    // type column
    auto typeCol = panel.removeFromLeft (40);
    {
        const int th = typeCol.getHeight() / 6;
        for (int i = 0; i < 6; ++i)
            typeBtns[(size_t) i].setBounds (typeCol.removeFromTop (th).reduced (3, 2));
    }
    divX1 = typeCol.getRight() + 8;

    // right column: solo
    auto rightCol = panel.removeFromRight (96);
    divX2 = rightCol.getX() - 8;
    soloBtn.setBounds (rightCol.withSizeKeepingCentre (rightCol.getWidth() - 12, 36));

    // knobs row
    panel.removeFromLeft (10);
    panel.removeFromRight (10);
    Knob* knobs[7] = { &freqK, &qK, &gainK, &rangeK, &threshK, &atkK, &relK };
    const int kw = panel.getWidth() / 7;
    for (int i = 0; i < 7; ++i)
        knobs[i]->setBounds (juce::Rectangle<int> (panel.getX() + i * kw, panel.getY(), kw, panel.getHeight())
                                 .reduced (9, 0));
}
