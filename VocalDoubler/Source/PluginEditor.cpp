#include "PluginEditor.h"

namespace
{
    const juce::StringArray kModDivisionNames { "1/1", "1/2", "1/4", "1/4.", "1/8", "1/8.", "1/8T", "1/16" };
}

//==============================================================================
// Baked-plate geometry, measured on the 2048x1360 chassis plates (ON-vs-OFF
// diff components, radial pink ring profiles, groove scans). Values are raw
// image pixels converted to canvas fractions.
namespace plategeo
{
    constexpr float px (float v) { return v / 2048.0f; }
    constexpr float py (float v) { return v / 1360.0f; }

    // top bar pills
    constexpr float tipsX0 = px (1379.0f), tipsX1 = px (1615.0f);
    constexpr float bypX0  = px (1644.0f), bypX1  = px (1833.0f);
    constexpr float menuX0 = px (1860.0f), menuX1 = px (1925.0f);
    constexpr float topY0  = py (128.0f),  topY1  = py (193.0f);

    // central interactive disc (dark glass face baked; dynamics drawn live)
    constexpr float discX0 = px (575.0f),  discX1 = px (1430.0f);
    constexpr float discY0 = py (239.0f),  discY1 = py (1093.0f);

    // left readouts (captions baked; big values drawn live)
    constexpr float sepValX0 = px (138.0f), sepValX1 = px (460.0f);
    constexpr float sepValY0 = py (562.0f), sepValY1 = py (672.0f);
    constexpr float varValY0 = py (810.0f), varValY1 = py (920.0f);

    // Effect Only pill
    constexpr float fxX0 = px (1595.0f), fxX1 = px (1861.0f);
    constexpr float fxY0 = py (377.0f),  fxY1 = py (444.0f);

    // amount knob: solid pink band r 88..112, bloom to 126, dome hugs inner edge
    constexpr float amtCx = px (1726.3f), amtCy = py (635.7f);
    constexpr float amtDomeDia = px (172.0f);
    constexpr float amtDomeR = px (86.0f), amtSolidR = px (112.0f), amtMaxR = px (126.0f);

    // mod rate cluster (component rect; children placed inside by ModRateControl)
    constexpr float modX0 = px (1540.0f), modX1 = px (2000.0f);
    constexpr float modY0 = py (905.0f),  modY1 = py (1075.0f);

    // rate knob: solid band r 32..50, bloom to 58
    constexpr float ratCx = px (1597.4f), ratCy = py (998.8f);
    constexpr float ratDomeR = px (32.0f), ratSolidR = px (50.0f), ratMaxR = px (58.0f);

    // sync + division pills (absolute, for lit masks + live div text)
    constexpr float syncX0 = px (1706.0f), syncX1 = px (1939.0f);
    constexpr float syncY0 = py (913.0f),  syncY1 = py (981.0f);
    constexpr float divX0 = px (1707.0f), divX1 = px (1938.0f);
    constexpr float divY0 = py (1000.0f), divY1 = py (1065.0f);
    constexpr float divTxtX0 = px (1760.0f), divTxtX1 = px (1885.0f);

    // MOD RATE live value, right-aligned to the pill column
    constexpr float mrValX0 = px (1670.0f), mrValX1 = px (1944.0f);
    constexpr float mrValY0 = py (832.0f),  mrValY1 = py (870.0f);
}

//==============================================================================
VocalDoublerEditor::VocalDoublerEditor (VocalDoublerProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);

    chassisImg   = skin::image ("doubler-chassis@2x.png");
    chassisOnImg = skin::image ("doubler-chassis-on@2x.png");
    plateBaked = chassisImg.isValid() && chassisOnImg.isValid();

    // ---- branding ----
    // Wordmark + tagline are drawn in paint() (two-tone + spaced caps). The
    // brand label is kept (empty) purely as a layout anchor.
    brand.setText ("", juce::dontSendNotification);
    brand.setFont (theme::font (21.0f, true));
    brand.setColour (juce::Label::textColourId, theme::ink);
    addAndMakeVisible (brand);

    brandSub.setText ("", juce::dontSendNotification);
    brandSub.setFont (theme::font (11.5f, false));
    brandSub.setColour (juce::Label::textColourId, theme::inkSoft);
    addAndMakeVisible (brandSub);

    // ---- top-right controls ----
    tipsBtn.onClick   = [this] { showTips(); };
    menuBtn.onClick   = [this] { showMenu(); };
    bypassBtn.onClick = [this] { proc.bypassed.store (bypassBtn.getToggleState()); };
    bypassBtn.setToggleState (proc.bypassed.load(), juce::dontSendNotification);
    addAndMakeVisible (tipsBtn);
    addAndMakeVisible (bypassBtn);
    addAndMakeVisible (menuBtn);

    // ---- centre interactive display ----
    display.onChange = [this] (float sep01, float var01)
    {
        setParam ("separation", sep01 * 100.0f);
        setParam ("variation",  var01 * 100.0f);
    };
    addAndMakeVisible (display);

    // ---- left readouts ----
    separationRO.onChange = [this] (float v01) { setParam ("separation", v01 * 100.0f); };
    variationRO.onChange  = [this] (float v01) { setParam ("variation",  v01 * 100.0f); };
    addAndMakeVisible (separationRO);
    addAndMakeVisible (variationRO);

    // ---- right controls ----
    addAndMakeVisible (effectOnly);
    effectOnlyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, "effectOnly", effectOnly);

    amountKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    amountKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    amountKnob.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                    juce::MathConstants<float>::pi * 2.8f, true);
    addAndMakeVisible (amountKnob);
    amountAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, "amount", amountKnob);

    amountLabel.setText ("Amount", juce::dontSendNotification);
    amountLabel.setJustificationType (juce::Justification::centred);
    amountLabel.setFont (theme::font (15.0f, false));
    amountLabel.setColour (juce::Label::textColourId, theme::ink);
    addAndMakeVisible (amountLabel);

    // ---- modulation rate (+ host-tempo sync) ----
    addAndMakeVisible (modRate);
    modRateAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, "modRate", modRate.rateKnob);
    modSyncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, "modSync", modRate.syncBtn);
    modRate.divBtn.onClick = [this] { cycleModDivision(); };

    if (plateBaked)
        setupPlateMode();

    startTimerHz (30);
    setSize (1024, 640);

    // License overlay sits on top of everything; it shows itself until activated.
    addChildComponent (licenseOverlay);
    licenseOverlay.setBounds (getLocalBounds());
}

VocalDoublerEditor::~VocalDoublerEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void VocalDoublerEditor::setParam (const char* id, float value0to100)
{
    if (auto* param = proc.apvts.getParameter (id))
        param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (value0to100));
}

void VocalDoublerEditor::cycleModDivision()
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("modDiv")))
    {
        const int n = choice->choices.size();
        if (n <= 0) return;
        const int next = (choice->getIndex() + 1) % n;
        choice->setValueNotifyingHost (choice->getNormalisableRange().convertTo0to1 ((float) next));
    }
}

void VocalDoublerEditor::showTips()
{
    auto* opts = new juce::AlertWindow ("Vocal Tips",
        "Doubling tips:\n\n"
        "- Keep Separation lower for a natural, glued double; push it wide for a "
        "big stacked chorus effect.\n"
        "- More Variation = looser, more human multi-take feel; less = a tight twin.\n"
        "- Use Amount to blend the doubles under the dry vocal.\n"
        "- Try Effect Only to audition just the doubled signal.",
        juce::MessageBoxIconType::NoIcon);
    opts->addButton ("Got it", 1);
    opts->enterModalState (true, juce::ModalCallbackFunction::create (
        [opts] (int) { delete opts; }), false);
}

void VocalDoublerEditor::showMenu()
{
    juce::PopupMenu menu;
    juce::PopupMenu presets;
    const int n = proc.getNumPrograms();
    for (int i = 0; i < n; ++i)
        presets.addItem (100 + i, proc.getProgramName (i), true, i == proc.getCurrentProgram());

    menu.addSubMenu ("Presets", presets);
    menu.addSeparator();
    menu.addItem (1, "Vocal Tips");
    menu.addItem (2, "About VocalDoubler", false);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuBtn),
        [this] (int result)
        {
            if (result >= 100)        proc.setCurrentProgram (result - 100);
            else if (result == 1)     showTips();
        });
}

//==============================================================================
void VocalDoublerEditor::timerCallback()
{
    const float sep = proc.apvts.getRawParameterValue ("separation")->load();
    const float var = proc.apvts.getRawParameterValue ("variation")->load();
    const float amt = proc.apvts.getRawParameterValue ("amount")->load();

    display.setValues (sep * 0.01f, var * 0.01f);
    display.setEnergy (juce::jlimit (0.0f, 1.0f, (amt * 0.6f + var * 0.4f) * 0.01f));

    separationRO.setValue01 (sep * 0.01f);
    variationRO.setValue01 (var * 0.01f);

    bypassBtn.setToggleState (proc.bypassed.load(), juce::dontSendNotification);

    // ---- modulation rate readout ----
    const bool  synced  = proc.apvts.getRawParameterValue ("modSync")->load() > 0.5f;
    const float modHz   = proc.apvts.getRawParameterValue ("modRate")->load();
    const int   divIdx  = juce::jlimit (0, kModDivisionNames.size() - 1,
                                        (int) proc.apvts.getRawParameterValue ("modDiv")->load());
    const juce::String divName = kModDivisionNames[divIdx];

    modRate.divBtn.setText (divName);
    modRate.divBtn.setEnabled (synced);
    modRate.rateKnob.setEnabled (! synced); // when synced the rate is host-driven
    modRate.setReadout (synced, modHz, divName);

    if (plateBaked)
        repaint();   // lit masks + ring wedges + value texts live in paintPlate
}

//==============================================================================
// Baked-plate mode: static art comes from the OFF chassis plate; lit states
// are revealed by blitting the same regions from the pixel-registered ON
// plate. Knobs draw rotating chrome dome sprites; captions are baked, value
// read-outs drawn live.
void VocalDoublerEditor::setupPlateMode()
{
    laf.plate = true;
    laf.domeLarge = skin::cropToDome (skin::image ("grit-knob-large@2x.png"),
                                      0.1999f, 0.3533f, 0.199f);
    laf.domeSmall = skin::cropToDome (skin::image ("grit-knob-small@2x.png"),
                                      0.4993f, 0.4648f, 0.615f);

    // knobs: dome sprites with a full-360 sweep from 6 o'clock
    amountKnob.setComponentID ("dome-large");
    modRate.rateKnob.setComponentID ("dome-small");
    for (auto* s : { &amountKnob, &modRate.rateKnob })
        s->setRotaryParameters (juce::MathConstants<float>::pi,
                                juce::MathConstants<float>::pi * 3.0f, true);

    // buttons become invisible hit areas; lit states are masked in paintPlate
    tipsBtn.plate = bypassBtn.plate = menuBtn.plate = true;
    effectOnly.plate = true;
    modRate.plate = true;
    modRate.syncBtn.plate = true;
    modRate.divBtn.plate = true;

    // the dark glass disc face is baked; only dynamics are drawn
    display.plate = true;

    // captions baked; big live numbers drawn by the readouts themselves
    separationRO.plate = true;
    variationRO.plate = true;

    amountLabel.setVisible (false);
    brand.setVisible (false);
    brandSub.setVisible (false);
}

juce::Rectangle<int> VocalDoublerEditor::plateFracRect (float fx0, float fy0, float fx1, float fy1) const
{
    const float W = (float) getWidth(), H = (float) getHeight();
    return juce::Rectangle<float> (fx0 * W, fy0 * H, (fx1 - fx0) * W, (fy1 - fy0) * H)
               .toNearestInt();
}

// Blit the matching region of the lit plate over the base plate — pixel
// registration is guaranteed because both images share the same canvas.
void VocalDoublerEditor::maskFromOn (juce::Graphics& g, juce::Rectangle<int> screenRect)
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
void VocalDoublerEditor::maskFromOnFeathered (juce::Graphics& g, juce::Rectangle<int> screenRect,
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
void VocalDoublerEditor::drawRingWedge (juce::Graphics& g, juce::Slider& s, float cxFrac, float cyFrac,
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

void VocalDoublerEditor::paintPlate (juce::Graphics& g)
{
    using namespace plategeo;

    g.drawImage (chassisImg, getLocalBounds().toFloat(),
                 juce::RectanglePlacement::stretchToFit);

    const int feather = juce::roundToInt ((float) getWidth() * 0.008f);

    // ---- toggle / momentary pills light up
    if (bypassBtn.getToggleState())
        maskFromOnFeathered (g, plateFracRect (bypX0, topY0, bypX1, topY1), feather);
    if (tipsBtn.isDown())
        maskFromOnFeathered (g, plateFracRect (tipsX0, topY0, tipsX1, topY1), feather);
    if (menuBtn.isDown())
        maskFromOnFeathered (g, plateFracRect (menuX0, topY0, menuX1, topY1), feather);
    if (effectOnly.getToggleState())
        maskFromOnFeathered (g, plateFracRect (fxX0, fxY0, fxX1, fxY1), feather);

    const bool synced = modRate.syncBtn.getToggleState();
    if (synced)
    {
        maskFromOnFeathered (g, plateFracRect (syncX0, syncY0, syncX1, syncY1), feather);
        maskFromOnFeathered (g, plateFracRect (divX0, divY0, divX1, divY1), feather);
    }

    // ---- knob neon ring wedges
    drawRingWedge (g, amountKnob,       amtCx, amtCy, amtDomeR, amtSolidR, amtMaxR);
    drawRingWedge (g, modRate.rateKnob, ratCx, ratCy, ratDomeR, ratSolidR, ratMaxR);

    // ---- live texts (captions are baked)
    g.setColour (theme::accent);
    g.setFont (theme::font (13.0f, true));
    g.drawText (modRate.readoutText(), plateFracRect (mrValX0, mrValY0, mrValX1, mrValY1),
                juce::Justification::centredRight);

    g.setColour (synced ? juce::Colours::white : theme::inkSoft.withAlpha (0.6f));
    g.setFont (theme::font (14.0f, false));
    g.drawText (modRate.divBtn.getText(), plateFracRect (divTxtX0, divY0, divTxtX1, divY1),
                juce::Justification::centred);
}

void VocalDoublerEditor::layoutPlate()
{
    using namespace plategeo;
    auto fr = [this] (float fx0, float fy0, float fx1, float fy1)
    {
        return plateFracRect (fx0, fy0, fx1, fy1);
    };
    const float W = (float) getWidth(), H = (float) getHeight();

    tipsBtn.setBounds   (fr (tipsX0, topY0, tipsX1, topY1));
    bypassBtn.setBounds (fr (bypX0,  topY0, bypX1,  topY1));
    menuBtn.setBounds   (fr (menuX0, topY0, menuX1, topY1));

    display.setBounds (fr (discX0, discY0, discX1, discY1));

    separationRO.setBounds (fr (sepValX0, sepValY0, sepValX1, sepValY1));
    variationRO.setBounds  (fr (sepValX0, varValY0, sepValX1, varValY1));

    effectOnly.setBounds (fr (fxX0, fxY0, fxX1, fxY1));

    {
        const float side = amtDomeDia * W * 1.06f;
        amountKnob.setBounds (juce::Rectangle<float> (amtCx * W - side * 0.5f,
                                                      amtCy * H - side * 0.5f,
                                                      side, side).toNearestInt());
    }
    amountLabel.setBounds (0, 0, 0, 0);

    modRate.setBounds (fr (modX0, modY0, modX1, modY1));

    brand.setBounds (0, 0, 0, 0);
    brandSub.setBounds (0, 0, 0, 0);
}

//==============================================================================
void VocalDoublerEditor::paint (juce::Graphics& g)
{
    if (plateBaked)
    {
        paintPlate (g);
        return;
    }

    theme::backdrop (g, getLocalBounds());

    // main floating card
    {
        auto rf = cardArea.toFloat();
        theme::elevate (g, rf, 24.0f);
        g.setColour (theme::card);
        g.fillRoundedRectangle (rf, 24.0f);
        theme::topHighlight (g, rf, 24.0f);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (rf, 24.0f, 1.0f);
    }

    // ---- two-tone wordmark + accent underline ----
    {
        const auto bb = brand.getBounds();
        auto wm = theme::font (21.0f, true);
        g.setFont (wm);
        const float vw = juce::GlyphArrangement::getStringWidth (wm, "vocal");
        g.setColour (theme::ink);
        g.drawText ("vocal", bb, juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.drawText ("doubler",
                    bb.withTrimmedLeft ((int) vw), juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.fillRoundedRectangle ((float) bb.getX(), (float) bb.getBottom() - 3.0f, 20.0f, 2.5f, 1.25f);
    }

    // ---- spaced-caps tagline ----
    theme::spacedText (g, "VOCAL DOUBLER", brandSub.getBounds().toFloat(),
                       theme::inkSoft, 9.5f, 2.6f, false, juce::Justification::centredLeft);

    // ---- logo glyph: a small pink waveform to the left of the brand ----
    {
        const auto bb = brand.getBounds();
        juce::Rectangle<float> logo ((float) bb.getX() - 40.0f, (float) bb.getCentreY() - 13.0f, 30.0f, 26.0f);
        juce::Path wave;
        const int pts = 48;
        for (int i = 0; i <= pts; ++i)
        {
            const float t = (float) i / (float) pts;
            const float x = logo.getX() + t * logo.getWidth();
            const float env = std::sin (t * juce::MathConstants<float>::pi); // taper ends
            const float y = logo.getCentreY()
                          - std::sin (t * juce::MathConstants<float>::twoPi * 2.2f)
                            * logo.getHeight() * 0.5f * env;
            if (i == 0) wave.startNewSubPath (x, y);
            else        wave.lineTo (x, y);
        }
        g.setColour (theme::accent);
        g.strokePath (wave, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    // ---- bottom legend: dry vocal / doubled vocal ----
    {
        g.setFont (theme::font (13.0f, false));
        const float y = (float) cardArea.getBottom() - 34.0f;
        const float cx = (float) cardArea.getCentreX();

        // gray dot + "dry vocal"
        const juce::String dry ("dry vocal");
        const juce::String wet ("doubled vocal");
        const float dryW = juce::GlyphArrangement::getStringWidth (theme::font (13.0f, false), dry);
        const float wetW = juce::GlyphArrangement::getStringWidth (theme::font (13.0f, false), wet);
        const float dotD = 9.0f, gap = 8.0f, sepGap = 28.0f;
        const float total = dotD + gap + dryW + sepGap + dotD + gap + wetW;
        float x = cx - total * 0.5f;

        g.setColour (theme::inkSoft.withAlpha (0.6f));
        g.fillEllipse (x, y - dotD * 0.5f, dotD, dotD);
        x += dotD + gap;
        g.setColour (theme::inkSoft);
        g.drawText (dry, juce::Rectangle<float> (x, y - 9.0f, dryW + 4.0f, 18.0f),
                    juce::Justification::centredLeft);
        x += dryW + sepGap * 0.5f;

        // thin divider
        g.setColour (theme::cardLine);
        g.drawLine (x, y - 8.0f, x, y + 8.0f, 1.0f);
        x += sepGap * 0.5f;

        g.setColour (theme::accent);
        g.fillEllipse (x, y - dotD * 0.5f, dotD, dotD);
        x += dotD + gap;
        g.setColour (theme::inkSoft);
        g.drawText (wet, juce::Rectangle<float> (x, y - 9.0f, wetW + 4.0f, 18.0f),
                    juce::Justification::centredLeft);
    }
}

//==============================================================================
void VocalDoublerEditor::resized()
{
    licenseOverlay.setBounds (getLocalBounds());

    if (plateBaked)
    {
        layoutPlate();
        return;
    }

    auto r = getLocalBounds().reduced (14);
    cardArea = r;

    auto inner = r.reduced (24);

    // ---- top bar ----
    auto top = inner.removeFromTop (40);

    // brand (leave room on the far left for the logo glyph drawn in paint)
    brand.setBounds (top.getX() + 44, top.getY() + 4, 160, 30);
    brandSub.setBounds (brand.getRight() + 10, top.getY() + 11, 140, 20);

    // top-right buttons (laid out right-to-left)
    const int h = 38;
    auto tr = top;
    menuBtn.setBounds (tr.removeFromRight (38).withHeight (h).withY (top.getY() + 1));
    tr.removeFromRight (10);
    bypassBtn.setBounds (tr.removeFromRight (104).withHeight (h).withY (top.getY() + 1));
    tr.removeFromRight (10);
    tipsBtn.setBounds (tr.removeFromRight (128).withHeight (h).withY (top.getY() + 1));

    // ---- left readouts ----
    auto body = inner.withTrimmedTop (8);
    auto leftCol = body.removeFromLeft (170);
    separationRO.setBounds (leftCol.getX(), leftCol.getCentreY() - 96, 170, 78);
    variationRO.setBounds  (leftCol.getX(), leftCol.getCentreY() + 12, 170, 78);

    // ---- right controls: Effect Only, Amount, then the Mod Rate group ----
    auto rightCol = body.removeFromRight (210);
    {
        const int cx = rightCol.getCentreX();

        const int pillW = 150, pillH = 40;
        const int knobSz = 108, labelH = 22, modH = 120;

        // centre the whole stack vertically within the right column
        const int stackH = pillH + 34 + knobSz + labelH + 18 + modH;
        int y = rightCol.getY() + juce::jmax (0, (rightCol.getHeight() - stackH) / 2);

        effectOnly.setBounds (cx - pillW / 2, y, pillW, pillH);
        y += pillH + 34;

        amountKnob.setBounds (cx - knobSz / 2, y, knobSz, knobSz);
        amountLabel.setBounds (cx - 70, amountKnob.getBottom() + 2, 140, labelH);
        y = amountLabel.getBottom() + 18;

        modRate.setBounds (rightCol.getX(), y, 210, modH);
    }

    // ---- centre interactive display fills what's left ----
    auto centre = body.withTrimmedBottom (30); // keep clear of the legend
    const int d = juce::jmin (centre.getWidth(), centre.getHeight());
    display.setBounds (centre.withSizeKeepingCentre (d, d));
}
