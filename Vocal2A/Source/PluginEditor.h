#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/Bounce.h"
#include "ui/KnobLookAndFeel.h"
#include "ui/VintageKnob.h"
#include "ui/ToggleSwitch.h"
#include "ui/VUMeter.h"
#include <array>
#include <memory>

//==============================================================================
class Vocal2AEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit Vocal2AEditor (Vocal2AProcessor&);
    ~Vocal2AEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void setChoice (const juce::String& paramID, int index);

    Vocal2AProcessor& proc;
    KnobLookAndFeel laf;

    // branding
    juce::Label brand, brandSub;

    // VU source selector
    juce::Label vuTitle;
    std::array<Bouncy<juce::TextButton>, 3> vuButtons; // input / GR / output

    // main controls
    VintageKnob gainKnob { VintageKnob::Style::Large };
    VintageKnob peakKnob { VintageKnob::Style::Large };
    juce::Label gainCap, peakCap;

    ToggleSwitch modeSwitch;   // compress / limit
    ToggleSwitch autoSwitch;   // auto makeup
    juce::Label  modeCap, autoCap;

    VUMeter vu;

    // bottom strip
    juce::Label analogLabel;
    std::array<Bouncy<juce::TextButton>, 3> analogButtons; // 50Hz / 60Hz / off

    juce::Label  hiFreqLabel, mixLabel, trimLabel;
    juce::Label  hiFreqCap, mixCap, trimCap;
    VintageKnob  hiFreqKnob { VintageKnob::Style::Small };
    VintageKnob  mixKnob    { VintageKnob::Style::Small };
    VintageKnob  trimKnob   { VintageKnob::Style::Small };

    std::unique_ptr<SliderAtt> gainAtt, peakAtt, hiFreqAtt, mixAtt, trimAtt;
    std::unique_ptr<ButtonAtt> autoAtt;

    // layout rects
    juce::Rectangle<int> cardArea, vuCard, bottomStrip, analogPill;
    juce::Rectangle<int> brandBounds, brandSubBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Vocal2AEditor)
};
