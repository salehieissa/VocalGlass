#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "licensing/LicenseGate.h"
#include "ui/EssLookAndFeel.h"
#include "ui/Meters.h"
#include "ui/Controls.h"
#include "ui/Bounce.h"

using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

//==============================================================================
class VocalEssEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit VocalEssEditor (VocalEssProcessor&);
    ~VocalEssEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setMonitor (int index);
    void updateBubble();
    static juce::RangedAudioParameter& param (VocalEssProcessor& p, const char* id);

    VocalEssProcessor& proc;
    EssLookAndFeel lnf;

    // Left column controls
    Bouncy<juce::TextButton> splitBtn;
    std::unique_ptr<ButtonAtt> splitAtt;
    FreqBox        freqBox;
    SCFilterButton scBtn;
    Bouncy<juce::TextButton> monAudio { "Audio" }, monSChain { "S Chain" };

    // Threshold
    juce::Slider thresholdSlider { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    std::unique_ptr<SliderAtt> threshAtt;
    Bubble threshBubble;

    // Meters
    Bar scBar    { -80.0f, 0.0f, false };
    Bar attenBar {   0.0f, 24.0f, true };
    Bar outLBar  { -30.0f, 0.0f, false };
    Bar outRBar  { -30.0f, 0.0f, false };

    // Numeric readouts
    juce::Label scValue, attenValue, outLValue, outRValue;

    // Geometry remembered for paint()
    std::array<juce::Rectangle<int>, 4> leftCards;
    juce::Rectangle<int> centerCard, rightCard;
    juce::Rectangle<int> threshScaleArea, attenScaleArea, outScaleArea;

    std::unique_ptr<licensing::LicenseGate> licenseGate;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalEssEditor)
};
