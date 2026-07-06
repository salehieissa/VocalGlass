#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/GateLookAndFeel.h"
#include "ui/GateKnob.h"
#include "ui/GRMeter.h"
#include "ui/IconButton.h"
#include "ui/Bounce.h"
#include "../../common/ui/Skin.h"
#include "../../common/Licensing/ActivationOverlay.h"

using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

//==============================================================================
class VocalGateEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit VocalGateEditor (VocalGateProcessor&);
    ~VocalGateEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void stepProgram (int delta);
    void configureKnob (GateKnob&, const juce::String& paramId, std::unique_ptr<SliderAtt>&);
    void initValue (juce::Label&, float height = 13.0f);

    // ---- baked-plate (photoreal skin) mode ----
    void setupPlateMode();
    void paintPlate (juce::Graphics&);
    void layoutPlate();
    juce::Rectangle<int> plateFracRect (float fx0, float fy0, float fx1, float fy1) const;
    void maskFromOn (juce::Graphics&, juce::Rectangle<int> screenRect);
    void maskFromOnFeathered (juce::Graphics&, juce::Rectangle<int> screenRect, int featherPx);
    void drawRingWedge (juce::Graphics&, juce::Slider&, float cxFrac, float cyFrac,
                        float domeRFrac, float solidRFrac, float maxRFrac);

    juce::Image chassisImg, chassisOnImg;
    bool plateBaked = false;

    VocalGateProcessor& proc;
    GateLookAndFeel lnf;

    // Header: wordmark is painted; preset capsule + steppers
    juce::Label brand, brandSub;
    juce::Label presetName;
    IconButton prevBtn { IconButton::Icon::Prev };
    IconButton nextBtn { IconButton::Icon::Next };

    // Left: gain-reduction meter
    GRMeter grMeter;
    juce::Label grValue;

    // Centre: hero threshold + range
    GateKnob threshKnob { 4.0f }, rangeKnob;
    std::unique_ptr<SliderAtt> threshAtt, rangeAtt;
    juce::Label threshValue, rangeValue;

    // Right: 2x2 timing grid + bottom row
    GateKnob attackKnob, holdKnob, releaseKnob, hystKnob, hpfKnob;
    std::unique_ptr<SliderAtt> attackAtt, holdAtt, releaseAtt, hystAtt, hpfAtt;
    juce::Label attackValue, holdValue, releaseValue, hystValue, hpfValue;

    Bouncy<juce::TextButton> scListenBtn { "SC LISTEN" };
    std::unique_ptr<ButtonAtt> scListenAtt;

    // Geometry remembered for paint()
    juce::Rectangle<int> leftCard, centerCard, rightCard, presetPill;

    // Full-editor "enter your license key" overlay (shown until activated).
    ActivationOverlay licenseOverlay { proc.license, "VocalGate", "https://vocalessential.com",
                                       [] (float h, bool b) { return theme::font (h, b); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalGateEditor)
};
