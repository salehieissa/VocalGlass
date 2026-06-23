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
VocalQEditor::VocalQEditor (VocalQProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), display (p)
{
    setLookAndFeel (&lnf);

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

    bindBand (selectedBand);
    startTimerHz (30);
    setSize (1024, 740);
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
}

//==============================================================================
void VocalQEditor::paint (juce::Graphics& g)
{
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
void VocalQEditor::resized()
{
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
