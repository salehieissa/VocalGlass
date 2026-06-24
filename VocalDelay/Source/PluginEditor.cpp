#include "PluginEditor.h"

namespace
{
    const juce::String kPhiL = juce::String (juce::CharPointer_UTF8 ("\xC3\x98L")); // "ØL"
    const juce::String kPhiR = juce::String (juce::CharPointer_UTF8 ("\xC3\x98R")); // "ØR"
    const std::array<juce::String, 4> kModeWords { kPhiL, "PING PONG", "DUAL", kPhiR };
    const std::array<juce::String, 3> kSyncWords { "BPM", "HOST", "MS" };

    juce::String hzText (float hz)
    {
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " kHz"
                             : juce::String (juce::roundToInt (hz)) + " Hz";
    }
}

//==============================================================================
VocalDelayEditor::VocalDelayEditor (VocalDelayProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);

    // ---- branding ----
    // The wordmark is painted as a two-tone "vocal"+"delay" in paint(); the
    // label keeps its slot in the layout but draws nothing.
    brand.setText ({}, juce::dontSendNotification);
    brand.setFont (theme::font (26.0f, true));
    brand.setColour (juce::Label::textColourId, theme::ink);
    addAndMakeVisible (brand);

    brandSub.setText ("VOCAL DELAY", juce::dontSendNotification);
    brandSub.setFont (theme::font (12.0f, false));
    brandSub.setColour (juce::Label::textColourId, theme::inkSoft);
    addAndMakeVisible (brandSub);

    // ---- far-left column ----
    tapButton.onClick = [this] { registerTap(); };
    addAndMakeVisible (tapButton);

    tapLabel.setText ("TAP", juce::dontSendNotification);
    tapLabel.setJustificationType (juce::Justification::centred);
    tapLabel.setFont (theme::font (15.0f, false));
    tapLabel.setColour (juce::Label::textColourId, theme::ink);
    addAndMakeVisible (tapLabel);

    lofiBtn.setClickingTogglesState (true);
    addAndMakeVisible (lofiBtn);
    lofiAtt = std::make_unique<ButtonAtt> (proc.apvts, "lofi", lofiBtn);

    // ---- knobs + attachments ----
    auto addKnob = [this] (LabeledKnob& k, const char* id, int slot)
    {
        addAndMakeVisible (k);
        knobAtt[(size_t) slot] = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
    };
    addKnob (feedbackKnob, "feedback", 0);
    addKnob (depthKnob,    "depth",    1);
    addKnob (rateKnob,     "rate",     2);
    addKnob (hipassKnob,   "hipass",   3);
    addKnob (lopassKnob,   "lopass",   4);
    addKnob (drywetKnob,   "drywet",   5);
    addKnob (outputKnob,   "output",   6);
    addAndMakeVisible (analogKnob);
    analogAtt = std::make_unique<SliderAtt> (proc.apvts, "analog", analogKnob.slider);

    // The big delay knob controls the musical division when synced, or the free
    // millisecond time in MS mode — its attachment is swapped to match.
    addAndMakeVisible (delayKnob);
    delayAttId = "division";
    delayAtt = std::make_unique<SliderAtt> (proc.apvts, delayAttId, delayKnob.slider);

    // ---- mode segmented ----
    for (int i = 0; i < 4; ++i)
    {
        auto& b = modeBtns[(size_t) i];
        b.setButtonText (kModeWords[(size_t) i]);
        b.setClickingTogglesState (false);
        b.onClick = [this, i] { setChoiceParam ("mode", i); };
        addAndMakeVisible (b);
    }

    // ---- sync segmented ----
    for (int i = 0; i < 3; ++i)
    {
        auto& b = syncBtns[(size_t) i];
        b.setButtonText (kSyncWords[(size_t) i]);
        b.setClickingTogglesState (false);
        b.onClick = [this, i] { setChoiceParam ("syncMode", i); };
        addAndMakeVisible (b);
    }

    // ---- centre display + meters ----
    addAndMakeVisible (display);
    addAndMakeVisible (meter);

    // ---- filters link ----
    addAndMakeVisible (linkBtn);
    linkAtt = std::make_unique<ButtonAtt> (proc.apvts, "filterLink", linkBtn);

    linkLabel.setText ("LINK", juce::dontSendNotification);
    linkLabel.setJustificationType (juce::Justification::centred);
    linkLabel.setFont (theme::font (11.0f, false));
    linkLabel.setColour (juce::Label::textColourId, theme::inkSoft);
    addAndMakeVisible (linkLabel);

    auto linkFrom = [this] (juce::Slider& from, juce::Slider& to)
    {
        if (! linkActive || linkGuard) return;
        linkGuard = true;
        const double prop = from.valueToProportionOfLength (from.getValue());
        to.setValue (to.proportionOfLengthToValue (prop), juce::sendNotificationSync);
        linkGuard = false;
    };
    hipassKnob.slider.onValueChange = [this, linkFrom] { linkFrom (hipassKnob.slider, lopassKnob.slider); };
    lopassKnob.slider.onValueChange = [this, linkFrom] { linkFrom (lopassKnob.slider, hipassKnob.slider); };

    // ---- section captions ----
    for (auto* l : { &modSection, &filtSection })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setFont (theme::font (11.0f, false));
        l->setColour (juce::Label::textColourId, theme::inkSoft);
        addAndMakeVisible (*l);
    }
    modSection.setText ("MODULATION", juce::dontSendNotification);
    filtSection.setText ("FILTERS", juce::dontSendNotification);

    startTimerHz (24);
    setSize (1024, 640);

    licenseGate = std::make_unique<licensing::LicenseGate> (*this, "VocalDelay", "VOCALDELAY");
}

VocalDelayEditor::~VocalDelayEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void VocalDelayEditor::setChoiceParam (const char* id, int value)
{
    if (auto* param = proc.apvts.getParameter (id))
        param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 ((float) value));
}

void VocalDelayEditor::registerTap()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (! tapTimes.empty() && now - tapTimes.back() > 2000.0)
        tapTimes.clear();

    tapTimes.push_back (now);
    while (tapTimes.size() > 4) tapTimes.erase (tapTimes.begin());

    if (tapTimes.size() >= 2)
    {
        double sum = 0.0;
        for (size_t i = 1; i < tapTimes.size(); ++i)
            sum += tapTimes[i] - tapTimes[i - 1];
        const double avg = sum / (double) (tapTimes.size() - 1);
        if (avg > 1.0)
        {
            const float bpm = juce::jlimit (40.0f, 300.0f, (float) (60000.0 / avg));
            if (auto* param = proc.apvts.getParameter ("bpm"))
                param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (bpm));
            setChoiceParam ("syncMode", 0); // tap implies internal BPM
        }
    }
}

//==============================================================================
void VocalDelayEditor::timerCallback()
{
    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    const int sync = (int) proc.apvts.getRawParameterValue ("syncMode")->load();
    linkActive = proc.apvts.getRawParameterValue ("filterLink")->load() > 0.5f;

    for (int i = 0; i < 4; ++i)
        modeBtns[(size_t) i].setToggleState (i == mode, juce::dontSendNotification);
    for (int i = 0; i < 3; ++i)
        syncBtns[(size_t) i].setToggleState (i == sync, juce::dontSendNotification);

    // swap the delay-knob attachment between division (synced) and msTime (free)
    const juce::String wantId = (sync == 2) ? "msTime" : "division";
    if (wantId != delayAttId)
    {
        delayAtt.reset();
        delayAttId = wantId;
        delayAtt = std::make_unique<SliderAtt> (proc.apvts, delayAttId, delayKnob.slider);
    }

    // knob value read-outs
    delayKnob.setValueText (proc.getDivisionText());
    feedbackKnob.setValueText (juce::String (juce::roundToInt (proc.apvts.getRawParameterValue ("feedback")->load())) + "%");
    depthKnob.setValueText (juce::String (juce::roundToInt (proc.apvts.getRawParameterValue ("depth")->load())) + "%");
    rateKnob.setValueText (juce::String (proc.apvts.getRawParameterValue ("rate")->load(), 2) + " Hz");
    hipassKnob.setValueText (hzText (proc.apvts.getRawParameterValue ("hipass")->load()));
    lopassKnob.setValueText (hzText (proc.apvts.getRawParameterValue ("lopass")->load()));
    drywetKnob.setValueText (juce::String (juce::roundToInt (proc.apvts.getRawParameterValue ("drywet")->load())) + "%");
    outputKnob.setValueText (juce::String (proc.apvts.getRawParameterValue ("output")->load(), 1) + " dB");
    analogKnob.setValueText (juce::String (juce::roundToInt (proc.apvts.getRawParameterValue ("analog")->load())));

    // centre display
    if (sync == 2)
    {
        const float ms = proc.getActiveDelayMs();
        display.setBigText (juce::String (ms, ms < 100.0f ? 1 : 0));
        display.setUnitText ("MS");
    }
    else
    {
        display.setBigText (juce::String (juce::roundToInt (proc.getActiveBpm())));
        display.setUnitText ("BPM");
    }
    display.setTaps (proc.apvts.getRawParameterValue ("feedback")->load() * 0.01f);
}

//==============================================================================
void VocalDelayEditor::paint (juce::Graphics& g)
{
    // warm off-white gradient backdrop + soft top light
    theme::backdrop (g, getLocalBounds());

    // main panel: a floating card surface (top highlight + hairline)
    auto frame = getLocalBounds().toFloat().reduced (8.0f);
    g.setColour (theme::card);
    g.fillRoundedRectangle (frame, 26.0f);
    theme::topHighlight (g, frame, 26.0f);
    g.setColour (theme::cardLine);
    g.drawRoundedRectangle (frame, 26.0f, 1.2f);

    // two-tone wordmark: ink "vocal" + accent "delay" with an accent underline
    {
        auto wm = theme::font (26.0f, true);
        g.setFont (wm);
        const float vw = juce::GlyphArrangement::getStringWidth (wm, "vocal");
        g.setColour (theme::ink);
        g.drawText ("vocal", 28, 22, 240, 34, juce::Justification::left);
        g.setColour (theme::accent);
        g.drawText ("delay", 28 + (int) vw, 22, 240, 34, juce::Justification::left);
        g.setColour (theme::accent);
        g.fillRoundedRectangle (29.0f, 51.0f, 22.0f, 2.5f, 1.25f);
    }

    // soft float shadow behind the centre display (drawn here so it never clips)
    theme::elevate (g, display.getBounds().toFloat(), 14.0f);

    // section bracket lines under MODULATION / FILTERS
    g.setColour (theme::cardLine);
    g.drawLine (176.0f, 548.0f, 382.0f, 548.0f, 1.2f);          // modulation bracket
    g.drawLine (452.0f, 548.0f, 732.0f, 548.0f, 1.2f);          // filters bracket
    g.drawLine (418.0f, 392.0f, 418.0f, 548.0f, 1.2f);          // vertical divider
}

//==============================================================================
void VocalDelayEditor::resized()
{
    // ---- branding ----
    brand.setBounds (28, 22, 240, 34);
    brandSub.setBounds (30, 56, 240, 18);

    // ---- far-left column ----
    tapButton.setBounds (30, 130, 124, 124);
    tapLabel.setBounds (30, 260, 124, 20);
    lofiBtn.setBounds (38, 336, 108, 40);

    // ---- big delay knob ----
    delayKnob.setBounds (176, 96, 158, 214);

    // ---- mode segmented row ----
    {
        const int y = 112, h = 34;
        int x = 376;
        const int widths[4] = { 48, 92, 62, 48 };
        for (int i = 0; i < 4; ++i)
        {
            modeBtns[(size_t) i].setBounds (x, y, widths[i], h);
            x += widths[i] + 6;
        }
    }

    // ---- centre display ----
    display.setBounds (376, 158, 264, 118);

    // ---- sync segmented row ----
    {
        const int y = 300, h = 34;
        const int w = 84;
        int x = 376;
        for (int i = 0; i < 3; ++i)
        {
            syncBtns[(size_t) i].setBounds (x, y, w, h);
            x += w + 6;
        }
    }

    // ---- big feedback knob ----
    feedbackKnob.setBounds (652, 96, 158, 214);

    // ---- meters ----
    meter.setBounds (824, 116, 104, 300);

    // ---- right column ----
    drywetKnob.setBounds (938, 108, 70, 120);
    outputKnob.setBounds (938, 250, 70, 120);
    analogKnob.setBounds (938, 392, 70, 120);

    // ---- modulation ----
    depthKnob.setBounds (180, 396, 92, 132);
    rateKnob.setBounds (288, 396, 92, 132);
    modSection.setBounds (176, 552, 206, 18);

    // ---- filters ----
    hipassKnob.setBounds (452, 396, 92, 132);
    lopassKnob.setBounds (640, 396, 92, 132);
    linkBtn.setBounds (568, 432, 52, 52);
    linkLabel.setBounds (548, 486, 92, 16);
    filtSection.setBounds (452, 552, 280, 18);
}
