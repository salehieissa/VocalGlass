#include "PluginEditor.h"
#include "Scales.h"

//==============================================================================
VocalTuneEditor::VocalTuneEditor (VocalTuneProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);

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

    startTimerHz (30);
    setSize (1024, 640);

    // License overlay sits on top of everything; it shows itself until activated.
    addChildComponent (licenseOverlay);
    licenseOverlay.setBounds (getLocalBounds());
}

VocalTuneEditor::~VocalTuneEditor()
{
    setLookAndFeel (nullptr);
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
void VocalTuneEditor::resized()
{
    licenseOverlay.setBounds (getLocalBounds());

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
