#include "PluginEditor.h"
#include "ui/Theme.h"

//==============================================================================
// Baked-plate geometry. The chassis PNGs are 2048x1360 and the plate fills the
// whole canvas; x/w are fractions of the image width, y/h of its height.
// Measured from the plates (ON-vs-OFF diff components, pink-core scans on the
// ON plate, groove scans on the OFF plate).
namespace plategeo
{
    // THRESHOLD slider groove (fill runs thumb -> bottom)
    constexpr float slCx = 0.3420f, slHalfW = 0.0105f;
    constexpr float slY0 = 0.1544f, slY1 = 0.8331f;

    // meter channels (thresh sidechain, atten, out L/R) — shared travel span
    constexpr float scCx = 0.3943f, attenCx = 0.5594f;
    constexpr float outLCx = 0.7515f, outRCx = 0.8860f;
    constexpr float mY0 = 0.1550f, mY1 = 0.8350f;
    constexpr float mHalfW = 0.0095f;

    // pills (mask rects from diff bounds)
    constexpr float splitRect[4]  = { 0.1050f, 0.1375f, 0.1343f, 0.0618f };
    constexpr float audioRect[4]  = { 0.0918f, 0.7176f, 0.1582f, 0.0603f };
    constexpr float schainRect[4] = { 0.0913f, 0.7882f, 0.1582f, 0.0618f };

    // sidechain filter button: icon box + full hit span (icon + chevron)
    constexpr float scIconRect[4] = { 0.0947f, 0.5110f, 0.0967f, 0.0875f };
    constexpr float scHitX1 = 0.2339f;

    // frequency capsule
    constexpr float freqRect[4] = { 0.0942f, 0.3125f, 0.1519f, 0.0590f };

    // readout capsule label rows (inside the baked capsules)
    constexpr float readY0 = 0.8560f, readY1 = 0.8920f;
    constexpr float scValCx = 0.4173f, attenValCx = 0.5931f;
    constexpr float outLValCx = 0.7737f, outRValCx = 0.8738f;
}

//==============================================================================
juce::RangedAudioParameter& VocalEssEditor::param (VocalEssProcessor& p, const char* id)
{
    return *p.apvts.getParameter (id);
}

//==============================================================================
VocalEssEditor::VocalEssEditor (VocalEssProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      freqBox (param (p, "freq")),
      scBtn   (param (p, "scType"))
{
    setLookAndFeel (&lnf);

    chassisImg   = skin::image ("ess-chassis@2x.png");
    chassisOnImg = skin::image ("ess-chassis-on@2x.png");
    const bool baked = chassisImg.isValid() && chassisOnImg.isValid();
    lnf.plate = baked;

    // Split toggle
    splitBtn.setClickingTogglesState (true);
    splitBtn.setButtonText ("Split");
    addAndMakeVisible (splitBtn);
    splitAtt = std::make_unique<ButtonAtt> (proc.apvts, "split", splitBtn);

    addAndMakeVisible (freqBox);
    addAndMakeVisible (scBtn);

    // Monitor radio
    for (auto* b : { &monAudio, &monSChain })
        addAndMakeVisible (b);
    monAudio .onClick = [this] { setMonitor (0); };
    monSChain.onClick = [this] { setMonitor (1); };

    // Threshold slider + bubble
    addAndMakeVisible (thresholdSlider);
    // Grab-and-drag (relative) instead of jumping to the click position, plus a
    // longer drag throw — makes the threshold feel smooth rather than steppy.
    thresholdSlider.setSliderSnapsToMousePosition (false);
    thresholdSlider.setMouseDragSensitivity (520);
    thresholdSlider.setVelocityBasedMode (false);
    threshAtt = std::make_unique<SliderAtt> (proc.apvts, "threshold", thresholdSlider);
    thresholdSlider.onValueChange = [this] { updateBubble(); };

    auto initValue = [this] (juce::Label& l)
    {
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, theme::accent);
        l.setFont (theme::font (13.0f, true));
        addAndMakeVisible (l);
    };
    initValue (scValue);
    initValue (attenValue);
    initValue (outLValue);
    initValue (outRValue);

    addAndMakeVisible (scBar);
    addAndMakeVisible (attenBar);
    addAndMakeVisible (outLBar);
    addAndMakeVisible (outRBar);

    // Added last so the floating readout sits on top of the meter bars.
    addAndMakeVisible (threshBubble);

    // ------------------------------------------------------------------
    // Baked plate: cards, captions, scales, grooves and capsules live on the
    // chassis images. Pills become invisible hit areas, the slider paints a
    // steel stud thumb, meters are masked from the ON plate, and readouts
    // render into the baked capsules.
    if (baked)
    {
        plateCrop = skin::plateBounds (chassisImg);

        for (auto* b : { &splitBtn, &monAudio, &monSChain })
            b->setComponentID ("hit");

        freqBox.setPlateMode (true);

        // glass tone inside the lit icon box (covers the baked hp curve so the
        // live filter curve can be drawn) — sampled at the least-bloomed spot
        scBtn.setPlateMode (true, juce::Colour (chassisOnImg.getPixelAt (347, 775)));

        for (auto* m : { &scBar, &attenBar, &outLBar, &outRBar })
            m->setVisible (false);

        for (auto* v : { &scValue, &attenValue, &outLValue, &outRValue })
        {
            v->setColour (juce::Label::textColourId, theme::ink);
            v->setFont (theme::font (13.0f, true));
        }

        threshBubble.setFontHeight (16.0f);
    }

    startTimerHz (30);
    // plate mode: the window shows ONLY the plate (cropped at the chrome edge),
    // and must match the crop's aspect exactly or baked circles get stretched
    if (baked)
        setSize (800, juce::roundToInt (800.0f * (float) plateCrop.getHeight()
                                               / (float) plateCrop.getWidth()));
    else
        setSize (680, 500);

    // License overlay sits on top of everything; it shows itself until activated.
    addChildComponent (licenseOverlay);
    licenseOverlay.setBounds (getLocalBounds());
}

VocalEssEditor::~VocalEssEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void VocalEssEditor::setMonitor (int index)
{
    if (auto* pm = proc.apvts.getParameter ("monitor"))
        pm->setValueNotifyingHost (pm->convertTo0to1 ((float) index));
}

void VocalEssEditor::updateBubble()
{
    const auto b = thresholdSlider.getBounds();
    const double prop = thresholdSlider.valueToProportionOfLength (thresholdSlider.getValue());
    const int y = b.getBottom() - (int) std::round (prop * b.getHeight());
    if (chassisImg.isValid())
        threshBubble.setBounds (b.getCentreX() + 6, y - 14, 60, 27);
    else
        threshBubble.setBounds (b.getCentreX() + 4, y - 11, 50, 22);
    threshBubble.toFront (false);
}

//==============================================================================
void VocalEssEditor::timerCallback()
{
    auto fmtDb = [] (float db) { return db < -90.0f ? juce::String ("-inf")
                                                    : juce::String (db, 1); };

    scBar.setLevelDb (proc.scLevelDb.load());
    attenBar.setLevelDb (proc.attenDb.load());
    outLBar.setLevelDb (proc.outLDb.load());
    outRBar.setLevelDb (proc.outRDb.load());

    scValue  .setText (fmtDb (proc.scLevelDb.load()), juce::dontSendNotification);
    attenValue.setText (proc.attenDb.load() < 0.05f ? "0.0"
                        : juce::String (-proc.attenDb.load(), 1), juce::dontSendNotification);
    outLValue.setText (fmtDb (proc.outLDb.load()), juce::dontSendNotification);
    outRValue.setText (fmtDb (proc.outRDb.load()), juce::dontSendNotification);

    threshBubble.setText (juce::String (proc.apvts.getRawParameterValue ("threshold")->load(), 1));

    const int mon = (int) proc.apvts.getRawParameterValue ("monitor")->load();
    monAudio .setToggleState (mon == 0, juce::dontSendNotification);
    monSChain.setToggleState (mon == 1, juce::dontSendNotification);

    const bool split = proc.apvts.getRawParameterValue ("split")->load() > 0.5f;
    splitBtn.setButtonText (split ? "Split" : "Wide");

    freqBox.refresh();
    scBtn.refresh();
    updateBubble();

    if (chassisImg.isValid())
    {
        // dirty-region repaints: only invalidate what actually changed this tick
        using namespace plategeo;

        if (splitBtn.getToggleState() != shownSplit)
        {
            shownSplit = splitBtn.getToggleState();
            repaint (plateFracRect (splitRect[0], splitRect[1],
                                    splitRect[2], splitRect[3]).expanded (12));
        }
        if (mon != shownMon)
        {
            shownMon = mon;
            repaint (plateFracRect (audioRect[0], audioRect[1],
                                    audioRect[2], audioRect[3]).expanded (12));
            repaint (plateFracRect (schainRect[0], schainRect[1],
                                    schainRect[2], schainRect[3]).expanded (12));
        }
        if (thresholdSlider.getValue() != shownThresh)
        {
            shownThresh = thresholdSlider.getValue();
            repaint (plateFracRect (slCx - slHalfW, slY0,
                                    slHalfW * 2.0f, slY1 - slY0).expanded (3));
        }

        // meters move continuously — repaint only their own channels every tick
        for (const float cx : { scCx, attenCx, outLCx, outRCx })
            repaint (plateFracRect (cx - mHalfW, mY0,
                                    mHalfW * 2.0f, mY1 - mY0).expanded (2));
    }
}

//==============================================================================
// plategeo fractions are of the FULL generated canvas; the window shows only
// the plateCrop region, so map full-canvas fraction -> cropped screen px.
juce::Rectangle<int> VocalEssEditor::plateFracRect (float fx, float fy, float fw, float fh) const
{
    const float iw = (float) chassisImg.getWidth(), ih = (float) chassisImg.getHeight();
    const float sx = (float) getWidth()  / (float) plateCrop.getWidth();
    const float sy = (float) getHeight() / (float) plateCrop.getHeight();
    return juce::Rectangle<float> ((fx * iw - (float) plateCrop.getX()) * sx,
                                   (fy * ih - (float) plateCrop.getY()) * sy,
                                   fw * iw * sx,
                                   fh * ih * sy).toNearestInt();
}

// Blit the matching region of the lit plate over the base plate. Both scaled
// caches share the editor's coordinate space, so this is a 1:1 copy — cheap,
// and registration is exact by construction.
void VocalEssEditor::maskFromOn (juce::Graphics& g, juce::Rectangle<int> screenRect)
{
    g.drawImage (plateOnScaled,
                 screenRect.getX(), screenRect.getY(), screenRect.getWidth(), screenRect.getHeight(),
                 screenRect.getX(), screenRect.getY(), screenRect.getWidth(), screenRect.getHeight());
}

// Same reveal but with a soft alpha ramp along the rect border, so the slight
// global tone drift between the two plates never shows as a hard rectangle.
void VocalEssEditor::maskFromOnFeathered (juce::Graphics& g, juce::Rectangle<int> screenRect,
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

void VocalEssEditor::paintPlate (juce::Graphics& g)
{
    using namespace plategeo;

    g.drawImageAt (plateScaled, 0, 0);   // cached 1:1 blit — no per-frame rescale

    const int fpx = juce::roundToInt ((float) getWidth() * 0.010f);

    // ---- lit pills
    if (splitBtn.getToggleState())
        maskFromOnFeathered (g, plateFracRect (splitRect[0], splitRect[1],
                                               splitRect[2], splitRect[3]), fpx);

    const int mon = (int) proc.apvts.getRawParameterValue ("monitor")->load();
    if (mon == 0)
        maskFromOnFeathered (g, plateFracRect (audioRect[0], audioRect[1],
                                               audioRect[2], audioRect[3]), fpx);
    else
        maskFromOnFeathered (g, plateFracRect (schainRect[0], schainRect[1],
                                               schainRect[2], schainRect[3]), fpx);

    // ---- sidechain filter button is always lit (pink outline + bloom)
    maskFromOnFeathered (g, plateFracRect (scIconRect[0], scIconRect[1],
                                           scIconRect[2], scIconRect[3]), fpx);

    // ---- THRESHOLD slider fill: thumb -> bottom of the groove
    {
        const float pr = juce::jlimit (0.0f, 1.0f,
                                       (float) thresholdSlider.valueToProportionOfLength (
                                           thresholdSlider.getValue()));
        const float yTop = slY1 - pr * (slY1 - slY0);
        if (slY1 - yTop > 0.002f)
            maskFromOn (g, plateFracRect (slCx - slHalfW, yTop,
                                          slHalfW * 2.0f, slY1 - yTop));
    }

    // ---- meter fills (atten fills downward from the top, the rest rise)
    auto meterUp = [&] (float cx, float t)
    {
        if (t <= 0.004f) return;
        const float yTop = mY1 - t * (mY1 - mY0);
        maskFromOn (g, plateFracRect (cx - mHalfW, yTop, mHalfW * 2.0f, mY1 - yTop));
    };
    meterUp (scCx,   scBar.getT());
    meterUp (outLCx, outLBar.getT());
    meterUp (outRCx, outRBar.getT());

    if (attenBar.getT() > 0.004f)
        maskFromOn (g, plateFracRect (attenCx - mHalfW, mY0,
                                      mHalfW * 2.0f, attenBar.getT() * (mY1 - mY0)));
}

//==============================================================================
void VocalEssEditor::paint (juce::Graphics& g)
{
    if (chassisImg.isValid())
    {
        paintPlate (g);
        return;
    }

    theme::backdrop (g, getLocalBounds());

    auto card = [&] (juce::Rectangle<int> r, float radius = 14.0f)
    {
        auto rf = r.toFloat();
        theme::elevate (g, rf, radius);
        g.setColour (theme::card);
        g.fillRoundedRectangle (rf, radius);
        theme::topHighlight (g, rf, radius);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (rf, radius, 1.0f);
    };

    for (auto& c : leftCards) card (c);
    card (centerCard);
    card (rightCard);

    // Left card headers — letter-spaced uppercase section labels
    auto header = [&] (juce::Rectangle<int> c, const juce::String& t)
    {
        theme::spacedText (g, t, c.reduced (14, 11).removeFromTop (18).toFloat(),
                           theme::ink, 11.5f, 2.0f, true, juce::Justification::centred);
    };
    header (leftCards[0], "Audio");
    header (leftCards[1], "Frequency");
    header (leftCards[2], "SideChain");
    header (leftCards[3], "Monitor");

    // Center / right group headers — letter-spaced uppercase labels
    auto groupTitle = [&] (juce::Rectangle<int> area, const juce::String& t)
    {
        theme::spacedText (g, t, area.reduced (10, 0).toFloat(),
                           theme::ink, 12.0f, 2.4f, true, juce::Justification::centred);
    };
    {
        const int innerX = centerCard.getX() + 26;
        const int innerW = centerCard.getWidth() - 52;
        const int threshW = innerW * 6 / 10;
        groupTitle (juce::Rectangle<int> (innerX, centerCard.getY() + 18, threshW, 24), "Threshold");
        groupTitle (juce::Rectangle<int> (innerX + threshW, centerCard.getY() + 18, innerW - threshW, 24), "Atten");
    }
    groupTitle (juce::Rectangle<int> (rightCard.getX(), rightCard.getY() + 18, rightCard.getWidth(), 24), "Output");

    // Scales
    auto drawScale = [&] (juce::Rectangle<int> area,
                          std::initializer_list<std::pair<juce::String, float>> ticks)
    {
        for (auto& t : ticks)
        {
            const int y = area.getY() + (int) (t.second * area.getHeight());
            g.setColour (theme::cardLine);
            g.fillRect (area.getX(), y, 8, 1);
            g.setColour (theme::inkSoft);
            g.setFont (theme::font (10.0f, false));
            g.drawText (t.first, area.getX() + 12, y - 7, area.getWidth() - 12, 14,
                        juce::Justification::centredLeft);
        }
    };
    drawScale (threshScaleArea, { {"0",0.0f}, {"20",0.25f}, {"40",0.5f}, {"60",0.75f}, {"80",1.0f} });
    drawScale (attenScaleArea,  { {"0",0.0f}, {"-3",0.125f}, {"-6",0.25f}, {"-12",0.5f}, {"-18",0.75f}, {"-Inf",1.0f} });
    drawScale (outScaleArea,    { {"0",0.0f}, {"-6",0.2f}, {"-12",0.4f}, {"-18",0.6f}, {"-24",0.8f}, {"-30",1.0f} });

    // Value pills (drawn behind the labels, which are child components)
    auto valuePill = [&] (juce::Component& c)
    {
        auto r = c.getBounds().toFloat();
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (r, 10.0f, 1.0f);
    };
    valuePill (scValue);
    valuePill (attenValue);
    valuePill (outLValue);
    valuePill (outRValue);
}

//==============================================================================
void VocalEssEditor::resized()
{
    licenseOverlay.setBounds (getLocalBounds());

    if (chassisImg.isValid())
    {
        using namespace plategeo;

        // rebuild the scaled plate caches for the new size (1:1 blits per frame)
        plateScaled   = skin::renderPlate (chassisImg,   plateCrop, getWidth(), getHeight());
        plateOnScaled = skin::renderPlate (chassisOnImg, plateCrop, getWidth(), getHeight());

        const float iw = (float) chassisImg.getWidth(), ih = (float) chassisImg.getHeight();
        const float sx = (float) getWidth()  / (float) plateCrop.getWidth();
        const float sy = (float) getHeight() / (float) plateCrop.getHeight();

        // pills + freq + sidechain: hit areas over the baked art
        splitBtn.setBounds  (plateFracRect (splitRect[0],  splitRect[1],  splitRect[2],  splitRect[3]));
        monAudio.setBounds  (plateFracRect (audioRect[0],  audioRect[1],  audioRect[2],  audioRect[3]));
        monSChain.setBounds (plateFracRect (schainRect[0], schainRect[1], schainRect[2], schainRect[3]));
        freqBox.setBounds   (plateFracRect (freqRect[0], freqRect[1], freqRect[2], freqRect[3]));
        scBtn.setBounds     (plateFracRect (scIconRect[0], scIconRect[1],
                                            scHitX1 - scIconRect[0], scIconRect[3]));

        // threshold slider: bounds sized so the JUCE thumb travel (inset by
        // the fixed 12px V4 thumb radius) matches the baked groove exactly.
        // Width (= drawn thumb dia) scales with the window. All coordinates
        // go through the crop mapping (fractions are of the full canvas).
        const int tr = 12;
        const int sw = juce::roundToInt (0.0273f * iw * sx);
        const float cx = (slCx * iw - (float) plateCrop.getX()) * sx;
        const float y0 = (slY0 * ih - (float) plateCrop.getY()) * sy;
        const float y1 = (slY1 * ih - (float) plateCrop.getY()) * sy;
        thresholdSlider.setBounds (juce::roundToInt (cx) - sw / 2,
                                   juce::roundToInt (y0) - tr,
                                   sw, juce::roundToInt (y1 - y0) + tr * 2);

        // readouts inside the baked capsules
        scValue.setBounds    (plateFracRect (scValCx    - 0.032f, readY0, 0.064f, readY1 - readY0));
        attenValue.setBounds (plateFracRect (attenValCx - 0.032f, readY0, 0.064f, readY1 - readY0));
        outLValue.setBounds  (plateFracRect (outLValCx  - 0.032f, readY0, 0.064f, readY1 - readY0));
        outRValue.setBounds  (plateFracRect (outRValCx  - 0.032f, readY0, 0.064f, readY1 - readY0));

        // hidden meters keep bounds so nothing depends on stale rects
        scBar.setBounds    (plateFracRect (scCx - mHalfW,    mY0, mHalfW * 2.0f, mY1 - mY0));
        attenBar.setBounds (plateFracRect (attenCx - mHalfW, mY0, mHalfW * 2.0f, mY1 - mY0));
        outLBar.setBounds  (plateFracRect (outLCx - mHalfW,  mY0, mHalfW * 2.0f, mY1 - mY0));
        outRBar.setBounds  (plateFracRect (outRCx - mHalfW,  mY0, mHalfW * 2.0f, mY1 - mY0));

        updateBubble();
        return;
    }

    auto content = getLocalBounds().reduced (14);

    auto centred = [] (juce::Rectangle<int> area, int w, int h)
    {
        return juce::Rectangle<int> (area.getCentreX() - w / 2, area.getCentreY() - h / 2, w, h);
    };

    // --- left column ---
    auto left = content.removeFromLeft (150);
    content.removeFromLeft (12);
    {
        auto stack = left;
        const int gap = 10;
        const int total = stack.getHeight() - gap * 3;
        leftCards[0] = stack.removeFromTop (juce::roundToInt (total * 0.20f)); stack.removeFromTop (gap);
        leftCards[1] = stack.removeFromTop (juce::roundToInt (total * 0.20f)); stack.removeFromTop (gap);
        leftCards[2] = stack.removeFromTop (juce::roundToInt (total * 0.25f)); stack.removeFromTop (gap);
        leftCards[3] = stack;
    }

    // Audio / Split
    {
        auto c = leftCards[0].reduced (12); c.removeFromTop (20);
        splitBtn.setBounds (centred (c, 92, 30));
    }
    // Frequency
    {
        auto c = leftCards[1].reduced (12); c.removeFromTop (20);
        freqBox.setBounds (centred (c, 108, 34));
    }
    // SideChain filter
    {
        auto c = leftCards[2].reduced (12); c.removeFromTop (22);
        scBtn.setBounds (centred (c, 108, 40));
    }
    // Monitor stacked pills
    {
        auto c = leftCards[3].reduced (12); c.removeFromTop (20);
        monAudio .setBounds (c.removeFromTop (30).reduced (4, 1));
        c.removeFromTop (7);
        monSChain.setBounds (c.removeFromTop (30).reduced (4, 1));
    }

    // --- right card (Output) ---
    rightCard = content.removeFromRight (168);
    content.removeFromRight (12);
    {
        auto inner = rightCard.reduced (16);
        inner.removeFromTop (30);
        auto valueRow = inner.removeFromBottom (24);
        inner.removeFromTop (4); inner.removeFromBottom (8);

        auto meter = inner;
        auto la = meter.removeFromLeft (30);
        auto ra = meter.removeFromRight (30);
        outScaleArea = meter;
        outLBar.setBounds (centred (la, 16, la.getHeight()));
        outRBar.setBounds (centred (ra, 16, ra.getHeight()));

        outLValue.setBounds (centred (valueRow.removeFromLeft (valueRow.getWidth() / 2), 48, 22));
        outRValue.setBounds (centred (valueRow, 48, 22));
    }

    // --- center card (Threshold + Atten) ---
    centerCard = content;
    {
        auto inner = centerCard.reduced (18);
        inner.removeFromTop (30);
        auto valueRow = inner.removeFromBottom (24);
        inner.removeFromTop (4); inner.removeFromBottom (8);

        auto meter = inner;
        auto threshGroup = meter.removeFromLeft (meter.getWidth() * 58 / 100);
        auto attenGroup  = meter;

        // threshold group: slider | gap | scBar | scale
        thresholdSlider.setBounds (threshGroup.removeFromLeft (38));
        threshGroup.removeFromLeft (8);
        scBar.setBounds (centred (threshGroup.removeFromLeft (22), 16, threshGroup.getHeight()));
        threshScaleArea = threshGroup;

        // atten group: attenBar | scale
        attenBar.setBounds (centred (attenGroup.removeFromLeft (22), 16, attenGroup.getHeight()));
        attenGroup.removeFromLeft (6);
        attenScaleArea = attenGroup;

        // value pills under each group
        auto vThresh = valueRow.removeFromLeft (valueRow.getWidth() * 58 / 100);
        scValue.setBounds (centred (vThresh, 52, 22));
        attenValue.setBounds (centred (valueRow, 52, 22));
    }

    updateBubble();
}
