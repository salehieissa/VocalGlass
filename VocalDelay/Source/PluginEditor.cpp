#include "PluginEditor.h"

namespace
{
    const juce::String kPhiL = juce::String (juce::CharPointer_UTF8 ("\xC3\x98L")); // "ØL"
    const juce::String kPhiR = juce::String (juce::CharPointer_UTF8 ("\xC3\x98R")); // "ØR"
    const std::array<juce::String, 4> kModeWords { kPhiL, "PING PONG", "DUAL", kPhiR };
    const std::array<juce::String, 3> kSyncWords { "BPM", "HOST", "MS" };

    juce::String hzText (float hz)
    {
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " kHz"
                             : juce::String (juce::roundToInt (hz)) + " Hz";
    }
}

//==============================================================================
// Baked-plate geometry, measured on the 2048x1360 chassis plates (ON-vs-OFF
// diff components, radial pink ring profiles, groove scans). Values are raw
// image pixels converted to canvas fractions.
namespace plategeo
{
    constexpr float px (float v) { return v / 2048.0f; }
    constexpr float py (float v) { return v / 1360.0f; }

    // tap pad + LoFi capsule
    constexpr float tapX0 = px (109.0f),  tapX1 = px (341.0f);
    constexpr float tapY0 = py (289.0f),  tapY1 = py (543.0f);
    constexpr float lofiX0 = px (116.0f), lofiX1 = px (322.0f);
    constexpr float lofiY0 = py (703.0f), lofiY1 = py (777.0f);

    // large knobs (delay / feedback): ring solid band 98..120, bloom to 132
    constexpr float delCx = px (543.6f),   delCy = py (380.9f);
    constexpr float fbCx  = px (1451.4f),  fbCy  = py (381.7f);
    constexpr float bigDomeDia = px (192.0f);
    constexpr float bigDomeR = px (94.0f), bigSolidR = px (120.0f), bigMaxR = px (132.0f);
    constexpr float delValX0 = px (430.0f),  delValX1 = px (660.0f);
    constexpr float fbValX0  = px (1340.0f), fbValX1  = px (1570.0f);
    constexpr float bigValY0 = py (546.0f),  bigValY1 = py (592.0f);

    // mode capsules (ØL / PING PONG / DUAL / ØR)
    constexpr float modeX0[4] = { px (739.0f), px (855.0f), px (1046.0f), px (1189.0f) };
    constexpr float modeX1[4] = { px (835.0f), px (1030.0f), px (1169.0f), px (1283.0f) };
    constexpr float modeY0 = py (222.0f), modeY1 = py (286.0f);

    // sync capsules (BPM / HOST / MS)
    constexpr float syncX0[3] = { px (738.0f), px (921.0f), px (1108.0f) };
    constexpr float syncX1[3] = { px (902.0f), px (1089.0f), px (1271.0f) };
    constexpr float syncY0 = py (629.0f), syncY1 = py (698.0f);

    // centre display card (surface baked; number + unit drawn live)
    constexpr float cardX0 = px (718.0f), cardX1 = px (1332.0f);
    constexpr float cardY0 = py (298.0f), cardY1 = py (590.0f);

    // stereo meter grooves
    constexpr float metX0 = px (1628.0f), metX1 = px (1737.0f);
    constexpr float metY0 = py (242.0f),  metY1 = py (815.0f);
    constexpr float barLX0 = 1628.0f, barLX1 = 1670.0f;   // raw px (used for blits)
    constexpr float barRX0 = 1695.0f, barRX1 = 1737.0f;
    constexpr float barYT = 242.0f,  barYB = 815.0f;

    // small knobs (depth / rate / hipass / lopass): solid 51..61, bloom 71
    constexpr float depCx = px (465.9f),  depCy = py (906.3f);
    constexpr float ratCx = px (686.6f),  ratCy = py (907.1f);
    constexpr float hipCx = px (982.8f),  hipCy = py (908.0f);
    constexpr float lopCx = px (1357.3f), lopCy = py (908.2f);
    constexpr float smallDomeDia = px (98.0f);
    constexpr float smallDomeR = px (47.0f), smallSolidR = px (61.0f), smallMaxR = px (71.0f);
    constexpr float smallValY0 = py (998.0f), smallValY1 = py (1044.0f);
    constexpr float depValX0 = px (400.0f),  depValX1 = px (533.0f);
    constexpr float ratValX0 = px (620.0f),  ratValX1 = px (754.0f);
    constexpr float hipValX0 = px (915.0f),  hipValX1 = px (1050.0f);
    constexpr float lopValX0 = px (1290.0f), lopValX1 = px (1425.0f);

    // LINK round button
    constexpr float linkCx = px (1170.9f), linkCy = py (932.9f);
    constexpr float linkR = px (50.0f);

    // right column knobs (dry/wet / output / analog): solid 37..44, bloom 54
    constexpr float dwCx = px (1888.7f), dwCy = py (313.8f);
    constexpr float outCx = px (1888.9f), outCy = py (607.1f);
    constexpr float anCx = px (1889.2f), anCy = py (894.4f);
    constexpr float tinyDomeDia = px (70.0f);
    constexpr float tinyDomeR = px (33.0f), tinySolidR = px (44.0f), tinyMaxR = px (54.0f);
    constexpr float rightValX0 = px (1835.0f), rightValX1 = px (1945.0f);
    constexpr float dwValY0 = py (392.0f),  dwValY1 = py (434.0f);
    constexpr float outValY0 = py (662.0f), outValY1 = py (702.0f);
    constexpr float anValY0 = py (976.0f),  anValY1 = py (1014.0f);
}

//==============================================================================
VocalDelayEditor::VocalDelayEditor (VocalDelayProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);

    chassisImg   = skin::image ("delay-chassis@2x.png");
    chassisOnImg = skin::image ("delay-chassis-on@2x.png");
    plateBaked = chassisImg.isValid() && chassisOnImg.isValid();

    // ---- branding ----
    // The wordmark is painted as a two-tone "vocal"+"delay" in paint(); the
    // label keeps its slot in the layout but draws nothing.
    brand.setText ({}, juce::dontSendNotification);
    brand.setFont (theme::font (26.0f, true));
    brand.setColour (juce::Label::textColourId, theme::ink);
    addAndMakeVisible (brand);

    brandSub.setText ("VOCAL DELAY", juce::dontSendNotification);
    brandSub.setFont (theme::font (12.0f, false));
    brandSub.setColour (juce::Label::textColourId, theme::inkSoft);
    addAndMakeVisible (brandSub);

    // ---- far-left column ----
    tapButton.onClick = [this] { registerTap(); };
    addAndMakeVisible (tapButton);

    tapLabel.setText ("TAP", juce::dontSendNotification);
    tapLabel.setJustificationType (juce::Justification::centred);
    tapLabel.setFont (theme::font (15.0f, false));
    tapLabel.setColour (juce::Label::textColourId, theme::ink);
    addAndMakeVisible (tapLabel);

    lofiBtn.setClickingTogglesState (true);
    addAndMakeVisible (lofiBtn);
    lofiAtt = std::make_unique<ButtonAtt> (proc.apvts, "lofi", lofiBtn);

    // ---- knobs + attachments ----
    auto addKnob = [this] (LabeledKnob& k, const char* id, int slot)
    {
        addAndMakeVisible (k);
        knobAtt[(size_t) slot] = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
    };
    addKnob (feedbackKnob, "feedback", 0);
    addKnob (depthKnob,    "depth",    1);
    addKnob (rateKnob,     "rate",     2);
    addKnob (hipassKnob,   "hipass",   3);
    addKnob (lopassKnob,   "lopass",   4);
    addKnob (drywetKnob,   "drywet",   5);
    addKnob (outputKnob,   "output",   6);
    addAndMakeVisible (analogKnob);
    analogAtt = std::make_unique<SliderAtt> (proc.apvts, "analog", analogKnob.slider);

    // The big delay knob controls the musical division when synced, or the free
    // millisecond time in MS mode — its attachment is swapped to match.
    addAndMakeVisible (delayKnob);
    delayAttId = "division";
    delayAtt = std::make_unique<SliderAtt> (proc.apvts, delayAttId, delayKnob.slider);

    // ---- mode segmented ----
    for (int i = 0; i < 4; ++i)
    {
        auto& b = modeBtns[(size_t) i];
        b.setButtonText (kModeWords[(size_t) i]);
        b.setClickingTogglesState (false);
        b.onClick = [this, i] { setChoiceParam ("mode", i); };
        addAndMakeVisible (b);
    }

    // ---- sync segmented ----
    for (int i = 0; i < 3; ++i)
    {
        auto& b = syncBtns[(size_t) i];
        b.setButtonText (kSyncWords[(size_t) i]);
        b.setClickingTogglesState (false);
        b.onClick = [this, i] { setChoiceParam ("syncMode", i); };
        addAndMakeVisible (b);
    }

    // ---- centre display + meters ----
    addAndMakeVisible (display);
    addAndMakeVisible (meter);

    // ---- filters link ----
    addAndMakeVisible (linkBtn);
    linkAtt = std::make_unique<ButtonAtt> (proc.apvts, "filterLink", linkBtn);

    linkLabel.setText ("LINK", juce::dontSendNotification);
    linkLabel.setJustificationType (juce::Justification::centred);
    linkLabel.setFont (theme::font (11.0f, false));
    linkLabel.setColour (juce::Label::textColourId, theme::inkSoft);
    addAndMakeVisible (linkLabel);

    auto linkFrom = [this] (juce::Slider& from, juce::Slider& to)
    {
        if (! linkActive || linkGuard) return;
        linkGuard = true;
        const double prop = from.valueToProportionOfLength (from.getValue());
        to.setValue (to.proportionOfLengthToValue (prop), juce::sendNotificationSync);
        linkGuard = false;
    };
    hipassKnob.slider.onValueChange = [this, linkFrom] { linkFrom (hipassKnob.slider, lopassKnob.slider); };
    lopassKnob.slider.onValueChange = [this, linkFrom] { linkFrom (lopassKnob.slider, hipassKnob.slider); };

    // ---- section captions ----
    for (auto* l : { &modSection, &filtSection })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setFont (theme::font (11.0f, false));
        l->setColour (juce::Label::textColourId, theme::inkSoft);
        addAndMakeVisible (*l);
    }
    modSection.setText ("MODULATION", juce::dontSendNotification);
    filtSection.setText ("FILTERS", juce::dontSendNotification);

    if (plateBaked)
        setupPlateMode();

    startTimerHz (24);
    // plate mode: the window shows ONLY the plate (cropped at the chrome edge),
    // and must match the crop's aspect exactly or circular dome sprites sit in
    // vertically-stretched (elliptical) baked grooves
    if (plateBaked)
        setSize (1024, juce::roundToInt (1024.0f * (float) plateCrop.getHeight()
                                                 / (float) plateCrop.getWidth()));
    else
        setSize (1024, 640);

    // License overlay sits on top of everything; it shows itself until activated.
    addChildComponent (licenseOverlay);
    licenseOverlay.setBounds (getLocalBounds());
}

VocalDelayEditor::~VocalDelayEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void VocalDelayEditor::setChoiceParam (const char* id, int value)
{
    if (auto* param = proc.apvts.getParameter (id))
        param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 ((float) value));
}

void VocalDelayEditor::registerTap()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (! tapTimes.empty() && now - tapTimes.back() > 2000.0)
        tapTimes.clear();

    tapTimes.push_back (now);
    while (tapTimes.size() > 4) tapTimes.erase (tapTimes.begin());

    if (tapTimes.size() >= 2)
    {
        double sum = 0.0;
        for (size_t i = 1; i < tapTimes.size(); ++i)
            sum += tapTimes[i] - tapTimes[i - 1];
        const double avg = sum / (double) (tapTimes.size() - 1);
        if (avg > 1.0)
        {
            const float bpm = juce::jlimit (40.0f, 300.0f, (float) (60000.0 / avg));
            if (auto* param = proc.apvts.getParameter ("bpm"))
                param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (bpm));
            setChoiceParam ("syncMode", 0); // tap implies internal BPM
        }
    }
}

//==============================================================================
void VocalDelayEditor::timerCallback()
{
    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    const int sync = (int) proc.apvts.getRawParameterValue ("syncMode")->load();
    linkActive = proc.apvts.getRawParameterValue ("filterLink")->load() > 0.5f;

    for (int i = 0; i < 4; ++i)
        modeBtns[(size_t) i].setToggleState (i == mode, juce::dontSendNotification);
    for (int i = 0; i < 3; ++i)
        syncBtns[(size_t) i].setToggleState (i == sync, juce::dontSendNotification);

    // swap the delay-knob attachment between division (synced) and msTime (free)
    const juce::String wantId = (sync == 2) ? "msTime" : "division";
    if (wantId != delayAttId)
    {
        delayAtt.reset();
        delayAttId = wantId;
        delayAtt = std::make_unique<SliderAtt> (proc.apvts, delayAttId, delayKnob.slider);
    }

    // knob value read-outs
    delayKnob.setValueText (proc.getDivisionText());
    feedbackKnob.setValueText (juce::String (juce::roundToInt (proc.apvts.getRawParameterValue ("feedback")->load())) + "%");
    depthKnob.setValueText (juce::String (juce::roundToInt (proc.apvts.getRawParameterValue ("depth")->load())) + "%");
    rateKnob.setValueText (juce::String (proc.apvts.getRawParameterValue ("rate")->load(), 2) + " Hz");
    hipassKnob.setValueText (hzText (proc.apvts.getRawParameterValue ("hipass")->load()));
    lopassKnob.setValueText (hzText (proc.apvts.getRawParameterValue ("lopass")->load()));
    drywetKnob.setValueText (juce::String (juce::roundToInt (proc.apvts.getRawParameterValue ("drywet")->load())) + "%");
    outputKnob.setValueText (juce::String (proc.apvts.getRawParameterValue ("output")->load(), 1) + " dB");
    analogKnob.setValueText (juce::String (juce::roundToInt (proc.apvts.getRawParameterValue ("analog")->load())));

    // centre display
    if (sync == 2)
    {
        const float ms = proc.getActiveDelayMs();
        display.setBigText (juce::String (ms, ms < 100.0f ? 1 : 0));
        display.setUnitText ("MS");
    }
    else
    {
        display.setBigText (juce::String (juce::roundToInt (proc.getActiveBpm())));
        display.setUnitText ("BPM");
    }
    display.setTaps (proc.apvts.getRawParameterValue ("feedback")->load() * 0.01f);

    if (! plateBaked)
        return;

    // dirty-region repaints: only invalidate what actually changed this tick
    using namespace plategeo;
    struct KnobRegion { LabeledKnob* k; float cx, cy, maxR; juce::Rectangle<int> val; };
    const KnobRegion regions[] = {
        { &delayKnob,    delCx, delCy, bigMaxR, plateFracRect (delValX0, bigValY0, delValX1, bigValY1) },
        { &feedbackKnob, fbCx,  fbCy,  bigMaxR, plateFracRect (fbValX0,  bigValY0, fbValX1,  bigValY1) },
        { &depthKnob,  depCx, depCy, smallMaxR, plateFracRect (depValX0, smallValY0, depValX1, smallValY1) },
        { &rateKnob,   ratCx, ratCy, smallMaxR, plateFracRect (ratValX0, smallValY0, ratValX1, smallValY1) },
        { &hipassKnob, hipCx, hipCy, smallMaxR, plateFracRect (hipValX0, smallValY0, hipValX1, smallValY1) },
        { &lopassKnob, lopCx, lopCy, smallMaxR, plateFracRect (lopValX0, smallValY0, lopValX1, smallValY1) },
        { &drywetKnob, dwCx,  dwCy,  tinyMaxR, plateFracRect (rightValX0, dwValY0,  rightValX1, dwValY1) },
        { &outputKnob, outCx, outCy, tinyMaxR, plateFracRect (rightValX0, outValY0, rightValX1, outValY1) },
        { &analogKnob, anCx,  anCy,  tinyMaxR, plateFracRect (rightValX0, anValY0,  rightValX1, anValY1) },
    };
    // radius fractions are of image WIDTH; vertical extents need the aspect factor
    const float ar = (float) chassisImg.getWidth() / (float) chassisImg.getHeight();
    for (size_t i = 0; i < 9; ++i)
    {
        const double v = regions[i].k->slider.getValue();
        if (v != shownKnob[i])
        {
            shownKnob[i] = v;
            repaint (plateFracRect (regions[i].cx - regions[i].maxR, regions[i].cy - regions[i].maxR * ar,
                                    regions[i].cx + regions[i].maxR, regions[i].cy + regions[i].maxR * ar)
                         .expanded (2));
            repaint (regions[i].val.expanded (2));
        }
    }

    const int fpx = juce::roundToInt ((float) getWidth() * 0.008f) + 2;
    if (mode != shownMode)
    {
        if (shownMode >= 0)
            repaint (plateFracRect (modeX0[shownMode], modeY0, modeX1[shownMode], modeY1).expanded (fpx));
        shownMode = mode;
        repaint (plateFracRect (modeX0[mode], modeY0, modeX1[mode], modeY1).expanded (fpx));
    }
    if (sync != shownSync)
    {
        if (shownSync >= 0)
            repaint (plateFracRect (syncX0[shownSync], syncY0, syncX1[shownSync], syncY1).expanded (fpx));
        shownSync = sync;
        repaint (plateFracRect (syncX0[sync], syncY0, syncX1[sync], syncY1).expanded (fpx));
        // the delay value read-out switches format (division <-> ms) with sync
        repaint (plateFracRect (delValX0, bigValY0, delValX1, bigValY1).expanded (2));
    }
    if (tapButton.isDown() != shownTap)
    {
        shownTap = tapButton.isDown();
        repaint (plateFracRect (tapX0, tapY0, tapX1, tapY1).expanded (fpx));
    }
    if (lofiBtn.getToggleState() != shownLofi)
    {
        shownLofi = lofiBtn.getToggleState();
        repaint (plateFracRect (lofiX0, lofiY0, lofiX1, lofiY1).expanded (fpx));
    }
    if (linkBtn.getToggleState() != shownLink)
    {
        shownLink = linkBtn.getToggleState();
        repaint (plateFracRect (linkCx - linkR, linkCy - linkR * ar,
                                linkCx + linkR, linkCy + linkR * ar).expanded (fpx));
    }
}

//==============================================================================
// Baked-plate mode: static art comes from the OFF chassis plate; lit states
// are revealed by blitting the same regions from the pixel-registered ON
// plate. Knobs draw rotating chrome dome sprites; captions are baked, value
// read-outs drawn live.
void VocalDelayEditor::setupPlateMode()
{
    using namespace plategeo;

    laf.plate = true;
    plateCrop = skin::plateBounds (chassisImg);
    laf.domeLarge = skin::cropToDome (skin::image ("grit-knob-large@2x.png"),
                                      0.1999f, 0.3533f, 0.199f);
    laf.domeSmall = skin::cropToDome (skin::image ("grit-knob-small@2x.png"),
                                      0.4993f, 0.4648f, 0.615f);

    // knobs: dome sprites with a full-360 sweep from 6 o'clock
    LabeledKnob* large[] = { &delayKnob, &feedbackKnob };
    LabeledKnob* small[] = { &depthKnob, &rateKnob, &hipassKnob, &lopassKnob,
                             &drywetKnob, &outputKnob, &analogKnob };
    for (auto* k : large)
    {
        k->setPlate (true);
        k->slider.setComponentID ("dome-large");
    }
    for (auto* k : small)
    {
        k->setPlate (true);
        k->slider.setComponentID ("dome-small");
    }
    for (auto* k : { &delayKnob, &feedbackKnob, &depthKnob, &rateKnob, &hipassKnob,
                     &lopassKnob, &drywetKnob, &outputKnob, &analogKnob })
        k->slider.setRotaryParameters (juce::MathConstants<float>::pi,
                                       juce::MathConstants<float>::pi * 3.0f, true);

    // buttons become invisible hit areas; their lit states are masked in paintPlate
    for (auto& b : modeBtns) b.setComponentID ("hit");
    for (auto& b : syncBtns) b.setComponentID ("hit");
    lofiBtn.setComponentID ("hit");
    tapButton.plate = true;
    linkBtn.plate = true;

    // display card surface + meter grooves/ruler are baked
    display.plate = true;
    meter.setPlate (chassisOnImg,
                    { barLX0, barYT, barRX1 - barLX0, barYB - barYT },
                    { barLX0, barYT, barLX1 - barLX0, barYB - barYT },
                    { barRX0, barYT, barRX1 - barRX0, barYB - barYT });

    // captions baked into the chassis
    for (auto* l : { &brand, &brandSub, &tapLabel, &linkLabel, &modSection, &filtSection })
        l->setVisible (false);
}

// plategeo fractions are of the FULL generated canvas; the window shows only
// the plateCrop region, so map full-canvas fraction -> cropped screen px.
juce::Rectangle<int> VocalDelayEditor::plateFracRect (float fx0, float fy0, float fx1, float fy1) const
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
void VocalDelayEditor::maskFromOn (juce::Graphics& g, juce::Rectangle<int> screenRect)
{
    g.drawImage (plateOnScaled,
                 screenRect.getX(), screenRect.getY(), screenRect.getWidth(), screenRect.getHeight(),
                 screenRect.getX(), screenRect.getY(), screenRect.getWidth(), screenRect.getHeight());
}

// Same reveal but with a soft alpha ramp along the rect border, so the slight
// global tone drift between the two plates never shows as a hard rectangle.
void VocalDelayEditor::maskFromOnFeathered (juce::Graphics& g, juce::Rectangle<int> screenRect,
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
void VocalDelayEditor::drawRingWedge (juce::Graphics& g, juce::Slider& s, float cxFrac, float cyFrac,
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
        wedge (a0,            a0 + fIn * 0.5f, rIn, rOut, alpha * 0.33f);
        wedge (a0 + fIn * 0.5f, a0 + fIn,      rIn, rOut, alpha * 0.66f);
    }
}

void VocalDelayEditor::paintPlate (juce::Graphics& g)
{
    using namespace plategeo;

    g.drawImageAt (plateScaled, 0, 0);   // cached 1:1 blit — no per-frame rescale

    const int feather = juce::roundToInt ((float) getWidth() * 0.008f);

    // ---- selected mode / sync capsules light up
    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    const int sync = (int) proc.apvts.getRawParameterValue ("syncMode")->load();
    maskFromOnFeathered (g, plateFracRect (modeX0[mode], modeY0, modeX1[mode], modeY1), feather);
    maskFromOnFeathered (g, plateFracRect (syncX0[sync], syncY0, syncX1[sync], syncY1), feather);

    // ---- momentary / toggle buttons
    if (tapButton.isDown())
        maskFromOnFeathered (g, plateFracRect (tapX0, tapY0, tapX1, tapY1), feather);
    if (lofiBtn.getToggleState())
        maskFromOnFeathered (g, plateFracRect (lofiX0, lofiY0, lofiX1, lofiY1), feather);
    if (linkBtn.getToggleState())
    {
        const float aspect = 2048.0f / 1360.0f;
        maskFromOnFeathered (g, plateFracRect (linkCx - linkR, linkCy - linkR * aspect,
                                               linkCx + linkR, linkCy + linkR * aspect), feather);
    }

    // ---- knob neon ring wedges
    drawRingWedge (g, delayKnob.slider,    delCx, delCy, bigDomeR, bigSolidR, bigMaxR);
    drawRingWedge (g, feedbackKnob.slider, fbCx,  fbCy,  bigDomeR, bigSolidR, bigMaxR);
    drawRingWedge (g, depthKnob.slider,  depCx, depCy, smallDomeR, smallSolidR, smallMaxR);
    drawRingWedge (g, rateKnob.slider,   ratCx, ratCy, smallDomeR, smallSolidR, smallMaxR);
    drawRingWedge (g, hipassKnob.slider, hipCx, hipCy, smallDomeR, smallSolidR, smallMaxR);
    drawRingWedge (g, lopassKnob.slider, lopCx, lopCy, smallDomeR, smallSolidR, smallMaxR);
    drawRingWedge (g, drywetKnob.slider, dwCx,  dwCy,  tinyDomeR, tinySolidR, tinyMaxR);
    drawRingWedge (g, outputKnob.slider, outCx, outCy, tinyDomeR, tinySolidR, tinyMaxR);
    drawRingWedge (g, analogKnob.slider, anCx,  anCy,  tinyDomeR, tinySolidR, tinyMaxR);

    // ---- live value read-outs (captions are baked; values are dynamic ink)
    auto value = [&] (const LabeledKnob& k, float fx0, float fy0, float fx1, float fy1, float fontH)
    {
        g.setColour (theme::ink);
        g.setFont (theme::font (fontH, false));
        g.drawText (k.value.getText(), plateFracRect (fx0, fy0, fx1, fy1),
                    juce::Justification::centred);
    };
    value (delayKnob,    delValX0, bigValY0, delValX1, bigValY1, 16.0f);
    value (feedbackKnob, fbValX0,  bigValY0, fbValX1,  bigValY1, 16.0f);
    value (depthKnob,  depValX0, smallValY0, depValX1, smallValY1, 13.0f);
    value (rateKnob,   ratValX0, smallValY0, ratValX1, smallValY1, 13.0f);
    value (hipassKnob, hipValX0, smallValY0, hipValX1, smallValY1, 13.0f);
    value (lopassKnob, lopValX0, smallValY0, lopValX1, smallValY1, 13.0f);
    value (drywetKnob, rightValX0, dwValY0,  rightValX1, dwValY1,  13.0f);
    value (outputKnob, rightValX0, outValY0, rightValX1, outValY1, 13.0f);
    value (analogKnob, rightValX0, anValY0,  rightValX1, anValY1,  13.0f);
}

void VocalDelayEditor::layoutPlate()
{
    using namespace plategeo;

    // rebuild the scaled plate caches for the new size (1:1 blits per frame)
    plateScaled   = skin::renderPlate (chassisImg,   plateCrop, getWidth(), getHeight());
    plateOnScaled = skin::renderPlate (chassisOnImg, plateCrop, getWidth(), getHeight());

    auto fr = [this] (float fx0, float fy0, float fx1, float fy1)
    {
        return plateFracRect (fx0, fy0, fx1, fy1);
    };
    const float iw = (float) chassisImg.getWidth(), ih = (float) chassisImg.getHeight();
    const float sx = (float) getWidth()  / (float) plateCrop.getWidth();
    const float sy = (float) getHeight() / (float) plateCrop.getHeight();
    const float aspect = 2048.0f / 1360.0f;

    tapButton.setBounds (fr (tapX0, tapY0, tapX1, tapY1));
    tapLabel.setBounds (0, 0, 0, 0);
    lofiBtn.setBounds (fr (lofiX0, lofiY0, lofiX1, lofiY1));

    auto domeSquare = [&] (float cx, float cy, float diaFrac)
    {
        const float side = diaFrac * iw * sx * 1.06f;
        const float px = (cx * iw - (float) plateCrop.getX()) * sx;
        const float py2 = (cy * ih - (float) plateCrop.getY()) * sy;
        return juce::Rectangle<float> (px - side * 0.5f, py2 - side * 0.5f,
                                       side, side).toNearestInt();
    };
    delayKnob.setBounds    (domeSquare (delCx, delCy, bigDomeDia));
    feedbackKnob.setBounds (domeSquare (fbCx,  fbCy,  bigDomeDia));
    depthKnob.setBounds  (domeSquare (depCx, depCy, smallDomeDia));
    rateKnob.setBounds   (domeSquare (ratCx, ratCy, smallDomeDia));
    hipassKnob.setBounds (domeSquare (hipCx, hipCy, smallDomeDia));
    lopassKnob.setBounds (domeSquare (lopCx, lopCy, smallDomeDia));
    drywetKnob.setBounds (domeSquare (dwCx, dwCy, tinyDomeDia));
    outputKnob.setBounds (domeSquare (outCx, outCy, tinyDomeDia));
    analogKnob.setBounds (domeSquare (anCx, anCy, tinyDomeDia));

    for (int i = 0; i < 4; ++i)
        modeBtns[(size_t) i].setBounds (fr (modeX0[i], modeY0, modeX1[i], modeY1));
    for (int i = 0; i < 3; ++i)
        syncBtns[(size_t) i].setBounds (fr (syncX0[i], syncY0, syncX1[i], syncY1));

    display.setBounds (fr (cardX0, cardY0, cardX1, cardY1));
    meter.setBounds (fr (metX0, metY0, metX1, metY1));

    linkBtn.setBounds (fr (linkCx - linkR, linkCy - linkR * aspect,
                           linkCx + linkR, linkCy + linkR * aspect));
    linkLabel.setBounds (0, 0, 0, 0);

    brand.setBounds (0, 0, 0, 0);
    brandSub.setBounds (0, 0, 0, 0);
    modSection.setBounds (0, 0, 0, 0);
    filtSection.setBounds (0, 0, 0, 0);
}

//==============================================================================
void VocalDelayEditor::paint (juce::Graphics& g)
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

    // two-tone wordmark: ink "vocal" + accent "delay" with an accent underline
    {
        auto wm = theme::font (26.0f, true);
        g.setFont (wm);
        const float vw = juce::GlyphArrangement::getStringWidth (wm, "vocal");
        g.setColour (theme::ink);
        g.drawText ("vocal", 28, 22, 240, 34, juce::Justification::left);
        g.setColour (theme::accent);
        g.drawText ("delay", 28 + (int) vw, 22, 240, 34, juce::Justification::left);
        g.setColour (theme::accent);
        g.fillRoundedRectangle (29.0f, 51.0f, 22.0f, 2.5f, 1.25f);
    }

    // soft float shadow behind the centre display (drawn here so it never clips)
    theme::elevate (g, display.getBounds().toFloat(), 14.0f);

    // section bracket lines under MODULATION / FILTERS
    g.setColour (theme::cardLine);
    g.drawLine (176.0f, 548.0f, 382.0f, 548.0f, 1.2f);          // modulation bracket
    g.drawLine (452.0f, 548.0f, 732.0f, 548.0f, 1.2f);          // filters bracket
    g.drawLine (418.0f, 392.0f, 418.0f, 548.0f, 1.2f);          // vertical divider
}

//==============================================================================
void VocalDelayEditor::resized()
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

    // ---- far-left column ----
    tapButton.setBounds (30, 130, 124, 124);
    tapLabel.setBounds (30, 260, 124, 20);
    lofiBtn.setBounds (38, 336, 108, 40);

    // ---- big delay knob ----
    delayKnob.setBounds (176, 96, 158, 214);

    // ---- mode segmented row ----
    {
        const int y = 112, h = 34;
        int x = 376;
        const int widths[4] = { 48, 92, 62, 48 };
        for (int i = 0; i < 4; ++i)
        {
            modeBtns[(size_t) i].setBounds (x, y, widths[i], h);
            x += widths[i] + 6;
        }
    }

    // ---- centre display ----
    display.setBounds (376, 158, 264, 118);

    // ---- sync segmented row ----
    {
        const int y = 300, h = 34;
        const int w = 84;
        int x = 376;
        for (int i = 0; i < 3; ++i)
        {
            syncBtns[(size_t) i].setBounds (x, y, w, h);
            x += w + 6;
        }
    }

    // ---- big feedback knob ----
    feedbackKnob.setBounds (652, 96, 158, 214);

    // ---- meters ----
    meter.setBounds (824, 116, 104, 300);

    // ---- right column ----
    drywetKnob.setBounds (938, 108, 70, 120);
    outputKnob.setBounds (938, 250, 70, 120);
    analogKnob.setBounds (938, 392, 70, 120);

    // ---- modulation ----
    depthKnob.setBounds (180, 396, 92, 132);
    rateKnob.setBounds (288, 396, 92, 132);
    modSection.setBounds (176, 552, 206, 18);

    // ---- filters ----
    hipassKnob.setBounds (452, 396, 92, 132);
    lopassKnob.setBounds (640, 396, 92, 132);
    linkBtn.setBounds (568, 432, 52, 52);
    linkLabel.setBounds (548, 486, 92, 16);
    filtSection.setBounds (452, 552, 280, 18);
}
