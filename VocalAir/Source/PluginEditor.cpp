#include "PluginEditor.h"

//==============================================================================
VocalAirEditor::VocalAirEditor (VocalAirProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);

    // ---- branding ----
    // The wordmark, sub-tagline and display label are painted directly (two-tone
    // wordmark + underline, letter-spaced small caps) in paint(); the Label
    // components are kept purely as layout anchors.
    addAndMakeVisible (brand);
    addAndMakeVisible (brandSub);
    addAndMakeVisible (displayLabel);

    // ---- top-bar icon buttons ----
    undoBtn.setDrawer ([] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
                       { icons::undo (g, r, c); });
    undoBtn.onClick = [this] { proc.undoManager.undo(); };
    addAndMakeVisible (undoBtn);

    redoBtn.setDrawer ([] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
                       { icons::redo (g, r, c); });
    redoBtn.onClick = [this] { proc.undoManager.redo(); };
    addAndMakeVisible (redoBtn);

    saveBtn.setDrawer ([] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
                       { icons::save (g, r, c); });
    saveBtn.onClick = [this]
    {
        // Stamp the current settings into the active A/B slot.
        abState[abSlot] = proc.apvts.copyState();
    };
    addAndMakeVisible (saveBtn);

    menuBtn.setDrawer ([] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
                       { icons::hamburger (g, r, c); });
    menuBtn.onClick = [this]
    {
        juce::PopupMenu m;
        for (int i = 0; i < proc.getNumPrograms(); ++i)
            m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (menuBtn),
                         [this] (int r) { if (r > 0) proc.setCurrentProgram (r - 1); });
    };
    addAndMakeVisible (menuBtn);

    // ---- A/B compare ----
    abState[0] = proc.apvts.copyState();
    abState[1] = proc.apvts.copyState();

    for (auto* b : { &abA, &abB })
    {
        b->setClickingTogglesState (false);
        b->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (*b);
    }
    abA.setToggleState (true, juce::dontSendNotification);
    abA.onClick = [this] { selectABSlot (0); };
    abB.onClick = [this] { selectABSlot (1); };

    abArrow.setDrawer ([] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
                       { icons::chevron (g, r, c); });
    abArrow.onClick = [this]
    {
        // copy the active slot's live settings into the other slot
        abState[1 - abSlot] = proc.apvts.copyState();
    };
    addAndMakeVisible (abArrow);

    // ---- preset combo ----
    refreshPresetBox();
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != proc.getCurrentProgram())
            proc.setCurrentProgram (idx);
    };
    addAndMakeVisible (presetBox);

    // ---- meter ----
    addAndMakeVisible (meter);

    // ---- big knobs ----
    midKnob.setCaption ("mid air");
    highKnob.setCaption ("high air");
    addAndMakeVisible (midKnob);
    addAndMakeVisible (highKnob);

    midAtt  = std::make_unique<SliderAtt> (proc.apvts, "midAir",  midKnob);
    highAtt = std::make_unique<SliderAtt> (proc.apvts, "highAir", highKnob);

    midKnob.onValueChange  = [this] { mirrorLink (true);  };
    highKnob.onValueChange = [this] { mirrorLink (false); };

    auto setupRange = [this] (juce::Label& l, const juce::String& txt)
    {
        l.setText (txt, juce::dontSendNotification);
        l.setFont (theme::font (13.0f, false));
        l.setColour (juce::Label::textColourId, theme::inkSoft);
        addAndMakeVisible (l);
    };
    setupRange (midLo,  "0");   midLo.setJustificationType (juce::Justification::centredLeft);
    setupRange (midHi,  "100"); midHi.setJustificationType (juce::Justification::centredRight);
    setupRange (highLo, "0");   highLo.setJustificationType (juce::Justification::centredLeft);
    setupRange (highHi, "100"); highHi.setJustificationType (juce::Justification::centredRight);

    // ---- link pill ----
    linkBtn.setClickingTogglesState (true);
    linkBtn.setDrawer ([this] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour)
    {
        const bool on = linkBtn.getToggleState();
        auto rr = r.reduced (1.0f);
        const float radius = rr.getHeight() * 0.5f;
        if (on)
        {
            g.setColour (theme::accentSoft);
            g.fillRoundedRectangle (rr.expanded (3.0f), radius + 3.0f);
            g.setColour (theme::accent);
            g.fillRoundedRectangle (rr, radius);
        }
        else
        {
            g.setColour (juce::Colours::white);
            g.fillRoundedRectangle (rr, radius);
            g.setColour (theme::cardLine);
            g.drawRoundedRectangle (rr, radius, 1.2f);
        }

        const juce::Colour fg = on ? juce::Colours::white : theme::ink;
        const float ih = rr.getHeight() * 0.5f;
        auto iconR = juce::Rectangle<float> (rr.getX() + 14.0f, rr.getCentreY() - ih * 0.5f, ih * 1.4f, ih);
        icons::chain (g, iconR, fg, 1.5f);

        g.setColour (fg);
        g.setFont (theme::font (rr.getHeight() * 0.42f, false));
        g.drawText ("link",
                    juce::Rectangle<float> (iconR.getRight() + 4.0f, rr.getY(),
                                            rr.getRight() - iconR.getRight() - 14.0f, rr.getHeight()),
                    juce::Justification::centredLeft);
    });
    linkAtt = std::make_unique<ButtonAtt> (proc.apvts, "link", linkBtn);
    addAndMakeVisible (linkBtn);

    // ---- power ----
    powerLabel.setText ("power", juce::dontSendNotification);
    powerLabel.setFont (theme::font (13.0f, false));
    powerLabel.setColour (juce::Label::textColourId, theme::inkSoft);
    addAndMakeVisible (powerLabel);

    powerBtn.setClickingTogglesState (true);
    powerBtn.setDrawer ([this] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour)
    {
        const bool on = powerBtn.getToggleState();
        auto d = r.reduced (4.0f);
        const float radius = juce::jmin (d.getWidth(), d.getHeight()) * 0.5f;
        const auto c = d.getCentre();

        juce::Rectangle<float> disc (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);

        // soft cast shadow kept within the button bounds
        juce::Path sp; sp.addEllipse (disc.translated (0.0f, radius * 0.12f));
        juce::DropShadow (juce::Colours::black.withAlpha (0.16f),
                          (int) juce::jlimit (4.0f, 8.0f, radius * 0.4f), { 0, 2 }).drawForPath (g, sp);

        // white domed face
        AirKnob::paintDome (g, disc);
        if (on)
        {
            g.setColour (theme::accent.withAlpha (0.85f));
            g.drawEllipse (disc.reduced (0.6f), 1.6f);
        }

        icons::power (g, d, on ? theme::accent : theme::inkSoft, 2.0f);
    });
    powerAtt = std::make_unique<ButtonAtt> (proc.apvts, "power", powerBtn);
    addAndMakeVisible (powerBtn);

    // ---- trim ----
    trimLabel.setText ("trim", juce::dontSendNotification);
    trimLabel.setFont (theme::font (13.0f, false));
    trimLabel.setColour (juce::Label::textColourId, theme::inkSoft);
    trimLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (trimLabel);

    addAndMakeVisible (trimKnob);
    trimAtt = std::make_unique<SliderAtt> (proc.apvts, "trim", trimKnob);

    startTimerHz (30);
    setSize (1024, 640);

    licenseGate = std::make_unique<licensing::LicenseGate> (*this, "VocalAir", "VOCALAIR");
}

VocalAirEditor::~VocalAirEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void VocalAirEditor::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

void VocalAirEditor::selectABSlot (int slot)
{
    if (slot == abSlot) return;
    abState[abSlot] = proc.apvts.copyState();   // remember the slot we are leaving
    abSlot = slot;
    proc.apvts.replaceState (abState[abSlot].createCopy());

    abA.setToggleState (abSlot == 0, juce::dontSendNotification);
    abB.setToggleState (abSlot == 1, juce::dontSendNotification);
}

void VocalAirEditor::mirrorLink (bool fromMid)
{
    if (linking) return;
    if (! linkBtn.getToggleState()) return;

    linking = true;
    if (fromMid)
    {
        if (auto* h = proc.apvts.getParameter ("highAir"))
            h->setValueNotifyingHost (h->getNormalisableRange().convertTo0to1 ((float) midKnob.getValue()));
    }
    else
    {
        if (auto* m = proc.apvts.getParameter ("midAir"))
            m->setValueNotifyingHost (m->getNormalisableRange().convertTo0to1 ((float) highKnob.getValue()));
    }
    linking = false;
}

//==============================================================================
void VocalAirEditor::timerCallback()
{
    meter.setLevel (proc.getOutputLevel());

    const int cur = proc.getCurrentProgram();
    if (presetBox.getSelectedId() != cur + 1)
        presetBox.setSelectedId (cur + 1, juce::dontSendNotification);
}

//==============================================================================
void VocalAirEditor::paint (juce::Graphics& g)
{
    // warm off-white gradient backdrop + soft top light
    theme::backdrop (g, getLocalBounds());

    // the wide "air display" floating card
    {
        auto rf = displayCard.toFloat();
        theme::elevate (g, rf, 18.0f);
        g.setColour (theme::card);
        g.fillRoundedRectangle (rf, 18.0f);
        theme::topHighlight (g, rf, 18.0f);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (rf, 18.0f, 1.0f);
    }

    // two-tone wordmark: ink "vocal" + accent "air", with an accent underline.
    {
        auto b = brand.getBounds().toFloat();
        auto wm = theme::font (24.0f, true);
        g.setFont (wm);
        const float vw = juce::GlyphArrangement::getStringWidth (wm, "vocal");
        g.setColour (theme::ink);
        g.drawText ("vocal", b, juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.drawText ("air", b.withTrimmedLeft (vw), juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.fillRoundedRectangle (b.getX(), b.getBottom() - 5.0f, 22.0f, 2.5f, 1.25f);
    }

    // letter-spaced sub-tagline
    theme::spacedText (g, "VOCAL AIR", brandSub.getBounds().toFloat(),
                       theme::inkSoft, 9.5f, 2.4f, false, juce::Justification::left);

    // letter-spaced card label
    theme::spacedText (g, "AIR DISPLAY", displayLabel.getBounds().toFloat(),
                       theme::inkSoft, 10.5f, 2.2f, true, juce::Justification::left);
}

//==============================================================================
void VocalAirEditor::resized()
{
    auto r = getLocalBounds().reduced (24);

    // ---- top bar ----
    auto top = r.removeFromTop (44);
    brand.setBounds (top.getX(), top.getY() + 4, 130, 34);
    brandSub.setBounds (brand.getRight() + 6, top.getY() + 13, 90, 20);

    // right-aligned cluster
    int x = top.getRight();
    const int icon = 30, gap = 10;

    menuBtn.setBounds (x - icon, top.getY() + 6, icon, icon);            x -= icon + gap;
    saveBtn.setBounds (x - icon, top.getY() + 6, icon, icon);            x -= icon + gap + 6;

    const int comboW = 170, comboH = 34;
    presetBox.setBounds (x - comboW, top.getY() + 4, comboW, comboH);    x -= comboW + gap + 8;

    // A > B cluster
    const int abW = 26;
    abB.setBounds (x - abW, top.getY() + 6, abW, icon);                  x -= abW + 2;
    abArrow.setBounds (x - 22, top.getY() + 6, 22, icon);               x -= 22 + 2;
    abA.setBounds (x - abW, top.getY() + 6, abW, icon);                 x -= abW + gap + 8;

    redoBtn.setBounds (x - icon, top.getY() + 6, icon, icon);            x -= icon + gap;
    undoBtn.setBounds (x - icon, top.getY() + 6, icon, icon);

    r.removeFromTop (16);

    // ---- display card with arc meter ----
    displayCard = r.removeFromTop (230);
    displayLabel.setBounds (displayCard.getX() + 22, displayCard.getY() + 12, 120, 22);
    meter.setBounds (displayCard.reduced (24).withTrimmedTop (10));

    r.removeFromTop (24);

    // ---- knob row ----
    auto knobRow = r.removeFromTop (240);
    const int knobSize = 230;
    const int centreGap = 150;

    auto leftCol  = knobRow.removeFromLeft (knobRow.getWidth() / 2);
    auto rightCol = knobRow;

    midKnob.setBounds (leftCol.getRight() - centreGap / 2 - knobSize,
                       leftCol.getY(), knobSize, knobSize);
    highKnob.setBounds (rightCol.getX() + centreGap / 2,
                        rightCol.getY(), knobSize, knobSize);

    // range labels beneath each knob
    const int lblY = midKnob.getBottom() - 18;
    midLo.setBounds (midKnob.getX() - 6, lblY, 40, 20);
    midHi.setBounds (midKnob.getRight() - 34, lblY, 40, 20);
    highLo.setBounds (highKnob.getX() - 6, lblY, 40, 20);
    highHi.setBounds (highKnob.getRight() - 34, lblY, 40, 20);

    // link pill centred between the knobs
    const int linkW = 86, linkH = 36;
    linkBtn.setBounds ((midKnob.getRight() + highKnob.getX()) / 2 - linkW / 2,
                       midKnob.getBounds().getCentreY() - linkH / 2, linkW, linkH);

    // ---- bottom row: power (left) + trim (right) ----
    auto bottom = getLocalBounds().reduced (24).removeFromBottom (70);
    powerLabel.setBounds (bottom.getX(), bottom.getY(), 70, 20);
    powerBtn.setBounds (bottom.getX(), bottom.getY() + 22, 44, 44);

    trimLabel.setBounds (bottom.getRight() - 70, bottom.getY(), 70, 20);
    trimKnob.setBounds (bottom.getRight() - 60, bottom.getY() + 16, 54, 54);
}
