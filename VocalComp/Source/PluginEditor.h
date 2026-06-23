#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/Bounce.h"
#include "ui/KnobLookAndFeel.h"
#include "ui/MeterBar.h"
#include "ui/CurveDisplay.h"
#include <array>
#include <memory>

//==============================================================================
class VocalCompEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit VocalCompEditor (VocalCompProcessor&);
    ~VocalCompEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void setMode (int mode);
    void refreshPresetBox();

    VocalCompProcessor& proc;
    KnobLookAndFeel laf;

    // branding + presets
    juce::Label brand, brandSub;
    juce::ComboBox presetBox;

    // left column (threshold + input)
    juce::Label threshCap, threshVal, inputCap, inLVal, inRVal;
    juce::Slider threshSlider;
    MeterBar inMeterL, inMeterR;
    std::unique_ptr<SliderAtt> threshAtt;

    // centre (ratio)
    juce::Label ratioCap;
    CurveDisplay curve;
    std::unique_ptr<SliderAtt> ratioAtt;

    // right column (makeup + output)
    juce::Label gainCap, gainVal, outputCap, outLVal, outRVal;
    juce::Slider gainSlider;
    MeterBar outMeterL, outMeterR;
    std::unique_ptr<SliderAtt> gainAtt;

    // attack / release
    juce::Label attackCap, releaseCap, attackVal, releaseVal;
    juce::Slider attackSlider, releaseSlider;
    std::unique_ptr<SliderAtt> attackAtt, releaseAtt;

    // mode pills
    juce::Label modeCap;
    std::array<Bouncy<juce::TextButton>, 3> modeBtns;

    // gate / mix / trim
    juce::Label gateCap, gateVal, mixCap, trimCap, mixMin, mixMax, trimMin, trimMax;
    juce::Slider gateKnob, mixKnob, trimKnob;
    std::unique_ptr<SliderAtt> gateAtt, mixAtt, trimAtt;

    // layout rects painted in paint()
    juce::Rectangle<int> mainCard, curveCard, modeContainer, bottomStrip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalCompEditor)
};
