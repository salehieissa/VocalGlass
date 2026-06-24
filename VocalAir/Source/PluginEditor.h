#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "licensing/LicenseGate.h"
#include "ui/Theme.h"
#include "ui/Bounce.h"
#include "ui/KnobLookAndFeel.h"
#include "ui/Icons.h"
#include "ui/IconButton.h"
#include "ui/ArcMeter.h"
#include "ui/AirKnob.h"

//==============================================================================
class VocalAirEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit VocalAirEditor (VocalAirProcessor&);
    ~VocalAirEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void refreshPresetBox();
    void selectABSlot (int slot);
    void mirrorLink (bool fromMid);

    VocalAirProcessor& proc;
    KnobLookAndFeel laf;

    // branding
    juce::Label brand, brandSub, displayLabel;

    // top-bar controls
    IconButton undoBtn, redoBtn, saveBtn, menuBtn;
    Bouncy<juce::TextButton> abA { "A" }, abB { "B" };
    IconButton abArrow;
    juce::ComboBox presetBox;

    // meter
    ArcMeter meter;

    // knobs
    AirKnob midKnob, highKnob;
    juce::Label midLo, midHi, highLo, highHi;
    IconButton linkBtn;

    // bottom controls
    juce::Label powerLabel, trimLabel;
    IconButton powerBtn;
    MiniKnob trimKnob;

    std::unique_ptr<SliderAtt> midAtt, highAtt, trimAtt;
    std::unique_ptr<ButtonAtt> linkAtt, powerAtt;

    // A/B compare snapshots
    juce::ValueTree abState[2];
    int abSlot = 0;

    bool linking = false;

    // layout rectangles
    juce::Rectangle<int> displayCard;

    std::unique_ptr<licensing::LicenseGate> licenseGate;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalAirEditor)
};
