#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/Bounce.h"
#include "ui/KnobLookAndFeel.h"
#include "ui/MeterBar.h"
#include "ui/CurveDisplay.h"
#include "../../common/ui/Skin.h"
#include "../../common/Licensing/ActivationOverlay.h"
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

    // ---- baked photoreal plate ----
    juce::Image chassisImg, chassisOnImg;
    float inLDb = -60.0f, inRDb = -60.0f, outLDb = -60.0f, outRDb = -60.0f;

    juce::Rectangle<int> plateFracRect (float fx, float fy, float fw, float fh) const;
    void maskFromOn (juce::Graphics& g, juce::Rectangle<int> screenRect);
    void maskFromOnFeathered (juce::Graphics& g, juce::Rectangle<int> screenRect, int featherPx);
    void drawRingWedge (juce::Graphics& g, juce::Slider& s, float cxFrac, float cyFrac,
                        float domeRFrac, float solidRFrac, float maxRFrac);
    void paintPlate (juce::Graphics& g);

    // Full-editor "enter your license key" overlay (shown until activated).
    ActivationOverlay licenseOverlay { proc.license, "VocalComp", "https://vocalessential.com",
                                       [] (float h, bool b) { return theme::font (h, b); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalCompEditor)
};
