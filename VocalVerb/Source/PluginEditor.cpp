#include "PluginEditor.h"

namespace
{
    juce::String pctStr  (double v) { return juce::String (juce::roundToInt (v)) + "%"; }
    juce::String hzStr   (double v) { return juce::String (juce::roundToInt (v)) + " Hz"; }
    juce::String msStr   (double v) { return juce::String (juce::roundToInt (v)) + " ms"; }
    juce::String dbStr   (double v) { return juce::String (juce::roundToInt (v)) + " dB"; }
    juce::String secStr  (double v) { return juce::String (v, 2) + " s"; }
    juce::String multStr (double v) { return juce::String (v, 2) + "x"; }
    juce::String rateStr (double v) { return juce::String (v, 2) + " Hz"; }
}

//==============================================================================
VocalVerbEditor::VocalVerbEditor (VocalVerbProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);

    // ---- branding ----
    // The wordmark + subtitle are rendered in paint() (two-tone wordmark, spaced
    // subtitle). These labels remain as invisible layout anchors so geometry is
    // unchanged.
    brand.setText ({}, juce::dontSendNotification);
    brand.setFont (theme::font (34.0f, true));
    addAndMakeVisible (brand);

    brandSub.setText ({}, juce::dontSendNotification);
    brandSub.setFont (theme::font (15.0f, false));
    addAndMakeVisible (brandSub);

    // ---- top-right controls ----
    bypassBtn.setClickingTogglesState (true);
    addAndMakeVisible (bypassBtn);
    bypassAtt = std::make_unique<ButtonAtt> (proc.apvts, "bypass", bypassBtn);

    addAndMakeVisible (linkBtn);
    addAndMakeVisible (dotsBtn);
    dotsBtn.onClick = [this]
    {
        juce::PopupMenu m;
        for (int i = 0; i < proc.getNumPrograms(); ++i)
            m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (dotsBtn),
                         [this] (int r) { if (r > 0) proc.setCurrentProgram (r - 1); });
    };

    // ---- knobs + attachments + formatters ----
    attach (mixKnob,      "mix").setFormatter (pctStr);
    attach (predelayKnob, "predelay").setFormatter (msStr);
    attach (decayKnob,    "decay").setFormatter (secStr);
    attach (hiFreqKnob,   "dampHighFreq").setFormatter (hzStr);
    attach (hiShelfKnob,  "dampHighShelf").setFormatter (dbStr);
    attach (bassFreqKnob, "bassFreq").setFormatter (hzStr);
    attach (bassMultKnob, "bassMult").setFormatter (multStr);
    attach (sizeKnob,     "size").setFormatter (pctStr);
    attach (attackKnob,   "attack").setFormatter (pctStr);
    attach (earlyKnob,    "diffEarly").setFormatter (pctStr);
    attach (lateKnob,     "diffLate").setFormatter (pctStr);
    attach (rateKnob,     "modRate").setFormatter (rateStr);
    attach (depthKnob,    "modDepth").setFormatter (pctStr);
    attach (hiCutKnob,    "highCut").setFormatter (hzStr);
    attach (loCutKnob,    "lowCut").setFormatter (hzStr);

    // ---- host-tempo sync toggles + division selectors ----
    auto setupSync = [this] (Bouncy<juce::TextButton>& btn, juce::ComboBox& box,
                             const juce::StringArray& divs,
                             const char* syncId, const char* divId,
                             std::unique_ptr<ButtonAtt>& btnAtt,
                             std::unique_ptr<ComboAtt>& boxAtt)
    {
        btn.setClickingTogglesState (true);
        btn.setColour (juce::TextButton::textColourOffId, theme::inkSoft);
        btn.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        addAndMakeVisible (btn);
        btnAtt = std::make_unique<ButtonAtt> (proc.apvts, syncId, btn);

        box.addItemList (divs, 1);
        box.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (box);
        boxAtt = std::make_unique<ComboAtt> (proc.apvts, divId, box);
    };

    setupSync (modSyncBtn, modDivBox,
               { "1/1", "1/2", "1/4", "1/4.", "1/4T", "1/8", "1/8.", "1/8T", "1/16", "1/16T" },
               "modSync", "modDiv", modSyncAtt, modDivAtt);
    setupSync (preSyncBtn, preDivBox,
               { "1/64", "1/32", "1/16", "1/16.", "1/8T", "1/8", "1/4" },
               "preSync", "preDiv", preSyncAtt, preDivAtt);

    // ---- bottom bar ----
    auto setupLabel = [this] (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (theme::font (15.0f, false));
        l.setColour (juce::Label::textColourId, theme::inkSoft);
        l.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (l);
    };
    setupLabel (modeLabel,   "mode:");
    setupLabel (colorLabel,  "color:");
    setupLabel (presetLabel, "presets:");

    modeBox.addItemList ({ "concert hall", "plate", "room", "chamber", "ambience" }, 1);
    colorBox.addItemList ({ "1970s", "modern", "vintage", "dark", "bright" }, 1);
    for (auto* b : { &modeBox, &colorBox, &presetBox })
    {
        b->setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (*b);
    }
    modeAtt  = std::make_unique<ComboAtt> (proc.apvts, "mode",  modeBox);
    colorAtt = std::make_unique<ComboAtt> (proc.apvts, "color", colorBox);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0) proc.setCurrentProgram (id - 1);
    };

    prevBtn.setClickingTogglesState (false);
    nextBtn.setClickingTogglesState (false);
    prevBtn.onClick = [this] { stepProgram (-1); };
    nextBtn.onClick = [this] { stepProgram ( 1); };
    for (auto* b : { &prevBtn, &nextBtn })
    {
        b->setColour (juce::TextButton::textColourOffId, theme::accent);
        addAndMakeVisible (*b);
    }

    startTimerHz (24);
    setSize (1024, 640);

    // License overlay sits on top of everything; it shows itself until activated.
    addChildComponent (licenseOverlay);
    licenseOverlay.setBounds (getLocalBounds());
}

VocalVerbEditor::~VocalVerbEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
Knob& VocalVerbEditor::attach (Knob& k, const char* id)
{
    addAndMakeVisible (k);
    if ((size_t) attIndex < knobAtts.size())
        knobAtts[(size_t) attIndex++] =
            std::make_unique<SliderAtt> (proc.apvts, id, k.getSlider());
    k.refresh();
    return k;
}

void VocalVerbEditor::stepProgram (int delta)
{
    const int n = proc.getNumPrograms();
    if (n <= 0) return;
    const int idx = (proc.getCurrentProgram() + delta + n) % n;
    proc.setCurrentProgram (idx);
}

//==============================================================================
void VocalVerbEditor::timerCallback()
{
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);

    // When synced, show the tempo division in place of the Hz / ms readout.
    const bool modOn = proc.apvts.getRawParameterValue ("modSync")->load() > 0.5f;
    rateKnob.setOverrideText (modOn ? modDivBox.getText() : juce::String());
    modDivBox.setEnabled (modOn);

    const bool preOn = proc.apvts.getRawParameterValue ("preSync")->load() > 0.5f;
    predelayKnob.setOverrideText (preOn ? preDivBox.getText() : juce::String());
    preDivBox.setEnabled (preOn);
}

//==============================================================================
void VocalVerbEditor::paint (juce::Graphics& g)
{
    // Warm off-white gradient backdrop + soft top light.
    theme::backdrop (g, getLocalBounds());

    // Floating card: layered elevation shadow, near-white fill, top highlight
    // and a crisp hairline border.
    auto card = [&g] (juce::Rectangle<int> r, float radius, const juce::String& title)
    {
        if (r.isEmpty()) return;
        auto rf = r.toFloat();
        theme::elevate (g, rf, radius);
        g.setColour (theme::card);
        g.fillRoundedRectangle (rf, radius);
        theme::topHighlight (g, rf, radius);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (rf, radius, 1.0f);

        if (title.isNotEmpty())
            theme::spacedText (g, title, r.withHeight (40).reduced (8, 6).toFloat(),
                               theme::ink, 11.5f, 2.2f, true, juce::Justification::centred);
    };

    // ---- wordmark: two-tone "vocal" + "verb" with an accent underline tick ----
    {
        auto wm = theme::font (34.0f, true);
        g.setFont (wm);
        const float vw = juce::GlyphArrangement::getStringWidth (wm, "vocal");
        auto wb = brand.getBounds();
        g.setColour (theme::ink);
        g.drawText ("vocal", wb, juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.drawText ("verb", wb.withTrimmedLeft ((int) vw), juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.fillRoundedRectangle ((float) wb.getX(), (float) wb.getBottom() - 5.0f, 24.0f, 2.5f, 1.25f);
    }
    theme::spacedText (g, "VOCAL REVERB", brandSub.getBounds().toFloat(),
                       theme::inkSoft, 9.5f, 2.6f, false, juce::Justification::centredLeft);

    card (leftCard,  16.0f, {});
    card (dampCard,  16.0f, "damping");
    card (shapeCard, 16.0f, "shape");
    card (diffCard,  16.0f, "diff");
    card (modCard,   16.0f, "mod");
    card (eqCard,    16.0f, "eq");

    // hairline divider in the left mix/predelay card
    if (! leftCard.isEmpty())
    {
        g.setColour (theme::cardLine);
        g.drawHorizontalLine (leftDividerY, (float) leftCard.getX() + 12.0f,
                              (float) leftCard.getRight() - 12.0f);
    }

    // bottom bar card
    card (bottomBar, 18.0f, {});

    // arrow box inside the bottom bar: clean white pill, no shadow
    {
        auto ab = arrowBox.toFloat();
        juce::ColourGradient wg (juce::Colours::white, ab.getX(), ab.getY(),
                                 juce::Colour (0xfff4f5f8), ab.getX(), ab.getBottom(), false);
        g.setGradientFill (wg);
        g.fillRoundedRectangle (ab, 12.0f);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (ab, 12.0f, 1.2f);
        g.drawVerticalLine (arrowBox.getCentreX(), (float) arrowBox.getY() + 6.0f,
                            (float) arrowBox.getBottom() - 6.0f);
    }
}

//==============================================================================
void VocalVerbEditor::resized()
{
    licenseOverlay.setBounds (getLocalBounds());

    auto area = getLocalBounds();

    // ---- top bar ----
    auto top = area.removeFromTop (104);
    auto topIn = top.reduced (28, 0);
    brand.setBounds (topIn.getX() - 2, 26, 240, 50);
    brandSub.setBounds (brand.getRight() - 6, 41, 160, 24);

    const int bh = 34, by = 42;
    dotsBtn.setBounds (topIn.getRight() - 24, by, 24, bh);
    linkBtn.setBounds (dotsBtn.getX() - 60, by, 50, bh);
    bypassBtn.setBounds (linkBtn.getX() - 96, by, 86, bh);

    // ---- bottom bar ----
    area.removeFromBottom (14);
    auto bottom = area.removeFromBottom (82);
    bottomBar = bottom.reduced (18, 6);

    arrowBox = juce::Rectangle<int> (bottomBar.getRight() - 132, bottomBar.getCentreY() - 22,
                                     112, 44);
    prevBtn.setBounds (arrowBox.getX() + 5, arrowBox.getY() + 4, arrowBox.getWidth() / 2 - 7,
                       arrowBox.getHeight() - 8);
    nextBtn.setBounds (arrowBox.getCentreX() + 2, arrowBox.getY() + 4, arrowBox.getWidth() / 2 - 7,
                       arrowBox.getHeight() - 8);

    // mode / color / presets groups across the remaining width
    auto content = bottomBar.withTrimmedRight (150).reduced (24, 0);
    const int segW = content.getWidth() / 3;
    auto placeGroup = [] (juce::Label& lbl, juce::ComboBox& box, juce::Rectangle<int> seg)
    {
        seg = seg.withSizeKeepingCentre (seg.getWidth(), 32);
        lbl.setBounds (seg.removeFromLeft (78));
        seg.removeFromLeft (6);
        box.setBounds (seg.removeFromLeft (juce::jmin (160, seg.getWidth())));
    };
    placeGroup (modeLabel,   modeBox,   content.removeFromLeft (segW));
    placeGroup (colorLabel,  colorBox,  content.removeFromLeft (segW));
    placeGroup (presetLabel, presetBox, content);

    // ---- body cards ----
    area.removeFromTop (4);
    auto body = area.reduced (18, 8);

    const int gap = 13;
    int x = body.getX();
    auto col = [&] (int w) -> juce::Rectangle<int>
    {
        juce::Rectangle<int> r (x, body.getY(), w, body.getHeight());
        x += w + gap;
        return r;
    };

    leftCard          = col (104);
    auto decayArea    = col (210);
    dampCard          = col (212);
    shapeCard         = col (102);
    diffCard          = col (92);
    modCard           = col (92);
    eqCard            = col (98);

    // left card: mix on top, predelay on bottom (with sync row), divider between
    {
        auto in = leftCard.reduced (8, 14);
        auto topHalf = in.removeFromTop (in.getHeight() / 2);
        leftDividerY = topHalf.getBottom() + 4;
        in.removeFromTop (8);
        mixKnob.setBounds (topHalf);

        // predelay knob + a compact sync row beneath it
        const int syncH = 56;
        auto preArea = in;
        predelayKnob.setBounds (preArea.removeFromTop (juce::jmax (40, preArea.getHeight() - syncH)));
        auto sc = preArea.reduced (2, 2);
        preSyncBtn.setBounds (sc.removeFromTop (24));
        sc.removeFromTop (4);
        preDivBox.setBounds (sc.removeFromTop (24));
    }

    // decay: big knob centred in its column
    decayKnob.setBounds (decayArea.reduced (2, 6));

    // damping: 2x2 grid under the header
    {
        auto in = dampCard.reduced (10);
        in.removeFromTop (34);
        auto rowTop = in.removeFromTop (in.getHeight() / 2);
        auto rowBot = in;
        const int cw = rowTop.getWidth() / 2;
        hiFreqKnob.setBounds  (rowTop.removeFromLeft (cw));
        hiShelfKnob.setBounds (rowTop);
        bassFreqKnob.setBounds (rowBot.removeFromLeft (cw));
        bassMultKnob.setBounds (rowBot);
    }

    auto stackTwo = [] (juce::Rectangle<int> card, Knob& a, Knob& b)
    {
        auto in = card.reduced (8);
        in.removeFromTop (32);
        a.setBounds (in.removeFromTop (in.getHeight() / 2));
        b.setBounds (in);
    };
    stackTwo (shapeCard, sizeKnob,  attackKnob);
    stackTwo (diffCard,  earlyKnob, lateKnob);
    stackTwo (eqCard,    hiCutKnob, loCutKnob);

    // mod card: rate knob, a compact sync row, then depth knob
    {
        auto in = modCard.reduced (8);
        in.removeFromTop (32);
        const int syncH = 56;
        auto rateArea = in.removeFromTop ((in.getHeight() - syncH) / 2);
        auto syncCol  = in.removeFromTop (syncH);
        rateKnob.setBounds (rateArea);
        depthKnob.setBounds (in);

        auto sc = syncCol.reduced (4, 2);
        modSyncBtn.setBounds (sc.removeFromTop (24));
        sc.removeFromTop (4);
        modDivBox.setBounds (sc.removeFromTop (24));
    }
}
