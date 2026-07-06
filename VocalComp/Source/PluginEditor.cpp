#include "PluginEditor.h"

namespace
{
    const std::array<juce::String, 3> kModeWords { "ARC", "Opto", "Warm" };
}

//==============================================================================
// Baked-plate geometry. The chassis PNGs are 2048x1360 and the plate fills the
// whole canvas; x/w are fractions of the image width, y/h of its height.
// Everything below is measured from the plates (ON-vs-OFF diff components,
// pink-core scans on the ON plate, groove scans on the OFF plate).
namespace plategeo
{
    // vertical THRESH / GAIN sliders: groove centres + travel span
    constexpr float threshCx = 0.1213f, gainCx = 0.8760f;
    constexpr float vSlY0 = 0.2412f, vSlY1 = 0.6690f;
    constexpr float vFillHalfW = 0.0105f;              // pink core + glow

    // IN / OUT meter channels (bottom-up fill)
    constexpr float inLCx = 0.0869f, inRCx = 0.1550f;
    constexpr float outLCx = 0.8450f, outRCx = 0.9080f;
    constexpr float mY0 = 0.2382f, mY1 = 0.6449f;
    constexpr float mHalfW = 0.0062f;

    // Attack / Release horizontal sliders (left-to-right fill)
    constexpr float hX0 = 0.4072f, hX1 = 0.5347f;
    constexpr float attackCy = 0.7824f, releaseCy = 0.8578f;
    constexpr float hFillHalfH = 0.0145f;

    // Gate / Mix / Trim neon rings: hot band r=[0.0173,0.0240] of W
    constexpr float gateCx = 0.6819f, mixCx = 0.7801f, trimCx = 0.8868f;
    constexpr float knobCy = 0.8241f;
    constexpr float ringDomeR  = 0.0166f;
    constexpr float ringSolidR = 0.0250f, ringMaxR = 0.0310f;
    constexpr float knobDomeDia = 0.0342f;             // hugs the seat inner edge

    // MODE pills: mask rects run seam-to-seam (never expand!)
    constexpr float pillY0 = 0.7971f, pillY1 = 0.8721f;
    constexpr float pillX[3][2] = { { 0.0581f, 0.1475f },
                                    { 0.1475f, 0.2236f },
                                    { 0.2236f, 0.3022f } };

    // RATIO display inner smoked-glass rect (curve overlay is clipped to it)
    constexpr float dispX0 = 0.2085f, dispY0 = 0.1985f;
    constexpr float dispX1 = 0.7886f, dispY1 = 0.6875f;

    // preset capsule (baked, chevron included)
    constexpr float boxX0 = 0.6973f, boxY0 = 0.0691f, boxX1 = 0.9355f, boxY1 = 0.1265f;

    // live value rows: captions sit at y 0.1625..0.175, grooves start 0.2412
    constexpr float valY0 = 0.1800f, valY1 = 0.2350f;
    // -∞ readouts under the IN / OUT captions (0.6632..0.6757)
    constexpr float readY0 = 0.6810f, readY1 = 0.7130f;
    // attack / release values sit right of the grooves
    constexpr float arValX0 = 0.5430f, arValX1 = 0.6400f;
    // gate OFF/dB readout shares the row with the baked 0/100 range marks
    constexpr float gateValY0 = 0.8720f, gateValY1 = 0.9000f;
}

//==============================================================================
VocalCompEditor::VocalCompEditor (VocalCompProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);

    chassisImg   = skin::image ("comp-chassis@2x.png");
    chassisOnImg = skin::image ("comp-chassis-on@2x.png");
    const bool baked = chassisImg.isValid() && chassisOnImg.isValid();
    laf.plate = baked;

    auto addLabel = [this] (juce::Label& l, const juce::String& text, float size, bool bold,
                            juce::Colour col, juce::Justification just)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (theme::font (size, bold));
        l.setColour (juce::Label::textColourId, col);
        l.setJustificationType (just);
        addAndMakeVisible (l);
    };

    // ---- branding (the wordmark + subtitle are painted two-tone in paint();
    //      these labels just reserve their layout slots) ----
    addLabel (brand,    "",  23.0f, true,  theme::ink,     juce::Justification::centredLeft);
    addLabel (brandSub, "",  11.0f, false, theme::inkSoft, juce::Justification::centredLeft);

    // ---- preset combo ----
    refreshPresetBox();
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != proc.getCurrentProgram())
            proc.setCurrentProgram (idx);
    };
    addAndMakeVisible (presetBox);

    // ---- left column (threshold + input) ----
    addLabel (threshCap, "THRESH", 11.0f, false, theme::inkSoft, juce::Justification::centred);
    addLabel (threshVal, "-28.0",  21.0f, true,  theme::ink,     juce::Justification::centred);
    addLabel (inputCap,  "IN",     10.0f, false, theme::inkSoft, juce::Justification::centred);
    addLabel (inLVal,    "-1.4",   13.0f, false, theme::ink,     juce::Justification::centred);
    addLabel (inRVal,    "-1.4",   13.0f, false, theme::ink,     juce::Justification::centred);

    threshSlider.setSliderStyle (juce::Slider::LinearVertical);
    threshSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (threshSlider);
    threshAtt = std::make_unique<SliderAtt> (proc.apvts, "threshold", threshSlider);

    addAndMakeVisible (inMeterL);
    addAndMakeVisible (inMeterR);

    // ---- centre (ratio) ----
    addLabel (ratioCap, "RATIO", 11.0f, false, theme::inkSoft, juce::Justification::centred);
    addAndMakeVisible (curve);
    ratioAtt = std::make_unique<SliderAtt> (proc.apvts, "ratio", curve);

    // ---- right column (makeup + output) ----
    addLabel (gainCap,   "GAIN",   11.0f, false, theme::inkSoft, juce::Justification::centred);
    addLabel (gainVal,   "0.0",    21.0f, true,  theme::ink,     juce::Justification::centred);
    addLabel (outputCap, "OUT",    10.0f, false, theme::inkSoft, juce::Justification::centred);
    addLabel (outLVal,   "-0.0",   13.0f, false, theme::ink,     juce::Justification::centred);
    addLabel (outRVal,   "-6.1",   13.0f, false, theme::ink,     juce::Justification::centred);

    gainSlider.setSliderStyle (juce::Slider::LinearVertical);
    gainSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (gainSlider);
    gainAtt = std::make_unique<SliderAtt> (proc.apvts, "makeup", gainSlider);

    addAndMakeVisible (outMeterL);
    addAndMakeVisible (outMeterR);

    // ---- attack / release ----
    addLabel (attackCap,  "Attack",  12.0f, false, theme::inkSoft, juce::Justification::centredLeft);
    addLabel (releaseCap, "Release", 12.0f, false, theme::inkSoft, juce::Justification::centredLeft);
    addLabel (attackVal,  "20.0",    13.0f, true,  theme::ink,     juce::Justification::centredRight);
    addLabel (releaseVal, "200",     13.0f, true,  theme::ink,     juce::Justification::centredRight);

    attackSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    attackSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (attackSlider);
    attackAtt = std::make_unique<SliderAtt> (proc.apvts, "attack", attackSlider);

    releaseSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    releaseSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (releaseSlider);
    releaseAtt = std::make_unique<SliderAtt> (proc.apvts, "release", releaseSlider);

    // ---- mode pills ----
    addLabel (modeCap, "MODE", 11.0f, false, theme::inkSoft, juce::Justification::centredLeft);
    for (int i = 0; i < 3; ++i)
    {
        auto& b = modeBtns[(size_t) i];
        b.setButtonText (kModeWords[(size_t) i]);
        b.setClickingTogglesState (false);
        b.onClick = [this, i] { setMode (i); };
        addAndMakeVisible (b);
    }

    // ---- gate / mix / trim ----
    addLabel (gateCap, "Gate", 12.0f, false, theme::inkSoft, juce::Justification::centred);
    addLabel (gateVal, "OFF",  10.0f, false, theme::inkSoft, juce::Justification::centred);
    addLabel (mixCap,  "Mix",  12.0f, false, theme::inkSoft, juce::Justification::centred);
    addLabel (trimCap, "Trim", 12.0f, false, theme::inkSoft, juce::Justification::centred);
    addLabel (mixMin,  "0",    10.0f, false, theme::inkSoft, juce::Justification::centredLeft);
    addLabel (mixMax,  "100",  10.0f, false, theme::inkSoft, juce::Justification::centredRight);
    addLabel (trimMin, "-18",  10.0f, false, theme::inkSoft, juce::Justification::centredLeft);
    addLabel (trimMax, "+18",  10.0f, false, theme::inkSoft, juce::Justification::centredRight);

    gateKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gateKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    gateKnob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                  juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (gateKnob);
    gateAtt = std::make_unique<SliderAtt> (proc.apvts, "gate", gateKnob);

    mixKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    mixKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    mixKnob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                 juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (mixKnob);
    mixAtt = std::make_unique<SliderAtt> (proc.apvts, "mix", mixKnob);

    trimKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    trimKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    trimKnob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                  juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (trimKnob);
    trimAtt = std::make_unique<SliderAtt> (proc.apvts, "trim", trimKnob);

    // ------------------------------------------------------------------
    // Baked plate: everything static lives on the chassis images. Captions
    // hide, pills become invisible hit areas, knobs paint steel dome sprites
    // (full 6-to-6 sweep so the plate ring wedges read the value), and the
    // curve display paints its dynamic overlay straight onto the baked glass.
    if (baked)
    {
        for (auto* c : std::initializer_list<juce::Component*> {
                 &brand, &brandSub, &threshCap, &gainCap, &inputCap, &outputCap,
                 &ratioCap, &modeCap, &attackCap, &releaseCap,
                 &gateCap, &mixCap, &trimCap, &mixMin, &mixMax, &trimMin, &trimMax,
                 &inMeterL, &inMeterR, &outMeterL, &outMeterR })
            c->setVisible (false);

        for (auto& b : modeBtns)
            b.setComponentID ("hit");

        for (auto* k : { &gateKnob, &mixKnob, &trimKnob })
        {
            k->setRotaryParameters (juce::MathConstants<float>::pi,
                                    juce::MathConstants<float>::pi * 3.0f, true);
            k->getProperties().set ("domeScale", 0.725);
        }

        curve.setPlateMode (true);
    }

    startTimerHz (30);
    setSize (baked ? 1024 : 820, baked ? 680 : 500);

    // License overlay sits on top of everything; it shows itself until activated.
    addChildComponent (licenseOverlay);
    licenseOverlay.setBounds (getLocalBounds());
}

VocalCompEditor::~VocalCompEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void VocalCompEditor::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

void VocalCompEditor::setMode (int mode)
{
    if (auto* m = proc.apvts.getParameter ("mode"))
        m->setValueNotifyingHost (m->getNormalisableRange().convertTo0to1 ((float) mode));
}

//==============================================================================
void VocalCompEditor::timerCallback()
{
    auto fmt1 = [] (float v) { return juce::String (v, 1); };
    // Show a clean "-∞" instead of a placeholder-looking "-100.0" when silent.
    auto fmtDb = [] (float v) { return v <= -60.0f ? juce::String (juce::CharPointer_UTF8 ("-\u221e"))
                                                   : juce::String (v, 1); };

    const float threshDb = proc.apvts.getRawParameterValue ("threshold")->load();
    threshVal.setText (fmt1 (threshDb), juce::dontSendNotification);
    gainVal.setText   (fmt1 (proc.apvts.getRawParameterValue ("makeup")->load()),
                       juce::dontSendNotification);
    attackVal.setText (fmt1 (proc.apvts.getRawParameterValue ("attack")->load()),
                       juce::dontSendNotification);
    releaseVal.setText (juce::String (juce::roundToInt (proc.apvts.getRawParameterValue ("release")->load())),
                        juce::dontSendNotification);

    const float gateDb = proc.apvts.getRawParameterValue ("gate")->load();
    gateVal.setText (gateDb <= -79.5f ? juce::String ("OFF") : juce::String (gateDb, 1),
                     juce::dontSendNotification);

    const float inL = proc.engine.meterInL.load(),  inR = proc.engine.meterInR.load();
    const float outL = proc.engine.meterOutL.load(), outR = proc.engine.meterOutR.load();
    inLVal.setText (fmtDb (inL),  juce::dontSendNotification);
    inRVal.setText (fmtDb (inR),  juce::dontSendNotification);
    outLVal.setText (fmtDb (outL), juce::dontSendNotification);
    outRVal.setText (fmtDb (outR), juce::dontSendNotification);

    inMeterL.setLevelDb (inL);
    inMeterR.setLevelDb (inR);
    outMeterL.setLevelDb (outL);
    outMeterR.setLevelDb (outR);

    inLDb = inL; inRDb = inR; outLDb = outL; outRDb = outR;

    curve.setGainReductionDb (proc.engine.meterGR.load());
    curve.setThresholdDb (threshDb);
    curve.setInputDb (juce::jmax (inL, inR));

    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    curve.setMode (mode);
    for (int i = 0; i < 3; ++i)
        modeBtns[(size_t) i].setToggleState (i == mode, juce::dontSendNotification);

    const int cur = proc.getCurrentProgram();
    if (presetBox.getSelectedId() != cur + 1)
        presetBox.setSelectedId (cur + 1, juce::dontSendNotification);

    if (chassisImg.isValid())
        repaint();   // meter fills, slider fills, pills + ring wedges live in paintPlate
}

//==============================================================================
juce::Rectangle<int> VocalCompEditor::plateFracRect (float fx, float fy, float fw, float fh) const
{
    return juce::Rectangle<float> (fx * (float) getWidth(),  fy * (float) getHeight(),
                                   fw * (float) getWidth(),  fh * (float) getHeight())
               .toNearestInt();
}

// Blit the matching region of the lit plate over the base plate — pixel
// registration is guaranteed because both images share the same canvas.
void VocalCompEditor::maskFromOn (juce::Graphics& g, juce::Rectangle<int> screenRect)
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
void VocalCompEditor::maskFromOnFeathered (juce::Graphics& g, juce::Rectangle<int> screenRect,
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
void VocalCompEditor::drawRingWedge (juce::Graphics& g, juce::Slider& s, float cxFrac, float cyFrac,
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

void VocalCompEditor::paintPlate (juce::Graphics& g)
{
    using namespace plategeo;

    g.drawImage (chassisImg, getLocalBounds().toFloat(),
                 juce::RectanglePlacement::stretchToFit);

    // ---- active mode pill lights up (ON plate has all three lit)
    if (auto* pm = proc.apvts.getRawParameterValue ("mode"))
    {
        const int mode = juce::jlimit (0, 2, (int) pm->load());
        maskFromOnFeathered (g, plateFracRect (pillX[mode][0], pillY0,
                                               pillX[mode][1] - pillX[mode][0],
                                               pillY1 - pillY0),
                             juce::roundToInt ((float) getWidth() * 0.010f));
    }

    auto propOf = [] (juce::Slider& s)
    {
        return juce::jlimit (0.0f, 1.0f,
                             (float) s.valueToProportionOfLength (s.getValue()));
    };

    // ---- vertical slider fills: bottom-up to the thumb
    for (auto& [s, cx] : { std::pair<juce::Slider*, float> { &threshSlider, threshCx },
                           std::pair<juce::Slider*, float> { &gainSlider,   gainCx } })
    {
        const float pr = propOf (*s);
        if (pr <= 0.004f) continue;
        const float yTop = vSlY1 - pr * (vSlY1 - vSlY0);
        maskFromOn (g, plateFracRect (cx - vFillHalfW, yTop,
                                      vFillHalfW * 2.0f, vSlY1 - yTop));
    }

    // ---- meter channels: bottom-up pink fill
    auto meterMask = [&] (float cx, float db)
    {
        const float pr = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        if (pr <= 0.004f) return;
        const float yTop = mY1 - pr * (mY1 - mY0);
        maskFromOn (g, plateFracRect (cx - mHalfW, yTop, mHalfW * 2.0f, mY1 - yTop));
    };
    meterMask (inLCx,  inLDb);
    meterMask (inRCx,  inRDb);
    meterMask (outLCx, outLDb);
    meterMask (outRCx, outRDb);

    // ---- attack / release fills: left-to-right to the thumb
    for (auto& [s, cy] : { std::pair<juce::Slider*, float> { &attackSlider,  attackCy },
                           std::pair<juce::Slider*, float> { &releaseSlider, releaseCy } })
    {
        const float pr = propOf (*s);
        if (pr <= 0.004f) continue;
        maskFromOn (g, plateFracRect (hX0, cy - hFillHalfH,
                                      (hX1 - hX0) * pr, hFillHalfH * 2.0f));
    }

    // ---- gate / mix / trim neon ring wedges
    drawRingWedge (g, gateKnob, gateCx, knobCy, ringDomeR, ringSolidR, ringMaxR);
    drawRingWedge (g, mixKnob,  mixCx,  knobCy, ringDomeR, ringSolidR, ringMaxR);
    drawRingWedge (g, trimKnob, trimCx, knobCy, ringDomeR, ringSolidR, ringMaxR);
}

//==============================================================================
void VocalCompEditor::paint (juce::Graphics& g)
{
    if (chassisImg.isValid())
    {
        paintPlate (g);
        return;
    }

    // warm off-white gradient backdrop + soft top light
    theme::backdrop (g, getLocalBounds());

    // floating-card helper: ambient elevation + fill + top highlight + hairline
    auto card = [&] (juce::Rectangle<int> r, float radius)
    {
        auto rf = r.toFloat();
        theme::elevate (g, rf, radius);
        g.setColour (theme::card);
        g.fillRoundedRectangle (rf, radius);
        theme::topHighlight (g, rf, radius);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (rf, radius, 1.0f);
    };

    // ---- two-tone wordmark + accent underline ----
    {
        auto wm = theme::font (23.0f, true);
        g.setFont (wm);
        auto br = brand.getBounds().toFloat();
        const float vw = juce::GlyphArrangement::getStringWidth (wm, "vocal");
        g.setColour (theme::ink);
        g.drawText ("vocal", br, juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.drawText ("comp", br.withTrimmedLeft (vw), juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.fillRoundedRectangle (br.getX(), br.getBottom() - 3.0f, 20.0f, 2.5f, 1.25f);
    }
    theme::spacedText (g, "VOCAL COMPRESSOR", brandSub.getBounds().toFloat(),
                       theme::inkSoft, 9.5f, 2.4f, false, juce::Justification::centredLeft);

    // main floating card holding all controls
    card (mainCard, 18.0f);

    // mood: a faint accent bloom behind the centre transfer-curve card
    theme::accentBloom (g, curveCard.toFloat().getCentre(),
                        (float) curveCard.getWidth() * 0.55f, 0.05f);

    // centre ratio sub-card (the CurveDisplay carves its own recessed face inside)
    card (curveCard, 14.0f);

    // hairline divider above the bottom strip
    g.setColour (theme::cardLine);
    g.drawLine ((float) mainCard.getX() + 20.0f, (float) bottomStrip.getY() - 7.0f,
                (float) mainCard.getRight() - 20.0f, (float) bottomStrip.getY() - 7.0f, 1.0f);

    // mode pill container: a clean recessed track the pills sit in
    {
        auto mc = modeContainer.toFloat();
        const float r = mc.getHeight() * 0.5f;
        theme::recess (g, mc, r);
    }
}

//==============================================================================
void VocalCompEditor::resized()
{
    licenseOverlay.setBounds (getLocalBounds());

    if (chassisImg.isValid())
    {
        using namespace plategeo;
        const float W = (float) getWidth();
        const float H = (float) getHeight();

        auto knobRect = [&] (float cx, float cy, float sideFracW)
        {
            const int side = juce::roundToInt (sideFracW * W);
            return juce::Rectangle<int> (juce::roundToInt (cx * W) - side / 2,
                                         juce::roundToInt (cy * H) - side / 2,
                                         side, side);
        };

        // vertical sliders: bounds sized so the JUCE thumb travel (inset by
        // the fixed 12px V4 thumb radius) matches the baked groove exactly
        const int tr = 12;   // LookAndFeel_V4 thumb radius (jmin 12)
        threshSlider.setBounds (juce::roundToInt (threshCx * W - 14.0f),
                                juce::roundToInt (vSlY0 * H) - tr,
                                28, juce::roundToInt ((vSlY1 - vSlY0) * H) + tr * 2);
        gainSlider.setBounds   (juce::roundToInt (gainCx * W - 14.0f),
                                juce::roundToInt (vSlY0 * H) - tr,
                                28, juce::roundToInt ((vSlY1 - vSlY0) * H) + tr * 2);

        // attack / release horizontal sliders
        attackSlider.setBounds  (juce::roundToInt (hX0 * W) - tr,
                                 juce::roundToInt (attackCy * H - 13.0f),
                                 juce::roundToInt ((hX1 - hX0) * W) + tr * 2, 26);
        releaseSlider.setBounds (juce::roundToInt (hX0 * W) - tr,
                                 juce::roundToInt (releaseCy * H - 13.0f),
                                 juce::roundToInt ((hX1 - hX0) * W) + tr * 2, 26);

        // live values above the grooves (captions are baked)
        threshVal.setBounds (plateFracRect (threshCx - 0.055f, valY0, 0.110f, valY1 - valY0));
        gainVal.setBounds   (plateFracRect (gainCx   - 0.055f, valY0, 0.110f, valY1 - valY0));

        // -∞ readouts under the IN / OUT captions
        inLVal.setBounds  (plateFracRect ((inLCx + threshCx) * 0.5f - 0.026f, readY0,
                                          0.052f, readY1 - readY0));
        inRVal.setBounds  (plateFracRect ((inRCx + threshCx) * 0.5f - 0.026f, readY0,
                                          0.052f, readY1 - readY0));
        outLVal.setBounds (plateFracRect ((outLCx + gainCx) * 0.5f - 0.026f, readY0,
                                          0.052f, readY1 - readY0));
        outRVal.setBounds (plateFracRect ((outRCx + gainCx) * 0.5f - 0.026f, readY0,
                                          0.052f, readY1 - readY0));

        // attack / release values right of the grooves
        attackVal.setBounds  (plateFracRect (arValX0, attackCy - 0.017f,
                                             arValX1 - arValX0, 0.034f));
        releaseVal.setBounds (plateFracRect (arValX0, releaseCy - 0.017f,
                                             arValX1 - arValX0, 0.034f));
        attackVal.setJustificationType (juce::Justification::centred);
        releaseVal.setJustificationType (juce::Justification::centred);

        // curve overlay clipped to the baked smoked-glass rect
        curve.setBounds (plateFracRect (dispX0, dispY0, dispX1 - dispX0, dispY1 - dispY0));

        // mode pills: invisible hit areas over the baked pills (seam-to-seam)
        for (int i = 0; i < 3; ++i)
            modeBtns[(size_t) i].setBounds (plateFracRect (pillX[i][0], pillY0,
                                                           pillX[i][1] - pillX[i][0],
                                                           pillY1 - pillY0));

        // gate / mix / trim: dome sprites over the baked seats. Bounds exceed
        // the drawn dome (domeScale) so the tiny knobs stay grabbable.
        const float knobSide = knobDomeDia / skin::croppedDomeFrac() / 0.725f;
        gateKnob.setBounds (knobRect (gateCx, knobCy, knobSide));
        mixKnob.setBounds  (knobRect (mixCx,  knobCy, knobSide));
        trimKnob.setBounds (knobRect (trimCx, knobCy, knobSide));

        // gate readout shares the row with the baked 0/100 range marks
        gateVal.setBounds (plateFracRect (gateCx - 0.045f, gateValY0,
                                          0.090f, gateValY1 - gateValY0));

        presetBox.setBounds (plateFracRect (boxX0, boxY0, boxX1 - boxX0, boxY1 - boxY0));
        return;
    }

    auto bounds = getLocalBounds();
    auto outer = bounds.reduced (20, 16);

    // ---- top bar (on the bare background, above the card) ----
    auto top = outer.removeFromTop (36);
    brand.setBounds (top.getX(), top.getY() + 3, 150, 30);
    brandSub.setBounds (top.getX() + 138, top.getY() + 11, 170, 18);

    const int comboW = 184, comboH = 30;
    presetBox.setBounds (top.getRight() - comboW, top.getY() + 3, comboW, comboH);

    outer.removeFromTop (10);

    // ---- main card ----
    mainCard = outer;
    auto inner = mainCard.reduced (20, 16);

    // bottom strip reserved first so the columns size to the remaining space
    const int stripH = 92;
    bottomStrip = inner.removeFromBottom (stripH);
    inner.removeFromBottom (14);   // gap for the divider
    auto strip = bottomStrip;

    // ---- three columns (threshold | ratio | gain) ----
    const int colW = 92;
    auto leftCol  = inner.removeFromLeft (colW);
    auto rightCol = inner.removeFromRight (colW);
    inner.removeFromLeft (16);
    inner.removeFromRight (16);
    auto centre = inner;

    auto layoutSideColumn = [] (juce::Rectangle<int> col,
                                juce::Label& cap, juce::Label& val,
                                juce::Slider& slider, MeterBar& mL, MeterBar& mR,
                                juce::Label& readCap, juce::Label& readL, juce::Label& readR)
    {
        cap.setBounds (col.removeFromTop (14));
        val.setBounds (col.removeFromTop (24));

        // compact readout block pinned to the bottom
        auto read = col.removeFromBottom (30);
        readCap.setBounds (read.removeFromTop (13));
        readL.setBounds (read.removeFromLeft (read.getWidth() / 2));
        readR.setBounds (read);

        col.removeFromTop (6);
        col.removeFromBottom (4);

        // vertical slider centred, meter bars hugging it on both sides
        const int cx = col.getCentreX();
        const int barW = 5, barGap = 8, sliderW = 26;
        slider.setBounds (cx - sliderW / 2, col.getY(), sliderW, col.getHeight());
        mL.setBounds (cx - sliderW / 2 - barGap - barW, col.getY(), barW, col.getHeight());
        mR.setBounds (cx + sliderW / 2 + barGap,        col.getY(), barW, col.getHeight());
    };

    layoutSideColumn (leftCol, threshCap, threshVal, threshSlider, inMeterL, inMeterR,
                      inputCap, inLVal, inRVal);
    layoutSideColumn (rightCol, gainCap, gainVal, gainSlider, outMeterL, outMeterR,
                      outputCap, outLVal, outRVal);

    // ---- centre ratio card (transfer-curve hero) ----
    ratioCap.setBounds (centre.removeFromTop (14));
    centre.removeFromTop (3);
    curveCard = centre;
    curve.setBounds (curveCard.reduced (8));

    // ---- bottom strip: mode pills | attack/release | gate/mix/trim ----
    auto modeArea = strip.removeFromLeft (216);
    auto knobArea = strip.removeFromRight (264);
    auto arArea   = strip.reduced (20, 0);

    // mode pills (left), vertically centred
    {
        const int capH = 13, contH = 38;
        const int blockY = strip.getY() + (stripH - (capH + 5 + contH)) / 2;
        modeCap.setBounds (modeArea.getX() + 2, blockY, 80, capH);
        modeContainer = juce::Rectangle<int> (modeArea.getX(), blockY + capH + 5,
                                              modeArea.getWidth() - 8, contH);
        auto pills = modeContainer.reduced (6, 6);
        const int pillGap = 6;
        const int pillW = (pills.getWidth() - pillGap * 2) / 3;
        for (int i = 0; i < 3; ++i)
            modeBtns[(size_t) i].setBounds (pills.getX() + i * (pillW + pillGap), pills.getY(),
                                            pillW, pills.getHeight());
    }

    // attack / release (centre), two centred rows
    {
        const int rowH = 26, rowGap = 12;
        const int labelW = 58, valW = 46;
        const int blockY = strip.getY() + (stripH - (rowH * 2 + rowGap)) / 2;
        auto row1 = juce::Rectangle<int> (arArea.getX(), blockY, arArea.getWidth(), rowH);
        auto row2 = juce::Rectangle<int> (arArea.getX(), blockY + rowH + rowGap, arArea.getWidth(), rowH);

        attackCap.setBounds (row1.removeFromLeft (labelW));
        attackVal.setBounds (row1.removeFromRight (valW));
        attackSlider.setBounds (row1.reduced (8, 0));

        releaseCap.setBounds (row2.removeFromLeft (labelW));
        releaseVal.setBounds (row2.removeFromRight (valW));
        releaseSlider.setBounds (row2.reduced (8, 0));
    }

    // gate / mix / trim knobs (right), vertically centred
    {
        constexpr int knobSize = 50;
        const int capH = 14, rangeH = 14;
        const int blockY = strip.getY() + (stripH - (capH + knobSize + rangeH)) / 2;
        const int third = knobArea.getWidth() / 3;
        auto gateCell = knobArea.removeFromLeft (third);
        auto mixCell  = knobArea.removeFromLeft (third);
        auto trimCell = knobArea;

        auto placeKnob = [blockY] (juce::Rectangle<int> cell, juce::Label& cap,
                                   juce::Slider& knob, juce::Label& lo, juce::Label& hi)
        {
            const int cx = cell.getCentreX();
            cap.setBounds (cx - 50, blockY, 100, capH);
            knob.setBounds (cx - knobSize / 2, blockY + capH, knobSize, knobSize);
            lo.setBounds (cx - knobSize / 2 - 6, knob.getBottom() - 1, 26, 13);
            hi.setBounds (cx + knobSize / 2 - 20, knob.getBottom() - 1, 26, 13);
        };

        // Gate shows a single centred live readout (OFF / dB) instead of min/max.
        {
            const int cx = gateCell.getCentreX();
            gateCap.setBounds (cx - 50, blockY, 100, capH);
            gateKnob.setBounds (cx - knobSize / 2, blockY + capH, knobSize, knobSize);
            gateVal.setBounds (cx - 40, gateKnob.getBottom() - 1, 80, 13);
        }

        placeKnob (mixCell,  mixCap,  mixKnob,  mixMin,  mixMax);
        placeKnob (trimCell, trimCap, trimKnob, trimMin, trimMax);
    }
}
