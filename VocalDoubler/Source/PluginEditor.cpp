#include "PluginEditor.h"

namespace
{
    const juce::StringArray kModDivisionNames { "1/1", "1/2", "1/4", "1/4.", "1/8", "1/8.", "1/8T", "1/16" };
}

//==============================================================================
VocalDoublerEditor::VocalDoublerEditor (VocalDoublerProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);

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

    startTimerHz (30);
    setSize (1024, 640);

    licenseGate = std::make_unique<licensing::LicenseGate> (*this, "VocalDoubler", "VOCALDOUBLER");
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
}

//==============================================================================
void VocalDoublerEditor::paint (juce::Graphics& g)
{
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
