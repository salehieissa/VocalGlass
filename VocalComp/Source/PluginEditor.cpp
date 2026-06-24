#include "PluginEditor.h"

namespace
{
    const std::array<juce::String, 3> kModeWords { "ARC", "Opto", "Warm" };
}

//==============================================================================
VocalCompEditor::VocalCompEditor (VocalCompProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);

    auto addLabel = [this] (juce::Label& l, const juce::String& text, float size, bool bold,
                            juce::Colour col, juce::Justification just)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (theme::font (size, bold));
        l.setColour (juce::Label::textColourId, col);
        l.setJustificationType (just);
        addAndMakeVisible (l);
    };

    // ---- branding (the wordmark + subtitle are painted two-tone in paint();
    //      these labels just reserve their layout slots) ----
    addLabel (brand,    "",  23.0f, true,  theme::ink,     juce::Justification::centredLeft);
    addLabel (brandSub, "",  11.0f, false, theme::inkSoft, juce::Justification::centredLeft);

    // ---- preset combo ----
    refreshPresetBox();
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != proc.getCurrentProgram())
            proc.setCurrentProgram (idx);
    };
    addAndMakeVisible (presetBox);

    // ---- left column (threshold + input) ----
    addLabel (threshCap, "THRESH", 11.0f, false, theme::inkSoft, juce::Justification::centred);
    addLabel (threshVal, "-28.0",  21.0f, true,  theme::ink,     juce::Justification::centred);
    addLabel (inputCap,  "IN",     10.0f, false, theme::inkSoft, juce::Justification::centred);
    addLabel (inLVal,    "-1.4",   13.0f, false, theme::ink,     juce::Justification::centred);
    addLabel (inRVal,    "-1.4",   13.0f, false, theme::ink,     juce::Justification::centred);

    threshSlider.setSliderStyle (juce::Slider::LinearVertical);
    threshSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (threshSlider);
    threshAtt = std::make_unique<SliderAtt> (proc.apvts, "threshold", threshSlider);

    addAndMakeVisible (inMeterL);
    addAndMakeVisible (inMeterR);

    // ---- centre (ratio) ----
    addLabel (ratioCap, "RATIO", 11.0f, false, theme::inkSoft, juce::Justification::centred);
    addAndMakeVisible (curve);
    ratioAtt = std::make_unique<SliderAtt> (proc.apvts, "ratio", curve);

    // ---- right column (makeup + output) ----
    addLabel (gainCap,   "GAIN",   11.0f, false, theme::inkSoft, juce::Justification::centred);
    addLabel (gainVal,   "0.0",    21.0f, true,  theme::ink,     juce::Justification::centred);
    addLabel (outputCap, "OUT",    10.0f, false, theme::inkSoft, juce::Justification::centred);
    addLabel (outLVal,   "-0.0",   13.0f, false, theme::ink,     juce::Justification::centred);
    addLabel (outRVal,   "-6.1",   13.0f, false, theme::ink,     juce::Justification::centred);

    gainSlider.setSliderStyle (juce::Slider::LinearVertical);
    gainSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (gainSlider);
    gainAtt = std::make_unique<SliderAtt> (proc.apvts, "makeup", gainSlider);

    addAndMakeVisible (outMeterL);
    addAndMakeVisible (outMeterR);

    // ---- attack / release ----
    addLabel (attackCap,  "Attack",  12.0f, false, theme::inkSoft, juce::Justification::centredLeft);
    addLabel (releaseCap, "Release", 12.0f, false, theme::inkSoft, juce::Justification::centredLeft);
    addLabel (attackVal,  "20.0",    13.0f, true,  theme::ink,     juce::Justification::centredRight);
    addLabel (releaseVal, "200",     13.0f, true,  theme::ink,     juce::Justification::centredRight);

    attackSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    attackSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (attackSlider);
    attackAtt = std::make_unique<SliderAtt> (proc.apvts, "attack", attackSlider);

    releaseSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    releaseSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (releaseSlider);
    releaseAtt = std::make_unique<SliderAtt> (proc.apvts, "release", releaseSlider);

    // ---- mode pills ----
    addLabel (modeCap, "MODE", 11.0f, false, theme::inkSoft, juce::Justification::centredLeft);
    for (int i = 0; i < 3; ++i)
    {
        auto& b = modeBtns[(size_t) i];
        b.setButtonText (kModeWords[(size_t) i]);
        b.setClickingTogglesState (false);
        b.onClick = [this, i] { setMode (i); };
        addAndMakeVisible (b);
    }

    // ---- gate / mix / trim ----
    addLabel (gateCap, "Gate", 12.0f, false, theme::inkSoft, juce::Justification::centred);
    addLabel (gateVal, "OFF",  10.0f, false, theme::inkSoft, juce::Justification::centred);
    addLabel (mixCap,  "Mix",  12.0f, false, theme::inkSoft, juce::Justification::centred);
    addLabel (trimCap, "Trim", 12.0f, false, theme::inkSoft, juce::Justification::centred);
    addLabel (mixMin,  "0",    10.0f, false, theme::inkSoft, juce::Justification::centredLeft);
    addLabel (mixMax,  "100",  10.0f, false, theme::inkSoft, juce::Justification::centredRight);
    addLabel (trimMin, "-18",  10.0f, false, theme::inkSoft, juce::Justification::centredLeft);
    addLabel (trimMax, "+18",  10.0f, false, theme::inkSoft, juce::Justification::centredRight);

    gateKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gateKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    gateKnob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                  juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (gateKnob);
    gateAtt = std::make_unique<SliderAtt> (proc.apvts, "gate", gateKnob);

    mixKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    mixKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    mixKnob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                 juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (mixKnob);
    mixAtt = std::make_unique<SliderAtt> (proc.apvts, "mix", mixKnob);

    trimKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    trimKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    trimKnob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                  juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (trimKnob);
    trimAtt = std::make_unique<SliderAtt> (proc.apvts, "trim", trimKnob);

    startTimerHz (30);
    setSize (820, 500);

    licenseGate = std::make_unique<licensing::LicenseGate> (*this, "VocalComp", "VOCALCOMP");
}

VocalCompEditor::~VocalCompEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void VocalCompEditor::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

void VocalCompEditor::setMode (int mode)
{
    if (auto* m = proc.apvts.getParameter ("mode"))
        m->setValueNotifyingHost (m->getNormalisableRange().convertTo0to1 ((float) mode));
}

//==============================================================================
void VocalCompEditor::timerCallback()
{
    auto fmt1 = [] (float v) { return juce::String (v, 1); };
    // Show a clean "-∞" instead of a placeholder-looking "-100.0" when silent.
    auto fmtDb = [] (float v) { return v <= -60.0f ? juce::String (juce::CharPointer_UTF8 ("-\u221e"))
                                                   : juce::String (v, 1); };

    const float threshDb = proc.apvts.getRawParameterValue ("threshold")->load();
    threshVal.setText (fmt1 (threshDb), juce::dontSendNotification);
    gainVal.setText   (fmt1 (proc.apvts.getRawParameterValue ("makeup")->load()),
                       juce::dontSendNotification);
    attackVal.setText (fmt1 (proc.apvts.getRawParameterValue ("attack")->load()),
                       juce::dontSendNotification);
    releaseVal.setText (juce::String (juce::roundToInt (proc.apvts.getRawParameterValue ("release")->load())),
                        juce::dontSendNotification);

    const float gateDb = proc.apvts.getRawParameterValue ("gate")->load();
    gateVal.setText (gateDb <= -79.5f ? juce::String ("OFF") : juce::String (gateDb, 1),
                     juce::dontSendNotification);

    const float inL = proc.engine.meterInL.load(),  inR = proc.engine.meterInR.load();
    const float outL = proc.engine.meterOutL.load(), outR = proc.engine.meterOutR.load();
    inLVal.setText (fmtDb (inL),  juce::dontSendNotification);
    inRVal.setText (fmtDb (inR),  juce::dontSendNotification);
    outLVal.setText (fmtDb (outL), juce::dontSendNotification);
    outRVal.setText (fmtDb (outR), juce::dontSendNotification);

    inMeterL.setLevelDb (inL);
    inMeterR.setLevelDb (inR);
    outMeterL.setLevelDb (outL);
    outMeterR.setLevelDb (outR);

    curve.setGainReductionDb (proc.engine.meterGR.load());
    curve.setThresholdDb (threshDb);
    curve.setInputDb (juce::jmax (inL, inR));

    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    curve.setMode (mode);
    for (int i = 0; i < 3; ++i)
        modeBtns[(size_t) i].setToggleState (i == mode, juce::dontSendNotification);

    const int cur = proc.getCurrentProgram();
    if (presetBox.getSelectedId() != cur + 1)
        presetBox.setSelectedId (cur + 1, juce::dontSendNotification);
}

//==============================================================================
void VocalCompEditor::paint (juce::Graphics& g)
{
    // warm off-white gradient backdrop + soft top light
    theme::backdrop (g, getLocalBounds());

    // floating-card helper: ambient elevation + fill + top highlight + hairline
    auto card = [&] (juce::Rectangle<int> r, float radius)
    {
        auto rf = r.toFloat();
        theme::elevate (g, rf, radius);
        g.setColour (theme::card);
        g.fillRoundedRectangle (rf, radius);
        theme::topHighlight (g, rf, radius);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (rf, radius, 1.0f);
    };

    // ---- two-tone wordmark + accent underline ----
    {
        auto wm = theme::font (23.0f, true);
        g.setFont (wm);
        auto br = brand.getBounds().toFloat();
        const float vw = juce::GlyphArrangement::getStringWidth (wm, "vocal");
        g.setColour (theme::ink);
        g.drawText ("vocal", br, juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.drawText ("comp", br.withTrimmedLeft (vw), juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.fillRoundedRectangle (br.getX(), br.getBottom() - 3.0f, 20.0f, 2.5f, 1.25f);
    }
    theme::spacedText (g, "VOCAL COMPRESSOR", brandSub.getBounds().toFloat(),
                       theme::inkSoft, 9.5f, 2.4f, false, juce::Justification::centredLeft);

    // main floating card holding all controls
    card (mainCard, 18.0f);

    // mood: a faint accent bloom behind the centre transfer-curve card
    theme::accentBloom (g, curveCard.toFloat().getCentre(),
                        (float) curveCard.getWidth() * 0.55f, 0.05f);

    // centre ratio sub-card (the CurveDisplay carves its own recessed face inside)
    card (curveCard, 14.0f);

    // hairline divider above the bottom strip
    g.setColour (theme::cardLine);
    g.drawLine ((float) mainCard.getX() + 20.0f, (float) bottomStrip.getY() - 7.0f,
                (float) mainCard.getRight() - 20.0f, (float) bottomStrip.getY() - 7.0f, 1.0f);

    // mode pill container: a clean recessed track the pills sit in
    {
        auto mc = modeContainer.toFloat();
        const float r = mc.getHeight() * 0.5f;
        theme::recess (g, mc, r);
    }
}

//==============================================================================
void VocalCompEditor::resized()
{
    auto bounds = getLocalBounds();
    auto outer = bounds.reduced (20, 16);

    // ---- top bar (on the bare background, above the card) ----
    auto top = outer.removeFromTop (36);
    brand.setBounds (top.getX(), top.getY() + 3, 150, 30);
    brandSub.setBounds (top.getX() + 138, top.getY() + 11, 170, 18);

    const int comboW = 184, comboH = 30;
    presetBox.setBounds (top.getRight() - comboW, top.getY() + 3, comboW, comboH);

    outer.removeFromTop (10);

    // ---- main card ----
    mainCard = outer;
    auto inner = mainCard.reduced (20, 16);

    // bottom strip reserved first so the columns size to the remaining space
    const int stripH = 92;
    bottomStrip = inner.removeFromBottom (stripH);
    inner.removeFromBottom (14);   // gap for the divider
    auto strip = bottomStrip;

    // ---- three columns (threshold | ratio | gain) ----
    const int colW = 92;
    auto leftCol  = inner.removeFromLeft (colW);
    auto rightCol = inner.removeFromRight (colW);
    inner.removeFromLeft (16);
    inner.removeFromRight (16);
    auto centre = inner;

    auto layoutSideColumn = [] (juce::Rectangle<int> col,
                                juce::Label& cap, juce::Label& val,
                                juce::Slider& slider, MeterBar& mL, MeterBar& mR,
                                juce::Label& readCap, juce::Label& readL, juce::Label& readR)
    {
        cap.setBounds (col.removeFromTop (14));
        val.setBounds (col.removeFromTop (24));

        // compact readout block pinned to the bottom
        auto read = col.removeFromBottom (30);
        readCap.setBounds (read.removeFromTop (13));
        readL.setBounds (read.removeFromLeft (read.getWidth() / 2));
        readR.setBounds (read);

        col.removeFromTop (6);
        col.removeFromBottom (4);

        // vertical slider centred, meter bars hugging it on both sides
        const int cx = col.getCentreX();
        const int barW = 5, barGap = 8, sliderW = 26;
        slider.setBounds (cx - sliderW / 2, col.getY(), sliderW, col.getHeight());
        mL.setBounds (cx - sliderW / 2 - barGap - barW, col.getY(), barW, col.getHeight());
        mR.setBounds (cx + sliderW / 2 + barGap,        col.getY(), barW, col.getHeight());
    };

    layoutSideColumn (leftCol, threshCap, threshVal, threshSlider, inMeterL, inMeterR,
                      inputCap, inLVal, inRVal);
    layoutSideColumn (rightCol, gainCap, gainVal, gainSlider, outMeterL, outMeterR,
                      outputCap, outLVal, outRVal);

    // ---- centre ratio card (transfer-curve hero) ----
    ratioCap.setBounds (centre.removeFromTop (14));
    centre.removeFromTop (3);
    curveCard = centre;
    curve.setBounds (curveCard.reduced (8));

    // ---- bottom strip: mode pills | attack/release | gate/mix/trim ----
    auto modeArea = strip.removeFromLeft (216);
    auto knobArea = strip.removeFromRight (264);
    auto arArea   = strip.reduced (20, 0);

    // mode pills (left), vertically centred
    {
        const int capH = 13, contH = 38;
        const int blockY = strip.getY() + (stripH - (capH + 5 + contH)) / 2;
        modeCap.setBounds (modeArea.getX() + 2, blockY, 80, capH);
        modeContainer = juce::Rectangle<int> (modeArea.getX(), blockY + capH + 5,
                                              modeArea.getWidth() - 8, contH);
        auto pills = modeContainer.reduced (6, 6);
        const int pillGap = 6;
        const int pillW = (pills.getWidth() - pillGap * 2) / 3;
        for (int i = 0; i < 3; ++i)
            modeBtns[(size_t) i].setBounds (pills.getX() + i * (pillW + pillGap), pills.getY(),
                                            pillW, pills.getHeight());
    }

    // attack / release (centre), two centred rows
    {
        const int rowH = 26, rowGap = 12;
        const int labelW = 58, valW = 46;
        const int blockY = strip.getY() + (stripH - (rowH * 2 + rowGap)) / 2;
        auto row1 = juce::Rectangle<int> (arArea.getX(), blockY, arArea.getWidth(), rowH);
        auto row2 = juce::Rectangle<int> (arArea.getX(), blockY + rowH + rowGap, arArea.getWidth(), rowH);

        attackCap.setBounds (row1.removeFromLeft (labelW));
        attackVal.setBounds (row1.removeFromRight (valW));
        attackSlider.setBounds (row1.reduced (8, 0));

        releaseCap.setBounds (row2.removeFromLeft (labelW));
        releaseVal.setBounds (row2.removeFromRight (valW));
        releaseSlider.setBounds (row2.reduced (8, 0));
    }

    // gate / mix / trim knobs (right), vertically centred
    {
        constexpr int knobSize = 50;
        const int capH = 14, rangeH = 14;
        const int blockY = strip.getY() + (stripH - (capH + knobSize + rangeH)) / 2;
        const int third = knobArea.getWidth() / 3;
        auto gateCell = knobArea.removeFromLeft (third);
        auto mixCell  = knobArea.removeFromLeft (third);
        auto trimCell = knobArea;

        auto placeKnob = [blockY] (juce::Rectangle<int> cell, juce::Label& cap,
                                   juce::Slider& knob, juce::Label& lo, juce::Label& hi)
        {
            const int cx = cell.getCentreX();
            cap.setBounds (cx - 50, blockY, 100, capH);
            knob.setBounds (cx - knobSize / 2, blockY + capH, knobSize, knobSize);
            lo.setBounds (cx - knobSize / 2 - 6, knob.getBottom() - 1, 26, 13);
            hi.setBounds (cx + knobSize / 2 - 20, knob.getBottom() - 1, 26, 13);
        };

        // Gate shows a single centred live readout (OFF / dB) instead of min/max.
        {
            const int cx = gateCell.getCentreX();
            gateCap.setBounds (cx - 50, blockY, 100, capH);
            gateKnob.setBounds (cx - knobSize / 2, blockY + capH, knobSize, knobSize);
            gateVal.setBounds (cx - 40, gateKnob.getBottom() - 1, 80, 13);
        }

        placeKnob (mixCell,  mixCap,  mixKnob,  mixMin,  mixMax);
        placeKnob (trimCell, trimCap, trimKnob, trimMin, trimMax);
    }
}
