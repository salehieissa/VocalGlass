#include "PluginEditor.h"
#include "Scales.h"

//==============================================================================
// Baked-plate geometry. The chassis PNGs are 2048x1360 and the plate fills the
// whole canvas; x fractions are of the image width, y fractions of its height.
// Everything below is measured from the plates (ON-vs-OFF diff components,
// radial pink profiles, groove scans and glyph fits on the OFF plate).
namespace plategeo
{
    // header dropdown capsules (Vocal Range / Key / Scale) + shared row band
    constexpr float vrX0 = 0.2544f, vrX1 = 0.3882f;
    constexpr float keyX0 = 0.4058f, keyX1 = 0.4658f;
    constexpr float scX0 = 0.4824f, scX1 = 0.6187f;
    constexpr float headY0 = 0.0949f, headY1 = 0.1434f;

    // preset pill cavity + round stepper buttons (art order is '<' '>' '...')
    constexpr float presX0 = 0.6572f, presX1 = 0.8325f;
    constexpr float presY0 = 0.0897f, presY1 = 0.1471f;
    constexpr float prevX0 = 0.8325f, prevX1 = 0.8652f;   // '<' glyph c=1738
    constexpr float nextX0 = 0.8652f, nextX1 = 0.8989f;   // '>' glyph c=1806
    constexpr float dotsX0 = 0.8989f, dotsX1 = 0.9326f;
    constexpr float btnY0 = 0.0904f, btnY1 = 0.1463f;

    // dark smoked-glass pitch display
    constexpr float dispX0 = 0.0625f, dispY0 = 0.1912f;
    constexpr float dispX1 = 0.6216f, dispY1 = 0.3691f;

    // cents meter groove (dark channel on the OFF plate)
    constexpr float metX0 = 0.0830f, metY0 = 0.3904f;
    constexpr float metX1 = 0.6006f, metY1 = 0.4449f;

    // reset (circular arrow) beside "edit notes"
    constexpr float rstX0 = 0.5884f, rstY0 = 0.4890f;
    constexpr float rstX1 = 0.6235f, rstY1 = 0.5419f;

    // keyboard (white-key extent of the GPT-drawn 15-white keyboard)
    constexpr float kbX0 = 0.0641f, kbY0 = 0.5574f;
    constexpr float kbX1 = 0.6187f, kbY1 = 0.7426f;

    // detune: bold readout text (no capsule in this art) + small knob
    constexpr float detValX0 = 0.0640f, detValY0 = 0.7897f;
    constexpr float detValX1 = 0.1560f, detValY1 = 0.8654f;
    constexpr float detCx = 0.2176f, detCy = 0.8302f, detDomeDia = 0.0264f;
    constexpr float detDomeR = 0.0122f, detSolidR = 0.0229f, detMaxR = 0.0264f;

    // mode pill: mask rects run seam-to-seam (never expand!)
    constexpr float modeX0 = 0.3647f, modeY0 = 0.8235f;
    constexpr float modeX1 = 0.5591f, modeY1 = 0.8735f;
    constexpr float modeSeam = 0.4741f;

    // hero retune-speed knob + its value capsule
    constexpr float heroCx = 0.7995f, heroCy = 0.3719f, heroDomeDia = 0.1348f;
    constexpr float heroDomeR = 0.0640f, heroSolidR = 0.0850f, heroMaxR = 0.0889f;
    constexpr float retValX0 = 0.8779f, retValY0 = 0.1919f;
    constexpr float retValX1 = 0.9307f, retValY1 = 0.2235f;

    // humanize / flex tune knobs + value capsules
    constexpr float humCx = 0.7273f, humCy = 0.6175f;
    constexpr float flexCx = 0.8722f, flexCy = 0.6177f;
    constexpr float smallDomeDia = 0.0439f;
    constexpr float smallDomeR = 0.0210f, smallSolidR = 0.0332f, smallMaxR = 0.0361f;
    constexpr float humValX0 = 0.7012f, humValX1 = 0.7563f;
    constexpr float flexValX0 = 0.8452f, flexValX1 = 0.9004f;
    constexpr float smallValY0 = 0.6846f, smallValY1 = 0.7147f;

    // Modern/Classic toggle track
    constexpr float togX0 = 0.7778f, togY0 = 0.7441f;
    constexpr float togX1 = 0.8306f, togY1 = 0.7831f;

    // bottom icon buttons (undo / redo / gear / power), circle-fit centres
    constexpr float botCx[4] = { 0.6924f, 0.7676f, 0.8423f, 0.9150f };
    constexpr float botCy = 0.8754f;
    constexpr float botR = 0.0176f, botMaskR = 0.0225f;
}

//==============================================================================
VocalTuneEditor::VocalTuneEditor (VocalTuneProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);

    chassisImg   = skin::image ("tune-chassis@2x.png");
    chassisOnImg = skin::image ("tune-chassis-on@2x.png");
    keyboard.setOnPlate (chassisOnImg);
    plateBaked = chassisImg.isValid() && chassisOnImg.isValid();
    laf.plate = plateBaked;

    // ---- branding ----
    // The wordmark + tagline are painted directly (two-tone + letter-spaced) in
    // paint(); these labels are kept (laid out in resized) purely to anchor the
    // painted text geometry, so they stay hidden.
    brand.setText ("vocaltune", juce::dontSendNotification);
    brand.setFont (theme::font (26.0f, true));
    brand.setColour (juce::Label::textColourId, theme::ink);
    addChildComponent (brand);

    brandSub.setText ("VOCAL TUNING", juce::dontSendNotification);
    brandSub.setFont (theme::font (11.0f, false));
    brandSub.setColour (juce::Label::textColourId, theme::inkSoft);
    addChildComponent (brandSub);

    auto setCap = [this] (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (theme::font (12.0f, false));
        l.setColour (juce::Label::textColourId, theme::inkSoft);
        addAndMakeVisible (l);
    };
    setCap (vrLabel,  "Vocal Range");
    setCap (keyLabel, "Key");
    setCap (ksLabel,  "Scale");

    // ---- dropdowns ----
    vocalRange.addItemList (music::vocalRangeNames(), 1);
    keyBox.addItemList (music::keyNames(), 1);
    keyScale.addItemList (music::scaleNames(), 1);
    addAndMakeVisible (vocalRange);
    addAndMakeVisible (keyBox);
    addAndMakeVisible (keyScale);
    vrAtt  = std::make_unique<ComboAtt> (proc.apvts, "vocalRange", vocalRange);
    keyAtt = std::make_unique<ComboAtt> (proc.apvts, "key",        keyBox);
    ksAtt  = std::make_unique<ComboAtt> (proc.apvts, "keyScale",   keyScale);

    // ---- preset selector ----
    presetName.setJustificationType (juce::Justification::centredLeft);
    presetName.setFont (theme::font (16.0f, false));
    presetName.setColour (juce::Label::textColourId, theme::ink);
    addAndMakeVisible (presetName);

    prevBtn.onClick = [this] { stepProgram (-1); };
    nextBtn.onClick = [this] { stepProgram ( 1); };
    dotsBtn.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);
    addAndMakeVisible (dotsBtn);

    // ---- note display + meter ----
    addAndMakeVisible (noteDisplay);
    addAndMakeVisible (centsMeter);

    editNotesLabel.setText ("edit notes", juce::dontSendNotification);
    editNotesLabel.setFont (theme::font (14.0f, false));
    editNotesLabel.setColour (juce::Label::textColourId, theme::inkSoft);
    addAndMakeVisible (editNotesLabel);

    resetBtn.onClick = [this] { proc.applyScaleToNotes(); };
    addAndMakeVisible (resetBtn);

    // ---- 12 note toggles ----
    for (int i = 0; i < 12; ++i)
    {
        addAndMakeVisible (noteButtons[(size_t) i]);
        noteAtt[(size_t) i] = std::make_unique<ButtonAtt> (
            proc.apvts, "note" + juce::String (i), noteButtons[(size_t) i]);
    }

    // ---- detune + mode ----
    detuneLabel.setText ("detune", juce::dontSendNotification);
    detuneLabel.setFont (theme::font (14.0f, false));
    detuneLabel.setColour (juce::Label::textColourId, theme::inkSoft);
    addAndMakeVisible (detuneLabel);

    detuneValue.setFont (theme::font (18.0f, true));
    detuneValue.setColour (juce::Label::textColourId, theme::ink);
    detuneValue.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (detuneValue);

    configureKnob (detuneKnob, "detuneHz", detuneAtt);
    addAndMakeVisible (detuneKnob);

    modeLabel.setText ("mode", juce::dontSendNotification);
    modeLabel.setFont (theme::font (12.0f, false));
    modeLabel.setColour (juce::Label::textColourId, theme::inkSoft);
    addAndMakeVisible (modeLabel);

    modeSeg.onChange = [this] (int idx)
    {
        if (auto* param = proc.apvts.getParameter ("hq"))
            param->setValueNotifyingHost (idx == 1 ? 1.0f : 0.0f);
    };
    addAndMakeVisible (modeSeg);

    // ---- right card knobs ----
    setCap (retuneLabel,   "retune speed");
    retuneLabel.setFont (theme::font (16.0f, false));
    retuneLabel.setColour (juce::Label::textColourId, theme::ink);
    setCap (humanizeLabel, "humanize");
    setCap (flexLabel,     "flex tune");

    configureKnob (retuneKnob,   "retuneSpeed", retuneAtt);
    // Retune speed: 0 = hardest/fastest. Show that as a FULL ring on the right
    // and empty it as the value rises toward a looser, more natural glide.
    retuneKnob.setInvertedFill (true);
    configureKnob (humanizeKnob, "humanize",    humanizeAtt);
    configureKnob (flexKnob,     "flexTune",    flexAtt);
    addAndMakeVisible (retuneKnob);
    addAndMakeVisible (humanizeKnob);
    addAndMakeVisible (flexKnob);

    configureBox (retuneBox,   "retuneSpeed");
    configureBox (humanizeBox, "humanize");
    configureBox (flexBox,     "flexTune");

    // ---- Modern | Classic switch ----
    modernLabel.setText ("Modern", juce::dontSendNotification);
    modernLabel.setFont (theme::font (14.0f, false));
    modernLabel.setColour (juce::Label::textColourId, theme::ink);
    modernLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (modernLabel);

    classicLabel.setText ("Classic", juce::dontSendNotification);
    classicLabel.setFont (theme::font (14.0f, false));
    classicLabel.setColour (juce::Label::textColourId, theme::ink);
    classicLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (classicLabel);

    addAndMakeVisible (modernSwitch);
    modernAtt = std::make_unique<ButtonAtt> (proc.apvts, "modern", modernSwitch);

    // ---- icon row ----
    undoBtn.onClick  = [this] { proc.undo.undo(); };
    redoBtn.onClick  = [this] { proc.undo.redo(); };
    gearBtn.onClick  = [this]
    {
        juce::PopupMenu m;
        m.addItem (1, "Reset notes to scale");
        m.addItem (2, "VocalTune \xe2\x80\x94 real-time pitch correction", false, false);
        m.showMenuAsync (juce::PopupMenu::Options(), [this] (int r)
        {
            if (r == 1) proc.applyScaleToNotes();
        });
    };
    powerBtn.onClick = [this]
    {
        if (auto* param = proc.apvts.getParameter ("power"))
            param->setValueNotifyingHost (proc.apvts.getRawParameterValue ("power")->load() > 0.5f ? 0.0f : 1.0f);
    };
    addAndMakeVisible (undoBtn);
    addAndMakeVisible (redoBtn);
    addAndMakeVisible (gearBtn);
    addAndMakeVisible (powerBtn);

    if (plateBaked)
        setupPlateMode();

    startTimerHz (30);
    setSize (1024, plateBaked ? 680 : 640);

    // License overlay sits on top of everything; it shows itself until activated.
    addChildComponent (licenseOverlay);
    licenseOverlay.setBounds (getLocalBounds());
}

VocalTuneEditor::~VocalTuneEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
// Everything static is baked into the chassis plates; components become
// sprites (knob domes, keyboard, toggle thumb) or invisible hit areas, and
// lit states are masked from the ON plate in paintPlate().
void VocalTuneEditor::setupPlateMode()
{
    using namespace plategeo;

    // captions, wordmark, tick ruler, pill texts: all baked
    for (auto* l : { &vrLabel, &keyLabel, &ksLabel, &editNotesLabel, &detuneLabel,
                     &modeLabel, &retuneLabel, &humanizeLabel, &flexLabel,
                     &modernLabel, &classicLabel })
        l->setVisible (false);

    noteDisplay.plate = true;
    centsMeter.setVisible (false);              // drawn in paintPlate

    // 12 round buttons -> the two-octave keyboard sprite
    for (auto& b : noteButtons) b.setVisible (false);
    keyboard.isNoteOn = [this] (int i)
    {
        return proc.apvts.getRawParameterValue ("note" + juce::String (i))->load() > 0.5f;
    };
    keyboard.onToggle = [this] (int i)
    {
        if (auto* prm = proc.apvts.getParameter ("note" + juce::String (i)))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
            prm->endChangeGesture();
        }
    };
    addAndMakeVisible (keyboard);

    // knob domes: shared chrome sprites, full-360 sweep from 6 o'clock
    retuneKnob.setPlateSprite (skin::cropToDome (skin::image ("grit-knob-large@2x.png"),
                                                 0.1999f, 0.3533f, 0.199f));
    auto small = skin::cropToDome (skin::image ("grit-knob-small@2x.png"),
                                   0.4993f, 0.4648f, 0.615f);
    humanizeKnob.setPlateSprite (small);
    flexKnob.setPlateSprite (small);
    detuneKnob.setPlateSprite (small);

    modernSwitch.setPlateThumb (small);

    modeSeg.plate = true;
    modeSeg.splitFrac = (modeSeam - modeX0) / (modeX1 - modeX0);

    for (auto* b : { &prevBtn, &nextBtn, &dotsBtn, &resetBtn,
                     &undoBtn, &redoBtn, &gearBtn, &powerBtn })
        b->plate = true;

    presetName.setJustificationType (juce::Justification::centredLeft);

    // value boxes sit over baked white capsules
    for (auto* b : { &retuneBox, &humanizeBox, &flexBox })
        b->setFont (theme::font (15.0f, true));

    // detune readout is bold ink straight on the plate (no capsule in this art)
    detuneValue.setJustificationType (juce::Justification::centredLeft);
    detuneValue.setFont (theme::font (24.0f, true));
    detuneValue.setColour (juce::Label::textColourId, theme::ink);
}

//==============================================================================
void VocalTuneEditor::configureKnob (RingKnob& k, const juce::String& id,
                                     std::unique_ptr<SliderAtt>& att)
{
    k.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    att = std::make_unique<SliderAtt> (proc.apvts, id, k);
}

void VocalTuneEditor::configureBox (juce::Label& box, const juce::String& id)
{
    box.setJustificationType (juce::Justification::centred);
    box.setFont (theme::font (16.0f, false));
    box.setColour (juce::Label::textColourId, theme::ink);
    box.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    box.setEditable (true);
    box.onTextChange = [this, &box, id]
    {
        if (auto* param = proc.apvts.getParameter (id))
        {
            const float v = juce::jlimit (0.0f, 100.0f, box.getText().getFloatValue());
            param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (v));
        }
    };
    addAndMakeVisible (box);
}

//==============================================================================
void VocalTuneEditor::stepProgram (int delta)
{
    const int n = proc.getNumPrograms();
    if (n <= 0) return;
    const int idx = (proc.getCurrentProgram() + delta + n) % n;
    proc.setCurrentProgram (idx);
}

void VocalTuneEditor::showPresetMenu()
{
    juce::PopupMenu m;
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&dotsBtn),
                     [this] (int r) { if (r > 0) proc.setCurrentProgram (r - 1); });
}

//==============================================================================
void VocalTuneEditor::timerCallback()
{
    noteDisplay.setNote (proc.getDetectedNote(), proc.hasPitch());
    centsMeter.setCents (proc.getDetectedCents(), proc.hasPitch());

    if (plateBaked)
    {
        livePitch = proc.hasPitch();
        const float target = juce::jlimit (-100.0f, 100.0f, proc.getDetectedCents());
        liveCents += (target - liveCents) * 0.35f;
        keyboard.refresh();
        repaint();   // lit masks + ring wedges live in paintPlate
    }

    presetName.setText (proc.getProgramName (proc.getCurrentProgram()), juce::dontSendNotification);

    auto setBox = [] (juce::Label& box, float v)
    {
        if (! box.isBeingEdited())
            box.setText (juce::String (juce::roundToInt (v)), juce::dontSendNotification);
    };
    setBox (retuneBox,   proc.apvts.getRawParameterValue ("retuneSpeed")->load());
    setBox (humanizeBox, proc.apvts.getRawParameterValue ("humanize")->load());
    setBox (flexBox,     proc.apvts.getRawParameterValue ("flexTune")->load());

    detuneValue.setText (juce::String (proc.apvts.getRawParameterValue ("detuneHz")->load(), 1) + " Hz",
                         juce::dontSendNotification);

    modeSeg.setIndex (proc.apvts.getRawParameterValue ("hq")->load() > 0.5f ? 1 : 0);

    const bool on = proc.apvts.getRawParameterValue ("power")->load() > 0.5f;
    powerBtn.setActiveTint (on);
}

//==============================================================================
void VocalTuneEditor::paint (juce::Graphics& g)
{
    if (plateBaked)
    {
        paintPlate (g);
        return;
    }

    // warm off-white backdrop + soft top light
    theme::backdrop (g, getLocalBounds());

    auto drawCard = [&g] (juce::Rectangle<int> r, float corner)
    {
        auto rf = r.toFloat();
        theme::elevate (g, rf, corner);
        g.setColour (theme::card);
        g.fillRoundedRectangle (rf, corner);
        theme::topHighlight (g, rf, corner);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (rf, corner, 1.0f);
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
        g.drawText ("tune", bb.withTrimmedLeft ((int) vw), juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.fillRoundedRectangle ((float) bb.getX(), (float) bb.getY() + 26.0f, 22.0f, 2.5f, 1.25f);
    }
    theme::spacedText (g, "VOCAL TUNING", brandSub.getBounds().toFloat(),
                       theme::inkSoft, 9.5f, 2.6f, false, juce::Justification::left);

    drawCard (leftCard, 20.0f);
    drawCard (rightCard, 20.0f);
    drawCard (iconCard, 18.0f);

    // preset pill
    drawCard (presetPill, presetPill.getHeight() * 0.5f);

    auto box = [&g] (juce::Rectangle<int> r)
    {
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (r.toFloat(), 8.0f, 1.2f);
    };
    box (retuneBoxR);
    box (humanizeBoxR);
    box (flexBoxR);

    // hairline under "edit notes"
    g.setColour (theme::cardLine);
    const int lx = leftCard.getX() + 24;
    const int lr = leftCard.getRight() - 24;
    g.fillRect (lx, editLineY, lr - lx, 1);

    // divider above detune / mode row
    g.fillRect (lx, detuneDividerY, lr - lx, 1);

    // vertical divider between detune and mode
    g.fillRect (leftCard.getCentreX(), detuneDividerY + 12, 1, leftCard.getBottom() - detuneDividerY - 24);
}

//==============================================================================
juce::Rectangle<int> VocalTuneEditor::plateFracRect (float fx0, float fy0, float fx1, float fy1) const
{
    const float W = (float) getWidth(), H = (float) getHeight();
    return juce::Rectangle<float> (fx0 * W, fy0 * H, (fx1 - fx0) * W, (fy1 - fy0) * H)
               .toNearestInt();
}

// Blit the matching region of the lit plate over the base plate — pixel
// registration is guaranteed because both images share the same canvas.
void VocalTuneEditor::maskFromOn (juce::Graphics& g, juce::Rectangle<int> screenRect)
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
void VocalTuneEditor::maskFromOnFeathered (juce::Graphics& g, juce::Rectangle<int> screenRect,
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
void VocalTuneEditor::drawRingWedge (juce::Graphics& g, juce::Slider& s, float cxFrac, float cyFrac,
                                     float domeRFrac, float solidRFrac, float maxRFrac, bool inverted)
{
    const float W = (float) getWidth();
    const juce::Point<float> c (cxFrac * W, cyFrac * (float) getHeight());
    const float domeR  = domeRFrac  * W;
    const float solidR = solidRFrac * W;
    const float R      = maxRFrac   * W;

    float prop = juce::jlimit (0.0f, 1.0f,
                               (float) s.valueToProportionOfLength (s.getValue()));
    if (inverted) prop = 1.0f - prop;
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

void VocalTuneEditor::paintPlate (juce::Graphics& g)
{
    using namespace plategeo;

    g.drawImage (chassisImg, getLocalBounds().toFloat(),
                 juce::RectanglePlacement::stretchToFit);

    const int feather = juce::roundToInt ((float) getWidth() * 0.008f);

    // ---- mode pill: selected segment lights (seam-to-seam)
    if (modeSeg.getIndex() == 0)
        maskFromOnFeathered (g, plateFracRect (modeX0, modeY0, modeSeam, modeY1), feather);
    else
        maskFromOnFeathered (g, plateFracRect (modeSeam, modeY0, modeX1, modeY1), feather);

    // ---- Modern/Classic toggle track lights when Modern is on
    if (modernSwitch.getToggleState())
        maskFromOnFeathered (g, plateFracRect (togX0, togY0, togX1, togY1), feather);

    // ---- bottom icon buttons: power follows the param, the rest flash on press
    const bool powerOn = proc.apvts.getRawParameterValue ("power")->load() > 0.5f;
    juce::Button* bot[4] = { &undoBtn, &redoBtn, &gearBtn, &powerBtn };
    for (int i = 0; i < 4; ++i)
    {
        const bool lit = (i == 3) ? powerOn : bot[i]->isDown();
        if (lit)
            maskFromOnFeathered (g, plateFracRect (botCx[i] - botMaskR, botCy - botMaskR * 2048.0f / 1360.0f,
                                                   botCx[i] + botMaskR, botCy + botMaskR * 2048.0f / 1360.0f),
                                 feather);
    }

    // ---- cents meter: glowing block masked from the fully lit groove
    if (livePitch)
    {
        const auto groove = plateFracRect (metX0, metY0, metX1, metY1).toFloat();
        const float blockW = groove.getWidth() * 0.14f;
        const float norm = (liveCents + 100.0f) / 200.0f;
        const float bx = groove.getX() + norm * (groove.getWidth() - blockW);
        juce::Rectangle<float> block (bx, groove.getY(), blockW, groove.getHeight());

        juce::Path clip;
        clip.addRoundedRectangle (block, block.getHeight() * 0.45f);
        g.saveState();
        g.reduceClipRegion (clip);
        maskFromOn (g, block.toNearestInt());
        g.restoreState();
    }

    // ---- knob neon ring wedges
    drawRingWedge (g, retuneKnob,   heroCx, heroCy, heroDomeR, heroSolidR, heroMaxR, true);
    drawRingWedge (g, humanizeKnob, humCx,  humCy,  smallDomeR, smallSolidR, smallMaxR, false);
    drawRingWedge (g, flexKnob,     flexCx, flexCy, smallDomeR, smallSolidR, smallMaxR, false);
    drawRingWedge (g, detuneKnob,   detCx,  detCy,  detDomeR,  detSolidR,  detMaxR,  false);
}

void VocalTuneEditor::layoutPlate()
{
    using namespace plategeo;
    auto fr = [this] (float fx0, float fy0, float fx1, float fy1)
    {
        return plateFracRect (fx0, fy0, fx1, fy1);
    };
    const float W = (float) getWidth();

    // header
    vocalRange.setBounds (fr (vrX0, headY0, vrX1, headY1));
    keyBox.setBounds     (fr (keyX0, headY0, keyX1, headY1));
    keyScale.setBounds   (fr (scX0, headY0, scX1, headY1));

    presetName.setBounds (fr (presX0, presY0, presX1, presY1).withTrimmedLeft (10));
    nextBtn.setBounds (fr (nextX0, btnY0, nextX1, btnY1));
    prevBtn.setBounds (fr (prevX0, btnY0, prevX1, btnY1));
    dotsBtn.setBounds (fr (dotsX0, btnY0, dotsX1, btnY1));
    brand.setBounds (0, 0, 0, 0);
    brandSub.setBounds (0, 0, 0, 0);

    // left card
    noteDisplay.setBounds (fr (dispX0, dispY0, dispX1, dispY1));
    centsMeter.setBounds  (fr (metX0, metY0, metX1, metY1));
    resetBtn.setBounds    (fr (rstX0, rstY0, rstX1, rstY1));
    keyboard.setBounds    (fr (kbX0, kbY0, kbX1, kbY1));

    detuneValue.setBounds (fr (detValX0, detValY0, detValX1, detValY1));
    modeSeg.setBounds     (fr (modeX0, modeY0, modeX1, modeY1));

    // knobs: bounds are a square around the dome (sprite crop pads by 1.06)
    auto domeSquare = [&] (float cx, float cy, float diaFrac)
    {
        const float side = diaFrac * W * 1.06f;
        return juce::Rectangle<float> (cx * W - side * 0.5f,
                                       cy * (float) getHeight() - side * 0.5f,
                                       side, side).toNearestInt();
    };
    retuneKnob.setBounds   (domeSquare (heroCx, heroCy, heroDomeDia));
    humanizeKnob.setBounds (domeSquare (humCx,  humCy,  smallDomeDia));
    flexKnob.setBounds     (domeSquare (flexCx, flexCy, smallDomeDia));
    detuneKnob.setBounds   (domeSquare (detCx,  detCy,  detDomeDia));

    retuneBox.setBounds   (fr (retValX0, retValY0, retValX1, retValY1));
    humanizeBox.setBounds (fr (humValX0, smallValY0, humValX1, smallValY1));
    flexBox.setBounds     (fr (flexValX0, smallValY0, flexValX1, smallValY1));

    modernSwitch.setBounds (fr (togX0, togY0, togX1, togY1));

    // bottom icon buttons
    const float aspect = 2048.0f / 1360.0f;
    for (int i = 0; i < 4; ++i)
    {
        juce::Button* bs[4] = { &undoBtn, &redoBtn, &gearBtn, &powerBtn };
        bs[i]->setBounds (fr (botCx[i] - botR, botCy - botR * aspect,
                              botCx[i] + botR, botCy + botR * aspect));
    }

    // unused vector-layout rectangles
    leftCard = rightCard = iconCard = presetPill = {};
    retuneBoxR = humanizeBoxR = flexBoxR = {};
    editLineY = detuneDividerY = 0;
}

//==============================================================================
void VocalTuneEditor::resized()
{
    licenseOverlay.setBounds (getLocalBounds());

    if (plateBaked)
    {
        layoutPlate();
        return;
    }

    auto r = getLocalBounds();

    const int margin = 16;
    const int rightX = 678;
    const int rightW = getWidth() - rightX - margin;  // ~330

    // ---- top bar ----
    brand.setBounds (28, 24, 200, 34);
    brandSub.setBounds (30, 56, 160, 16);

    vrLabel.setBounds (232, 22, 160, 18);
    vocalRange.setBounds (232, 42, 150, 38);   // ends 382
    keyLabel.setBounds (394, 22, 60, 18);
    keyBox.setBounds (394, 42, 66, 38);         // ends 460
    ksLabel.setBounds (472, 22, 160, 18);
    keyScale.setBounds (472, 42, 150, 38);      // ends 622

    presetPill = juce::Rectangle<int> (rightX, 36, rightW, 44);
    {
        auto pp = presetPill.reduced (6, 4);
        dotsBtn.setBounds (pp.removeFromRight (30));
        nextBtn.setBounds (pp.removeFromRight (28));
        prevBtn.setBounds (pp.removeFromRight (28));
        presetName.setBounds (pp.withTrimmedLeft (8));
    }

    // ---- cards ----
    leftCard  = juce::Rectangle<int> (margin, 96, rightX - margin - 16, getHeight() - 96 - margin);
    rightCard = juce::Rectangle<int> (rightX, 96, rightW, 452);
    iconCard  = juce::Rectangle<int> (rightX, rightCard.getBottom() + 12, rightW,
                                      getHeight() - rightCard.getBottom() - 12 - margin);

    // ---- left card contents ----
    auto inner = leftCard.reduced (24, 18);

    noteDisplay.setBounds (inner.getX(), inner.getY() + 6, inner.getWidth(), 120);

    centsMeter.setBounds (inner.getX() + 16, noteDisplay.getBottom() + 4,
                          inner.getWidth() - 32, 78);

    const int editY = centsMeter.getBottom() + 18;
    editNotesLabel.setBounds (inner.getX(), editY, 120, 22);
    resetBtn.setBounds (inner.getRight() - 26, editY - 2, 26, 26);
    editLineY = editY + 28;

    // 12 note buttons
    const int nb = 12;
    const int dia = 50;
    const int totalW = inner.getWidth();
    const int gap = (totalW - nb * dia) / (nb - 1);
    int x = inner.getX();
    const int by = editLineY + 22;
    for (int i = 0; i < nb; ++i)
    {
        noteButtons[(size_t) i].setBounds (x, by, dia, dia);
        x += dia + gap;
    }

    // detune / mode row
    detuneDividerY = by + dia + 26;
    auto rowL = juce::Rectangle<int> (inner.getX(), detuneDividerY + 14,
                                      leftCard.getCentreX() - inner.getX() - 16, 60);
    detuneLabel.setBounds (rowL.getX(), rowL.getY(), 120, 18);
    detuneValue.setBounds (rowL.getX(), rowL.getY() + 18, 110, 26);
    detuneKnob.setBounds (rowL.getX() + 120, rowL.getY() - 4, 56, 56);

    auto rowR = juce::Rectangle<int> (leftCard.getCentreX() + 16, detuneDividerY + 14,
                                      inner.getRight() - leftCard.getCentreX() - 16, 60);
    modeLabel.setBounds (rowR.getX(), rowR.getY() - 2, 120, 16);
    modeSeg.setBounds (rowR.getX(), rowR.getY() + 16, 200, 34);

    // ---- right card contents ----
    auto ri = rightCard.reduced (22, 18);

    retuneLabel.setBounds (ri.getX(), ri.getY(), 180, 22);
    retuneBoxR = juce::Rectangle<int> (ri.getRight() - 58, ri.getY() - 2, 58, 32);
    retuneBox.setBounds (retuneBoxR);

    const int bigDia = 188;
    retuneKnob.setBounds (ri.getCentreX() - bigDia / 2, ri.getY() + 36, bigDia, bigDia);

    const int subY = retuneKnob.getBottom() + 18;
    const int smallDia = 86;
    const int colLW = ri.getWidth() / 2;
    const int leftCx = ri.getX() + colLW / 2 - 10;
    const int rightCx = ri.getRight() - colLW / 2 + 10;

    humanizeLabel.setBounds (leftCx - 60, subY, 120, 18);
    flexLabel.setBounds (rightCx - 60, subY, 120, 18);

    humanizeKnob.setBounds (leftCx - smallDia / 2, subY + 20, smallDia, smallDia);
    flexKnob.setBounds (rightCx - smallDia / 2, subY + 20, smallDia, smallDia);

    humanizeBoxR = juce::Rectangle<int> (leftCx - 28, humanizeKnob.getBottom() + 4, 56, 30);
    flexBoxR     = juce::Rectangle<int> (rightCx - 28, flexKnob.getBottom() + 4, 56, 30);
    humanizeBox.setBounds (humanizeBoxR);
    flexBox.setBounds (flexBoxR);

    // Modern | Classic
    const int swY = rightCard.getBottom() - 40;
    modernSwitch.setBounds (ri.getCentreX() - 26, swY, 52, 28);
    modernLabel.setBounds (ri.getX(), swY + 3, ri.getCentreX() - 26 - ri.getX() - 8, 22);
    classicLabel.setBounds (ri.getCentreX() + 34, swY + 3, ri.getRight() - (ri.getCentreX() + 34), 22);

    // ---- icon row ----
    auto ic = iconCard.reduced (18, 0);
    const int n = 4;
    const int idia = 40;
    const int igap = (ic.getWidth() - n * idia) / (n - 1);
    int ix = ic.getX();
    const int iy = ic.getCentreY() - idia / 2;
    IconButton* icons[] = { &undoBtn, &redoBtn, &gearBtn, &powerBtn };
    for (auto* b : icons) { b->setBounds (ix, iy, idia, idia); ix += idia + igap; }

    juce::ignoreUnused (r);
}
