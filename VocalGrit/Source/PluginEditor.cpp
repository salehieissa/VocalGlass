#include "PluginEditor.h"
#include "ui/Theme.h"

//==============================================================================
static juce::String toPercent (juce::Slider& s)
{
    const auto prop = s.valueToProportionOfLength (s.getValue());
    return juce::String (juce::roundToInt ((float) prop * 100.0f)) + "%";
}

void VSlider::setup (juce::AudioProcessorValueTreeState& apvts, const juce::String& id,
                     const juce::String& text, juce::Component& parent)
{
    parent.addAndMakeVisible (slider);

    name.setText (text, juce::dontSendNotification);
    name.setJustificationType (juce::Justification::centred);
    name.setFont (theme::font (12.0f, false));
    name.setColour (juce::Label::textColourId, theme::inkSoft);
    parent.addAndMakeVisible (name);

    value.setJustificationType (juce::Justification::centred);
    value.setFont (theme::font (14.0f, true));
    value.setColour (juce::Label::textColourId, theme::ink);
    parent.addAndMakeVisible (value);

    att = std::make_unique<SliderAtt> (apvts, id, slider);
    slider.onValueChange = [this] { refresh(); };
    refresh();
}

void VSlider::refresh()
{
    value.setText (fmt ? fmt (slider) : toPercent (slider), juce::dontSendNotification);
}

void VSlider::setFormat (std::function<juce::String (juce::Slider&)> f)
{
    fmt = std::move (f);
    refresh();
}

void VSlider::layout (juce::Rectangle<int> cell)
{
    name.setBounds (cell.removeFromTop (16));
    value.setBounds (cell.removeFromTop (18));
    slider.setBounds (cell.reduced (cell.getWidth() / 2 - 16, 6));
}

//==============================================================================
void Knob::setup (juce::AudioProcessorValueTreeState& apvts, const juce::String& id,
                  const juce::String& text, juce::Component& parent)
{
    parent.addAndMakeVisible (slider);

    name.setText (text, juce::dontSendNotification);
    name.setJustificationType (juce::Justification::centred);
    name.setFont (theme::font (11.5f, false));
    name.setColour (juce::Label::textColourId, theme::inkSoft);
    parent.addAndMakeVisible (name);

    value.setJustificationType (juce::Justification::centred);
    value.setFont (theme::font (12.0f, true));
    value.setColour (juce::Label::textColourId, theme::ink);
    parent.addAndMakeVisible (value);

    fmt = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    att = std::make_unique<SliderAtt> (apvts, id, slider);
    slider.onValueChange = [this] { refresh(); };
    refresh();
}

void Knob::layout (juce::Rectangle<int> cell)
{
    name.setBounds (cell.removeFromTop (15));
    value.setBounds (cell.removeFromBottom (16));
    slider.setBounds (cell);
}

void Knob::refresh()
{
    value.setText (fmt ? fmt (slider.getValue()) : juce::String (slider.getValue(), 2),
                   juce::dontSendNotification);
}

void Knob::asPercent()
{
    fmt = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    refresh();
}

void Knob::asMillis()
{
    fmt = [] (double v) { return juce::String (juce::roundToInt (v)) + " ms"; };
    refresh();
}

//==============================================================================
VocalGritEditor::VocalGritEditor (VocalGritProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      inMeter (p.inputLevel), outMeter (p.outputLevel)
{
    setLookAndFeel (&lnf);

    // --- preset browser ---
    presetPrev.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    presetNext.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    presetPrev.onClick = [this] { stepPreset (-1); };
    presetNext.onClick = [this] { stepPreset (+1); };
    addAndMakeVisible (presetPrev);
    addAndMakeVisible (presetNext);

    presetName.setJustificationType (juce::Justification::centred);
    presetName.setFont (theme::font (15.0f, false));
    presetName.setColour (juce::Label::textColourId, theme::ink);
    presetName.setText (proc.getProgramName (proc.getCurrentProgram()), juce::dontSendNotification);
    addAndMakeVisible (presetName);

    // --- grit dial ---
    gritDial.setCaption ("add bite without wrecking the vocal");
    addAndMakeVisible (gritDial);
    gritAtt = std::make_unique<SliderAtt> (proc.apvts, "drive", gritDial);

    driveS  .setup (proc.apvts, "bias",        "drive",   *this);
    toneS   .setup (proc.apvts, "tone",        "tone",    *this);
    widthS  .setup (proc.apvts, "stereoWidth", "width",   *this);
    formantS.setup (proc.apvts, "formant",     "formant", *this);

    // Tone reads in Hz/kHz, not a meaningless percentage.
    toneS.setFormat ([] (juce::Slider& s)
    {
        const double hz = s.getValue();
        return hz >= 1000.0 ? juce::String (hz / 1000.0, 1) + " kHz"
                            : juce::String (juce::roundToInt (hz)) + " Hz";
    });

    // Formant is bipolar: show it as +/- semitones, "0" at the centre detent.
    formantS.setFormat ([] (juce::Slider& s)
    {
        const int st = juce::roundToInt ((s.getValue() - 0.5) * 2.0 * 12.0);
        return st == 0 ? juce::String ("0 st")
                       : (st > 0 ? "+" : "") + juce::String (st) + " st";
    });

    // --- character buttons ---
    const char* charNames[] = { "clean", "warm", "dirty", "blown" };
    for (int i = 0; i < 4; ++i)
    {
        auto& b = charButtons[(size_t) i];
        b.setButtonText (charNames[i]);
        b.setClickingTogglesState (true);
        b.setRadioGroupId (100);
        b.onClick = [this, i] { setMode (i); };
        addAndMakeVisible (b);
    }

    // --- texture pills ---
    const char* pillNames[] = { "fuzz", "amp", "speaker", "presence" };
    const char* pillIds[]   = { "fuzzOn", "ampOn", "speakerOn", "presenceOn" };
    for (int i = 0; i < 4; ++i)
    {
        auto& b = pills[(size_t) i];
        b.setButtonText (pillNames[i]);
        addAndMakeVisible (b);
        buttonAtts.push_back (std::make_unique<ButtonAtt> (proc.apvts, pillIds[i], b));
    }

    // --- right card: meters + mix ---
    auto labelInit = [this] (juce::Label& l, const juce::String& t, float sz, juce::Colour c)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (theme::font (sz, false));
        l.setColour (juce::Label::textColourId, c);
        addAndMakeVisible (l);
    };
    labelInit (inMeterLabel,  "input level",  13.0f, theme::ink);
    labelInit (outMeterLabel, "output level", 13.0f, theme::ink);
    labelInit (mixLabel,      "mix",          14.0f, theme::ink);
    labelInit (rawLabel,      "raw vocal",    11.0f, theme::inkSoft);
    labelInit (procLabel,     "processed vocal", 11.0f, theme::inkSoft);
    procLabel.setJustificationType (juce::Justification::right);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    mixValue.setJustificationType (juce::Justification::right);
    mixValue.setFont (theme::font (16.0f, true));
    mixValue.setColour (juce::Label::textColourId, theme::accent);
    addAndMakeVisible (mixValue);
    addAndMakeVisible (mixSlider);
    mixAtt = std::make_unique<SliderAtt> (proc.apvts, "mix", mixSlider);
    mixSlider.onValueChange = [this] { mixValue.setText (toPercent (mixSlider), juce::dontSendNotification); };
    mixValue.setText (toPercent (mixSlider), juce::dontSendNotification);

    // Output trim (dB).
    labelInit (outputLabel, "output", 14.0f, theme::ink);
    outputValue.setJustificationType (juce::Justification::right);
    outputValue.setFont (theme::font (16.0f, true));
    outputValue.setColour (juce::Label::textColourId, theme::accent);
    addAndMakeVisible (outputValue);
    addAndMakeVisible (outputSlider);
    outputAtt = std::make_unique<SliderAtt> (proc.apvts, "output", outputSlider);
    auto fmtDb = [this]
    {
        const double db = outputSlider.getValue();
        outputValue.setText ((db > 0.0 ? "+" : "") + juce::String (db, 1) + " dB",
                             juce::dontSendNotification);
    };
    outputSlider.onValueChange = fmtDb;
    fmtDb();

    // --- FX modules ---
    setupFxModule (doublerOn, "doublerOn",
                   dblA, "doublerDetune", "detune",
                   dblB, "doublerWidth",  "width",
                   dblC, "doublerMix",    "mix");
    setupFxModule (delayOn, "delayOn",
                   delA, "delayTime",     "time",
                   delB, "delayFeedback", "feedback",
                   delC, "delayMix",      "mix");
    setupFxModule (reverbOn, "reverbOn",
                   revA, "reverbSize", "size",
                   revB, "reverbDamp", "damp",
                   revC, "reverbMix",  "mix");
    setupFxModule (glitchOn, "glitchOn",
                   glA, "glitchRate",  "rate",
                   glB, "glitchDepth", "depth",
                   glC, "glitchMix",   "mix");

    // Clean value readouts.
    for (auto* k : { &dblA, &dblB, &dblC, &delB, &delC, &revA, &revB, &revC, &glB, &glC })
        k->asPercent();
    delA.asMillis();

    // Glitch rate snaps to a note division — show the division name.
    glA.fmt = [] (double v)
    {
        static const char* names[6] = { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
        return juce::String (names[juce::jlimit (0, 5, juce::roundToInt (v * 5.0))]);
    };
    glA.refresh();

    // --- DELAY host-tempo sync: compact toggle + division selector ---
    delaySyncBtn.setButtonText ("sync");
    addAndMakeVisible (delaySyncBtn);
    delaySyncAtt = std::make_unique<ButtonAtt> (proc.apvts, "delaySync", delaySyncBtn);
    delaySyncBtn.onClick = [this] { updateDelaySyncUI(); };

    delayDivBox.addItemList ({ "1/1", "1/2", "1/2.", "1/2T", "1/4", "1/4.", "1/4T",
                               "1/8", "1/8.", "1/8T", "1/16", "1/16.", "1/16T", "1/32" }, 1);
    delayDivBox.setJustificationType (juce::Justification::centred);
    delayDivBox.setColour (juce::ComboBox::backgroundColourId, juce::Colours::white);
    delayDivBox.setColour (juce::ComboBox::textColourId,       theme::ink);
    delayDivBox.setColour (juce::ComboBox::outlineColourId,    theme::cardLine);
    delayDivBox.setColour (juce::ComboBox::arrowColourId,      theme::accent);
    addAndMakeVisible (delayDivBox);
    delayDivAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                      (proc.apvts, "delayDiv", delayDivBox);
    updateDelaySyncUI();

    startTimerHz (30);
    setSize (1000, 808);
}

VocalGritEditor::~VocalGritEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void VocalGritEditor::setupFxModule (juce::ToggleButton& toggle, const juce::String& toggleId,
                                     Knob& a, const juce::String& aId, const juce::String& aName,
                                     Knob& b, const juce::String& bId, const juce::String& bName,
                                     Knob& c, const juce::String& cId, const juce::String& cName)
{
    toggle.setButtonText ("on");
    addAndMakeVisible (toggle);
    buttonAtts.push_back (std::make_unique<ButtonAtt> (proc.apvts, toggleId, toggle));
    a.setup (proc.apvts, aId, aName, *this);
    b.setup (proc.apvts, bId, bName, *this);
    c.setup (proc.apvts, cId, cName, *this);
}

void VocalGritEditor::updateDelaySyncUI()
{
    const bool synced = proc.apvts.getRawParameterValue ("delaySync")->load() > 0.5f;

    // When synced the division box becomes the time control + readout; when
    // free, the regular time knob (ms) is shown instead.
    delayDivBox.setVisible (synced);
    delA.slider.setVisible (! synced);
    delA.value.setVisible  (! synced);
    delA.name.setText (synced ? "div" : "time", juce::dontSendNotification);
}

void VocalGritEditor::setMode (int index)
{
    if (auto* pm = proc.apvts.getParameter ("mode"))
        pm->setValueNotifyingHost (pm->convertTo0to1 ((float) index));
}

void VocalGritEditor::stepPreset (int delta)
{
    const int n = proc.getNumPrograms();
    if (n <= 0) return;
    int idx = (proc.getCurrentProgram() + delta + n) % n;
    proc.setCurrentProgram (idx);
    presetName.setText (proc.getProgramName (idx), juce::dontSendNotification);
}

void VocalGritEditor::timerCallback()
{
    // Sync character pill states from the 'mode' parameter.
    if (auto* pm = proc.apvts.getRawParameterValue ("mode"))
    {
        const int mode = (int) pm->load();
        for (int i = 0; i < 4; ++i)
            charButtons[(size_t) i].setToggleState (i == mode, juce::dontSendNotification);
    }
    presetName.setText (proc.getProgramName (proc.getCurrentProgram()), juce::dontSendNotification);

    // Keep the delay sync UI in step with presets / host automation.
    updateDelaySyncUI();

    // advance the glow pulse and repaint just the character row glow band
    pulsePhase += 0.13f;
    if (pulsePhase > juce::MathConstants<float>::twoPi)
        pulsePhase -= juce::MathConstants<float>::twoPi;

    // Repaint the halo bands every tick so glows animate when on and clear
    // promptly when toggled off (including via presets/automation).
    for (auto& b : charButtons) repaint (b.getBounds().expanded (18));
    for (auto& b : pills)       repaint (b.getBounds().expanded (18));
    for (auto* b : { &doublerOn, &delayOn, &reverbOn, &glitchOn })
        repaint (b->getBounds().expanded (18));
}

//==============================================================================
void VocalGritEditor::paint (juce::Graphics& g)
{
    theme::backdrop (g, getLocalBounds());

    // Mood: a faint accent bloom behind the hero dial.
    theme::accentBloom (g, { 178.0f, 270.0f }, 240.0f, 0.06f);

    auto card = [&] (juce::Rectangle<int> r, float radius = 18.0f)
    {
        auto rf = r.toFloat();
        theme::elevate (g, rf, radius);
        g.setColour (theme::card);
        g.fillRoundedRectangle (rf, radius);
        theme::topHighlight (g, rf, radius);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (rf, radius, 1.0f);
    };

    // Wordmark: solid ink "vocal" + accent "grit", with a spaced tagline.
    {
        auto wm = theme::font (27.0f, true);
        g.setFont (wm);
        const float vw = juce::GlyphArrangement::getStringWidth (wm, "vocal");
        g.setColour (theme::ink);
        g.drawText ("vocal", 24, 16, 120, 32, juce::Justification::left);
        g.setColour (theme::accent);
        g.drawText ("grit", 24 + (int) vw, 16, 120, 32, juce::Justification::left);
        // accent underline tick under the wordmark
        g.setColour (theme::accent);
        g.fillRoundedRectangle (24.0f, 45.0f, 22.0f, 2.5f, 1.25f);
    }
    theme::spacedText (g, "VOCAL TEXTURE",
                       juce::Rectangle<float> (54.0f, 41.0f, 220.0f, 14.0f),
                       theme::inkSoft, 9.5f, 2.6f, false, juce::Justification::left);

    // Cards first (so glows can sit on top of card fills, behind the controls).
    card (presetArea, presetArea.getHeight() * 0.5f);
    card (rightCardArea);

    const char* titles[] = { "DOUBLER", "DELAY", "REVERB", "GLITCH" };
    const char* kinds[]  = { "doubler", "delay", "reverb", "glitch" };
    for (int i = 0; i < 4; ++i)
    {
        card (fxAreas[(size_t) i]);
        auto head = fxAreas[(size_t) i].reduced (16, 12).removeFromTop (18).toFloat();
        auto iconBox = head.removeFromLeft (16.0f);
        theme::moduleIcon (g, kinds[i], iconBox.withSizeKeepingCentre (15.0f, 15.0f), theme::accent);
        theme::spacedText (g, titles[i], head.withTrimmedLeft (7.0f),
                           theme::ink, 11.5f, 2.2f, true, juce::Justification::left);
        // dotted divider under the header
        auto fa = fxAreas[(size_t) i];
        theme::dottedDivider (g, (float) fa.getX() + 16.0f, (float) fa.getRight() - 16.0f,
                              (float) fa.getY() + 38.0f, theme::cardLine);
    }

    // Glow behind active toggles (pulsing) — drawn after cards, behind controls.
    const float pulse = 0.5f + 0.5f * std::sin (pulsePhase);
    auto pillGlow = [&] (juce::Component& b, float base, float amp, int radius)
    {
        juce::Path p;
        p.addRoundedRectangle (b.getBounds().toFloat(), b.getHeight() * 0.5f);
        theme::glowPath (g, p, base + amp * pulse, radius);
    };

    for (auto& b : charButtons)
        if (b.getToggleState()) pillGlow (b, 0.22f, 0.22f, 20);

    for (auto& b : pills)
        if (b.getToggleState()) pillGlow (b, 0.16f, 0.18f, 16);

    for (auto* b : { &doublerOn, &delayOn, &reverbOn, &glitchOn })
        if (b->getToggleState()) pillGlow (*b, 0.14f, 0.16f, 14);
}

void VocalGritEditor::resized()
{
    auto content = getLocalBounds().reduced (24);

    // Header (wordmark drawn in paint; preset box centred)
    auto header = content.removeFromTop (52);
    presetArea = header.withSizeKeepingCentre (320, 46);
    {
        auto pb = presetArea;
        presetPrev.setBounds (pb.removeFromLeft (46));
        presetNext.setBounds (pb.removeFromRight (46));
        presetName.setBounds (pb);
    }
    content.removeFromTop (16);

    // FX row at the bottom
    auto fxRow = content.removeFromBottom (176);
    {
        const int gap = 14;
        const int cardW = (fxRow.getWidth() - gap * 3) / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto cardR = fxRow.removeFromLeft (i < 3 ? cardW : fxRow.getWidth());
            if (i < 3) fxRow.removeFromLeft (gap);
            fxAreas[(size_t) i] = cardR;
        }

        auto layoutFx = [] (juce::Rectangle<int> cardR, juce::ToggleButton& toggle,
                            Knob& a, Knob& b, Knob& c)
        {
            auto inner = cardR.reduced (14, 12);
            auto top = inner.removeFromTop (24);
            toggle.setBounds (top.removeFromRight (62));
            inner.removeFromTop (6);
            const int kw = inner.getWidth() / 3;
            a.layout (inner.removeFromLeft (kw));
            b.layout (inner.removeFromLeft (kw));
            c.layout (inner);
        };
        layoutFx (fxAreas[0], doublerOn, dblA, dblB, dblC);
        layoutFx (fxAreas[2], reverbOn,  revA, revB, revC);
        layoutFx (fxAreas[3], glitchOn,  glA,  glB,  glC);

        // Delay card: regular knob layout + a compact SYNC toggle in the top
        // row and a division selector that overlays the time slot when synced.
        {
            auto inner = fxAreas[1].reduced (14, 12);
            auto top = inner.removeFromTop (24);
            delayOn.setBounds (top.removeFromRight (62));
            top.removeFromRight (8);
            delaySyncBtn.setBounds (top.removeFromRight (58));
            inner.removeFromTop (6);
            const int kw = inner.getWidth() / 3;
            auto timeCell = inner.removeFromLeft (kw);
            delA.layout (timeCell);
            auto box = timeCell;
            box.removeFromTop (15);     // name row
            box.removeFromBottom (16);  // value row
            delayDivBox.setBounds (box.withSizeKeepingCentre (
                box.getWidth() - 2, juce::jmin (26, box.getHeight())));
            delB.layout (inner.removeFromLeft (kw));
            delC.layout (inner);
        }
    }
    content.removeFromBottom (16);

    // Right card (meters + mix)
    rightCardArea = content.removeFromRight (300);
    content.removeFromRight (22);
    {
        auto rc = rightCardArea.reduced (22);
        inMeterLabel.setBounds (rc.removeFromTop (20));
        inMeter.setBounds (rc.removeFromTop (28));
        rc.removeFromTop (20);
        outMeterLabel.setBounds (rc.removeFromTop (20));
        outMeter.setBounds (rc.removeFromTop (28));
        rc.removeFromTop (30);
        auto mixHeader = rc.removeFromTop (24);
        mixLabel.setBounds (mixHeader.removeFromLeft (mixHeader.getWidth() / 2));
        mixValue.setBounds (mixHeader);
        rc.removeFromTop (4);
        mixSlider.setBounds (rc.removeFromTop (30));
        rc.removeFromTop (4);
        auto cap = rc.removeFromTop (18);
        rawLabel.setBounds (cap.removeFromLeft (cap.getWidth() / 2));
        procLabel.setBounds (cap);

        rc.removeFromTop (24);
        auto outHeader = rc.removeFromTop (24);
        outputLabel.setBounds (outHeader.removeFromLeft (outHeader.getWidth() / 2));
        outputValue.setBounds (outHeader);
        rc.removeFromTop (4);
        outputSlider.setBounds (rc.removeFromTop (30));
    }

    // Dial + vertical sliders
    auto dialAndSliders = content.removeFromTop (300);
    gritDial.setBounds (dialAndSliders.removeFromLeft (300));
    dialAndSliders.removeFromLeft (10);
    {
        auto sliders = dialAndSliders;
        const int sw = sliders.getWidth() / 4;
        driveS  .layout (sliders.removeFromLeft (sw));
        toneS   .layout (sliders.removeFromLeft (sw));
        widthS  .layout (sliders.removeFromLeft (sw));
        formantS.layout (sliders);
    }

    content.removeFromTop (8);

    // Character buttons
    auto charRow = content.removeFromTop (52);
    {
        const int gap = 12;
        const int bw = (charRow.getWidth() - gap * 3) / 4;
        for (int i = 0; i < 4; ++i)
        {
            charButtons[(size_t) i].setBounds (charRow.removeFromLeft (bw).reduced (0, 6));
            if (i < 3) charRow.removeFromLeft (gap);
        }
    }

    content.removeFromTop (12);

    // Texture pills
    auto pillRow = content.removeFromTop (52);
    {
        const int gap = 12;
        const int bw = (pillRow.getWidth() - gap * 3) / 4;
        for (int i = 0; i < 4; ++i)
        {
            pills[(size_t) i].setBounds (pillRow.removeFromLeft (bw).reduced (0, 6));
            if (i < 3) pillRow.removeFromLeft (gap);
        }
    }
}
