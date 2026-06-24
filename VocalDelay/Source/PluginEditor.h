#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/Bounce.h"
#include "ui/KnobLookAndFeel.h"
#include "ui/LabeledKnob.h"
#include "ui/Meter.h"
#include "ui/DisplayCard.h"
#include "ui/TapButton.h"
#include "ui/LinkButton.h"
#include "../../common/Licensing/ActivationOverlay.h"
#include <array>
#include <vector>

//==============================================================================
class VocalDelayEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit VocalDelayEditor (VocalDelayProcessor&);
    ~VocalDelayEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void setChoiceParam (const char* id, int value);
    void registerTap();

    VocalDelayProcessor& proc;
    KnobLookAndFeel laf;

    // branding
    juce::Label brand, brandSub, tapLabel;

    // far-left column
    TapButton tapButton;
    Bouncy<juce::TextButton> lofiBtn { "LoFi" };
    std::unique_ptr<ButtonAtt> lofiAtt;

    // big knobs
    LabeledKnob delayKnob { "delay", true };
    LabeledKnob feedbackKnob { "feedback", true };

    // mode + sync segmented controls
    std::array<Bouncy<juce::TextButton>, 4> modeBtns;
    std::array<Bouncy<juce::TextButton>, 3> syncBtns;

    // centre display
    DisplayCard display;

    // meters
    Meter meter { proc.engine.peakL, proc.engine.peakR };

    // modulation + filters
    LabeledKnob depthKnob { "depth" }, rateKnob { "rate" };
    LabeledKnob hipassKnob { "hipass" }, lopassKnob { "lopass" };
    LinkButton linkBtn;
    std::unique_ptr<ButtonAtt> linkAtt;
    juce::Label linkLabel, modSection, filtSection;
    bool linkActive = false, linkGuard = false;

    // right column
    LabeledKnob drywetKnob { "dry/wet" }, outputKnob { "output" }, analogKnob { "analog" };

    // attachments
    std::array<std::unique_ptr<SliderAtt>, 8> knobAtt;
    std::unique_ptr<SliderAtt> analogAtt;
    std::unique_ptr<SliderAtt> delayAtt;
    juce::String delayAttId;

    // tap-tempo state
    std::vector<double> tapTimes;

    // Full-editor "enter your license key" overlay (shown until activated).
    ActivationOverlay licenseOverlay { proc.license, "VocalDelay" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalDelayEditor)
};
