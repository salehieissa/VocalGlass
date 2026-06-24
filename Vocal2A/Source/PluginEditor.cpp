#include "PluginEditor.h"

namespace
{
    const std::array<juce::String, 3> kVuWords     { "input", "GR", "output" };
    const std::array<juce::String, 3> kAnalogWords { "50Hz", "60Hz", "off" };
}

//==============================================================================
Vocal2AEditor::Vocal2AEditor (Vocal2AProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);

    // ---- branding (two-tone wordmark + tagline are painted in paint()) ----
    juce::ignoreUnused (brand, brandSub);

    // ---- VU source selector ----
    vuTitle.setText ("VU display", juce::dontSendNotification);
    vuTitle.setFont (theme::font (15.0f, false));
    vuTitle.setColour (juce::Label::textColourId, theme::ink);
    vuTitle.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (vuTitle);

    for (int i = 0; i < 3; ++i)
    {
        auto& b = vuButtons[(size_t) i];
        b.setButtonText (kVuWords[(size_t) i]);
        b.setClickingTogglesState (false);
        b.setComponentID ("flat");
        b.onClick = [this, i] { setChoice ("vuSource", i); };
        addAndMakeVisible (b);
    }

    // ---- big knobs ----
    addAndMakeVisible (gainKnob);
    addAndMakeVisible (peakKnob);
    gainAtt = std::make_unique<SliderAtt> (proc.apvts, "gain", gainKnob);
    peakAtt = std::make_unique<SliderAtt> (proc.apvts, "peakReduction", peakKnob);

    auto setupCap = [this] (juce::Label& l, const juce::String& t, float size, bool bold)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (theme::font (size, bold));
        l.setColour (juce::Label::textColourId, theme::ink);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };

    setupCap (gainCap, "gain", 17.0f, false);
    setupCap (peakCap, "peak reduction", 17.0f, false);

    // ---- toggles ----
    modeSwitch.setClickingTogglesState (true);
    modeSwitch.onClick = [this] { setChoice ("mode", modeSwitch.getToggleState() ? 1 : 0); };
    addAndMakeVisible (modeSwitch);

    addAndMakeVisible (autoSwitch);
    autoAtt = std::make_unique<ButtonAtt> (proc.apvts, "autoMakeup", autoSwitch);

    setupCap (modeCap, "compress / limit", 13.0f, false);
    setupCap (autoCap, "auto makeup", 13.0f, false);
    modeCap.setColour (juce::Label::textColourId, theme::inkSoft);
    autoCap.setColour (juce::Label::textColourId, theme::inkSoft);

    // ---- VU meter ----
    addAndMakeVisible (vu);

    // ---- bottom strip ----
    setupCap (analogLabel, "analog", 15.0f, false);
    analogLabel.setJustificationType (juce::Justification::centredLeft);

    for (int i = 0; i < 3; ++i)
    {
        auto& b = analogButtons[(size_t) i];
        b.setButtonText (kAnalogWords[(size_t) i]);
        b.setClickingTogglesState (false);
        b.setComponentID ("seg");
        b.onClick = [this, i] { setChoice ("analog", i); };
        addAndMakeVisible (b);
    }

    setupCap (hiFreqLabel, "hi freq", 15.0f, false);
    hiFreqLabel.setJustificationType (juce::Justification::centredLeft);
    setupCap (mixLabel, "mix", 15.0f, false);
    setupCap (trimLabel, "trim", 15.0f, false);

    setupCap (hiFreqCap, "flat", 12.0f, false);
    setupCap (mixCap, "100%", 12.0f, false);
    setupCap (trimCap, "0.0 dB", 12.0f, false);
    hiFreqCap.setColour (juce::Label::textColourId, theme::inkSoft);
    mixCap.setColour (juce::Label::textColourId, theme::inkSoft);
    trimCap.setColour (juce::Label::textColourId, theme::inkSoft);

    for (auto* k : { &hiFreqKnob, &mixKnob, &trimKnob })
    {
        k->setBigValueVisible (false);
        addAndMakeVisible (*k);
    }
    hiFreqAtt = std::make_unique<SliderAtt> (proc.apvts, "hiFreq", hiFreqKnob);
    mixAtt    = std::make_unique<SliderAtt> (proc.apvts, "mix", mixKnob);
    trimAtt   = std::make_unique<SliderAtt> (proc.apvts, "trim", trimKnob);

    startTimerHz (30);
    setSize (1024, 650);

    licenseGate = std::make_unique<licensing::LicenseGate> (*this, "Vocal2A", "VOCAL2A");
}

Vocal2AEditor::~Vocal2AEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void Vocal2AEditor::setChoice (const juce::String& paramID, int index)
{
    if (auto* param = proc.apvts.getParameter (paramID))
        param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 ((float) index));
}

//==============================================================================
void Vocal2AEditor::timerCallback()
{
    const int vuSource = (int) proc.apvts.getRawParameterValue ("vuSource")->load();
    const int analog   = (int) proc.apvts.getRawParameterValue ("analog")->load();
    const int mode     = (int) proc.apvts.getRawParameterValue ("mode")->load();

    for (int i = 0; i < 3; ++i)
        vuButtons[(size_t) i].setToggleState (i == vuSource, juce::dontSendNotification);
    for (int i = 0; i < 3; ++i)
        analogButtons[(size_t) i].setToggleState (i == analog, juce::dontSendNotification);

    modeSwitch.setToggleState (mode == OptoLeveler::Limit, juce::dontSendNotification);

    // small knob captions
    const float hiFreq = proc.apvts.getRawParameterValue ("hiFreq")->load();
    hiFreqCap.setText (hiFreq <= 1.0f ? "flat"
                       : hiFreq >= 99.0f ? "bright"
                       : juce::String (juce::roundToInt (hiFreq)), juce::dontSendNotification);

    const float mix = proc.apvts.getRawParameterValue ("mix")->load();
    mixCap.setText (juce::String (juce::roundToInt (mix)) + "%", juce::dontSendNotification);

    const float trim = proc.apvts.getRawParameterValue ("trim")->load();
    trimCap.setText (juce::String (trim, 1) + " dB", juce::dontSendNotification);

    // drive the VU needle from the selected source (scale value -20..+3)
    float scaleValue = 0.0f;
    if (vuSource == 1) // GR: rest at 0, swings left as reduction grows
        scaleValue = -proc.engine.grReductionDb.load();
    else if (vuSource == 0) // input level, 0 VU ~ -18 dBFS
        scaleValue = proc.engine.inputDb.load() + 18.0f;
    else                    // output level
        scaleValue = proc.engine.outputDb.load() + 18.0f;

    vu.setLevel (scaleValue);

    repaint();
}

//==============================================================================
void Vocal2AEditor::paint (juce::Graphics& g)
{
    // warm off-white gradient backdrop + soft top light
    theme::backdrop (g, getLocalBounds());

    // main card — full-bleed base panel (no cast shadow so nothing clips the
    // window edge); catches the top light and carries a hairline border.
    {
        auto rf = cardArea.toFloat();
        g.setColour (theme::card);
        g.fillRoundedRectangle (rf, 22.0f);
        theme::topHighlight (g, rf, 22.0f);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (rf, 22.0f, 1.0f);
    }

    // bottom strip: slightly recessed panel separated by a hairline divider
    {
        juce::Path clip;
        clip.addRoundedRectangle (cardArea.getX(), bottomStrip.getY(),
                                  cardArea.getWidth(), cardArea.getBottom() - bottomStrip.getY(),
                                  22.0f, 22.0f, false, false, true, true);
        g.setColour (theme::bgLo.withAlpha (0.5f));
        g.fillPath (clip);
    }
    g.setColour (theme::cardLine);
    g.drawLine ((float) cardArea.getX(), (float) bottomStrip.getY(),
                (float) cardArea.getRight(), (float) bottomStrip.getY(), 1.0f);

    // two-tone wordmark ("vocal" ink + "2a" accent) with an accent underline tick
    {
        auto wm = theme::font (30.0f, true);
        g.setFont (wm);
        const float vw = juce::GlyphArrangement::getStringWidth (wm, "vocal");
        g.setColour (theme::ink);
        g.drawText ("vocal", brandBounds, juce::Justification::centredLeft, false);
        g.setColour (theme::accent);
        g.drawText ("2a", brandBounds.withTrimmedLeft ((int) vw),
                    juce::Justification::centredLeft, false);
        g.setColour (theme::accent);
        g.fillRoundedRectangle ((float) brandBounds.getX(),
                                (float) brandBounds.getBottom() - 6.0f, 22.0f, 2.5f, 1.25f);
    }
    theme::spacedText (g, "VOCAL LEVELER", brandSubBounds.toFloat(),
                       theme::inkSoft, 9.5f, 2.6f, false, juce::Justification::centredLeft);

    // VU meter sub-card (floating)
    {
        auto rf = vuCard.toFloat();
        theme::elevate (g, rf, 18.0f);
        g.setColour (theme::card);
        g.fillRoundedRectangle (rf, 18.0f);
        theme::topHighlight (g, rf, 18.0f);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (rf, 18.0f, 1.2f);
    }

    // analog segmented container -> recessed track that hosts the pills
    theme::recess (g, analogPill.toFloat(), 10.0f);

    // divider in the bottom strip between hi-freq and mix groups
    g.setColour (theme::cardLine);
    const int divX = mixKnob.getX() - 42;
    g.drawLine ((float) divX, (float) bottomStrip.getY() + 30.0f,
                (float) divX, (float) bottomStrip.getBottom() - 30.0f, 1.0f);
}

//==============================================================================
void Vocal2AEditor::resized()
{
    auto r = getLocalBounds().reduced (16);
    cardArea = r;

    auto inner = r.reduced (24);

    // bottom strip across the full card width
    const int stripH = 150;
    bottomStrip = juce::Rectangle<int> (cardArea.getX(), cardArea.getBottom() - stripH,
                                        cardArea.getWidth(), stripH);

    auto content = inner.withTrimmedBottom (stripH - 24 + 8);

    // ---- top bar ----
    auto top = content.removeFromTop (56);
    brandBounds    = { top.getX(), top.getY(), 200, 44 };
    brandSubBounds = { top.getX() + 168, top.getY() + 14, 160, 24 };

    auto vuArea = top.removeFromRight (330);
    vuTitle.setBounds (vuArea.getX(), vuArea.getY() - 2, vuArea.getWidth(), 22);
    {
        const int bw = 70, bh = 22;
        int x = vuArea.getRight() - bw * 3 - 16;
        for (int i = 0; i < 3; ++i)
        {
            vuButtons[(size_t) i].setBounds (x, vuArea.getY() + 24, bw, bh);
            x += bw + 8;
        }
    }

    content.removeFromTop (8);

    // ---- three columns ----
    const int colW = 270;
    auto leftCol  = content.removeFromLeft (colW);
    auto rightCol = content.removeFromRight (colW);
    auto centreCol = content;

    // left column: gain knob + caption + mode toggle
    {
        auto col = leftCol.reduced (10, 0);
        auto knobArea = col.removeFromTop (230);
        gainKnob.setBounds (knobArea.withSizeKeepingCentre (220, 230));
        gainCap.setBounds (col.removeFromTop (26));
        col.removeFromTop (16);
        modeSwitch.setBounds (col.getCentreX() - 33, col.getY(), 66, 34);
        modeCap.setBounds (col.getX(), col.getY() + 40, col.getWidth(), 20);
    }

    // right column: peak knob + caption + auto toggle
    {
        auto col = rightCol.reduced (10, 0);
        auto knobArea = col.removeFromTop (230);
        peakKnob.setBounds (knobArea.withSizeKeepingCentre (220, 230));
        peakCap.setBounds (col.removeFromTop (26));
        col.removeFromTop (16);
        autoSwitch.setBounds (col.getCentreX() - 33, col.getY(), 66, 34);
        autoCap.setBounds (col.getX(), col.getY() + 40, col.getWidth(), 20);
    }

    // centre column: VU meter card
    vuCard = centreCol.reduced (12, 6);
    vu.setBounds (vuCard.reduced (16, 14));

    // ---- bottom strip controls ----
    auto strip = bottomStrip.reduced (28, 0);
    strip = strip.withTrimmedTop (8);

    analogLabel.setBounds (strip.getX(), strip.getCentreY() - 14, 70, 28);
    analogPill = juce::Rectangle<int> (strip.getX() + 78, strip.getCentreY() - 18, 210, 38);
    {
        const int seg = analogPill.getWidth() / 3;
        for (int i = 0; i < 3; ++i)
            analogButtons[(size_t) i].setBounds (analogPill.getX() + i * seg + 3,
                                                 analogPill.getY() + 3,
                                                 seg - 6, analogPill.getHeight() - 6);
    }

    // hi freq small knob
    hiFreqLabel.setBounds (analogPill.getRight() + 24, strip.getCentreY() - 14, 70, 28);
    {
        const int kx = hiFreqLabel.getRight() + 6;
        hiFreqKnob.setBounds (kx, strip.getCentreY() - 30, 60, 60);
        hiFreqCap.setBounds (kx - 10, hiFreqKnob.getBottom() - 2, 80, 18);
    }

    // mix + trim small knobs on the right
    {
        const int kSize = 60;
        trimKnob.setBounds (strip.getRight() - kSize, strip.getCentreY() - 30, kSize, kSize);
        trimLabel.setBounds (trimKnob.getX() - 10, strip.getCentreY() - 48, kSize + 20, 20);
        trimCap.setBounds (trimKnob.getX() - 10, trimKnob.getBottom() - 2, kSize + 20, 18);

        mixKnob.setBounds (trimKnob.getX() - 110, strip.getCentreY() - 30, kSize, kSize);
        mixLabel.setBounds (mixKnob.getX() - 10, strip.getCentreY() - 48, kSize + 20, 20);
        mixCap.setBounds (mixKnob.getX() - 10, mixKnob.getBottom() - 2, kSize + 20, 18);
    }
}
