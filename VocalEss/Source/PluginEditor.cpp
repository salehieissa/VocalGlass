#include "PluginEditor.h"
#include "ui/Theme.h"

//==============================================================================
juce::RangedAudioParameter& VocalEssEditor::param (VocalEssProcessor& p, const char* id)
{
    return *p.apvts.getParameter (id);
}

//==============================================================================
VocalEssEditor::VocalEssEditor (VocalEssProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      freqBox (param (p, "freq")),
      scBtn   (param (p, "scType"))
{
    setLookAndFeel (&lnf);

    // Split toggle
    splitBtn.setClickingTogglesState (true);
    splitBtn.setButtonText ("Split");
    addAndMakeVisible (splitBtn);
    splitAtt = std::make_unique<ButtonAtt> (proc.apvts, "split", splitBtn);

    addAndMakeVisible (freqBox);
    addAndMakeVisible (scBtn);

    // Monitor radio
    for (auto* b : { &monAudio, &monSChain })
        addAndMakeVisible (b);
    monAudio .onClick = [this] { setMonitor (0); };
    monSChain.onClick = [this] { setMonitor (1); };

    // Threshold slider + bubble
    addAndMakeVisible (thresholdSlider);
    // Grab-and-drag (relative) instead of jumping to the click position, plus a
    // longer drag throw — makes the threshold feel smooth rather than steppy.
    thresholdSlider.setSliderSnapsToMousePosition (false);
    thresholdSlider.setMouseDragSensitivity (520);
    thresholdSlider.setVelocityBasedMode (false);
    threshAtt = std::make_unique<SliderAtt> (proc.apvts, "threshold", thresholdSlider);
    thresholdSlider.onValueChange = [this] { updateBubble(); };

    auto initValue = [this] (juce::Label& l)
    {
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, theme::accent);
        l.setFont (theme::font (13.0f, true));
        addAndMakeVisible (l);
    };
    initValue (scValue);
    initValue (attenValue);
    initValue (outLValue);
    initValue (outRValue);

    addAndMakeVisible (scBar);
    addAndMakeVisible (attenBar);
    addAndMakeVisible (outLBar);
    addAndMakeVisible (outRBar);

    // Added last so the floating readout sits on top of the meter bars.
    addAndMakeVisible (threshBubble);

    startTimerHz (30);
    setSize (680, 500);
}

VocalEssEditor::~VocalEssEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void VocalEssEditor::setMonitor (int index)
{
    if (auto* pm = proc.apvts.getParameter ("monitor"))
        pm->setValueNotifyingHost (pm->convertTo0to1 ((float) index));
}

void VocalEssEditor::updateBubble()
{
    const auto b = thresholdSlider.getBounds();
    const double prop = thresholdSlider.valueToProportionOfLength (thresholdSlider.getValue());
    const int y = b.getBottom() - (int) std::round (prop * b.getHeight());
    threshBubble.setBounds (b.getCentreX() + 4, y - 11, 50, 22);
    threshBubble.toFront (false);
}

//==============================================================================
void VocalEssEditor::timerCallback()
{
    auto fmtDb = [] (float db) { return db < -90.0f ? juce::String ("-inf")
                                                    : juce::String (db, 1); };

    scBar.setLevelDb (proc.scLevelDb.load());
    attenBar.setLevelDb (proc.attenDb.load());
    outLBar.setLevelDb (proc.outLDb.load());
    outRBar.setLevelDb (proc.outRDb.load());

    scValue  .setText (fmtDb (proc.scLevelDb.load()), juce::dontSendNotification);
    attenValue.setText (proc.attenDb.load() < 0.05f ? "0.0"
                        : juce::String (-proc.attenDb.load(), 1), juce::dontSendNotification);
    outLValue.setText (fmtDb (proc.outLDb.load()), juce::dontSendNotification);
    outRValue.setText (fmtDb (proc.outRDb.load()), juce::dontSendNotification);

    threshBubble.setText (juce::String (proc.apvts.getRawParameterValue ("threshold")->load(), 1));

    const int mon = (int) proc.apvts.getRawParameterValue ("monitor")->load();
    monAudio .setToggleState (mon == 0, juce::dontSendNotification);
    monSChain.setToggleState (mon == 1, juce::dontSendNotification);

    const bool split = proc.apvts.getRawParameterValue ("split")->load() > 0.5f;
    splitBtn.setButtonText (split ? "Split" : "Wide");

    freqBox.refresh();
    scBtn.refresh();
    updateBubble();
}

//==============================================================================
void VocalEssEditor::paint (juce::Graphics& g)
{
    theme::backdrop (g, getLocalBounds());

    auto card = [&] (juce::Rectangle<int> r, float radius = 14.0f)
    {
        auto rf = r.toFloat();
        theme::elevate (g, rf, radius);
        g.setColour (theme::card);
        g.fillRoundedRectangle (rf, radius);
        theme::topHighlight (g, rf, radius);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (rf, radius, 1.0f);
    };

    for (auto& c : leftCards) card (c);
    card (centerCard);
    card (rightCard);

    // Left card headers — letter-spaced uppercase section labels
    auto header = [&] (juce::Rectangle<int> c, const juce::String& t)
    {
        theme::spacedText (g, t, c.reduced (14, 11).removeFromTop (18).toFloat(),
                           theme::ink, 11.5f, 2.0f, true, juce::Justification::centred);
    };
    header (leftCards[0], "Audio");
    header (leftCards[1], "Frequency");
    header (leftCards[2], "SideChain");
    header (leftCards[3], "Monitor");

    // Center / right group headers — letter-spaced uppercase labels
    auto groupTitle = [&] (juce::Rectangle<int> area, const juce::String& t)
    {
        theme::spacedText (g, t, area.reduced (10, 0).toFloat(),
                           theme::ink, 12.0f, 2.4f, true, juce::Justification::centred);
    };
    {
        const int innerX = centerCard.getX() + 26;
        const int innerW = centerCard.getWidth() - 52;
        const int threshW = innerW * 6 / 10;
        groupTitle (juce::Rectangle<int> (innerX, centerCard.getY() + 18, threshW, 24), "Threshold");
        groupTitle (juce::Rectangle<int> (innerX + threshW, centerCard.getY() + 18, innerW - threshW, 24), "Atten");
    }
    groupTitle (juce::Rectangle<int> (rightCard.getX(), rightCard.getY() + 18, rightCard.getWidth(), 24), "Output");

    // Scales
    auto drawScale = [&] (juce::Rectangle<int> area,
                          std::initializer_list<std::pair<juce::String, float>> ticks)
    {
        for (auto& t : ticks)
        {
            const int y = area.getY() + (int) (t.second * area.getHeight());
            g.setColour (theme::cardLine);
            g.fillRect (area.getX(), y, 8, 1);
            g.setColour (theme::inkSoft);
            g.setFont (theme::font (10.0f, false));
            g.drawText (t.first, area.getX() + 12, y - 7, area.getWidth() - 12, 14,
                        juce::Justification::centredLeft);
        }
    };
    drawScale (threshScaleArea, { {"0",0.0f}, {"20",0.25f}, {"40",0.5f}, {"60",0.75f}, {"80",1.0f} });
    drawScale (attenScaleArea,  { {"0",0.0f}, {"-3",0.125f}, {"-6",0.25f}, {"-12",0.5f}, {"-18",0.75f}, {"-Inf",1.0f} });
    drawScale (outScaleArea,    { {"0",0.0f}, {"-6",0.2f}, {"-12",0.4f}, {"-18",0.6f}, {"-24",0.8f}, {"-30",1.0f} });

    // Value pills (drawn behind the labels, which are child components)
    auto valuePill = [&] (juce::Component& c)
    {
        auto r = c.getBounds().toFloat();
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (r, 10.0f, 1.0f);
    };
    valuePill (scValue);
    valuePill (attenValue);
    valuePill (outLValue);
    valuePill (outRValue);
}

//==============================================================================
void VocalEssEditor::resized()
{
    auto content = getLocalBounds().reduced (14);

    auto centred = [] (juce::Rectangle<int> area, int w, int h)
    {
        return juce::Rectangle<int> (area.getCentreX() - w / 2, area.getCentreY() - h / 2, w, h);
    };

    // --- left column ---
    auto left = content.removeFromLeft (150);
    content.removeFromLeft (12);
    {
        auto stack = left;
        const int gap = 10;
        const int total = stack.getHeight() - gap * 3;
        leftCards[0] = stack.removeFromTop (juce::roundToInt (total * 0.20f)); stack.removeFromTop (gap);
        leftCards[1] = stack.removeFromTop (juce::roundToInt (total * 0.20f)); stack.removeFromTop (gap);
        leftCards[2] = stack.removeFromTop (juce::roundToInt (total * 0.25f)); stack.removeFromTop (gap);
        leftCards[3] = stack;
    }

    // Audio / Split
    {
        auto c = leftCards[0].reduced (12); c.removeFromTop (20);
        splitBtn.setBounds (centred (c, 92, 30));
    }
    // Frequency
    {
        auto c = leftCards[1].reduced (12); c.removeFromTop (20);
        freqBox.setBounds (centred (c, 108, 34));
    }
    // SideChain filter
    {
        auto c = leftCards[2].reduced (12); c.removeFromTop (22);
        scBtn.setBounds (centred (c, 108, 40));
    }
    // Monitor stacked pills
    {
        auto c = leftCards[3].reduced (12); c.removeFromTop (20);
        monAudio .setBounds (c.removeFromTop (30).reduced (4, 1));
        c.removeFromTop (7);
        monSChain.setBounds (c.removeFromTop (30).reduced (4, 1));
    }

    // --- right card (Output) ---
    rightCard = content.removeFromRight (168);
    content.removeFromRight (12);
    {
        auto inner = rightCard.reduced (16);
        inner.removeFromTop (30);
        auto valueRow = inner.removeFromBottom (24);
        inner.removeFromTop (4); inner.removeFromBottom (8);

        auto meter = inner;
        auto la = meter.removeFromLeft (30);
        auto ra = meter.removeFromRight (30);
        outScaleArea = meter;
        outLBar.setBounds (centred (la, 16, la.getHeight()));
        outRBar.setBounds (centred (ra, 16, ra.getHeight()));

        outLValue.setBounds (centred (valueRow.removeFromLeft (valueRow.getWidth() / 2), 48, 22));
        outRValue.setBounds (centred (valueRow, 48, 22));
    }

    // --- center card (Threshold + Atten) ---
    centerCard = content;
    {
        auto inner = centerCard.reduced (18);
        inner.removeFromTop (30);
        auto valueRow = inner.removeFromBottom (24);
        inner.removeFromTop (4); inner.removeFromBottom (8);

        auto meter = inner;
        auto threshGroup = meter.removeFromLeft (meter.getWidth() * 58 / 100);
        auto attenGroup  = meter;

        // threshold group: slider | gap | scBar | scale
        thresholdSlider.setBounds (threshGroup.removeFromLeft (38));
        threshGroup.removeFromLeft (8);
        scBar.setBounds (centred (threshGroup.removeFromLeft (22), 16, threshGroup.getHeight()));
        threshScaleArea = threshGroup;

        // atten group: attenBar | scale
        attenBar.setBounds (centred (attenGroup.removeFromLeft (22), 16, attenGroup.getHeight()));
        attenGroup.removeFromLeft (6);
        attenScaleArea = attenGroup;

        // value pills under each group
        auto vThresh = valueRow.removeFromLeft (valueRow.getWidth() * 58 / 100);
        scValue.setBounds (centred (vThresh, 52, 22));
        attenValue.setBounds (centred (valueRow, 52, 22));
    }

    updateBubble();
}
