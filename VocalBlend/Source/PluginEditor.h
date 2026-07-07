#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/Bounce.h"
#include "ui/KnobLookAndFeel.h"
#include "ui/LabeledKnob.h"
#include "../../common/Licensing/ActivationOverlay.h"
#include <array>
#include <memory>

//==============================================================================
class VocalBlendEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit VocalBlendEditor (VocalBlendProcessor&);
    ~VocalBlendEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void setChoiceParam (const char* id, int value);
    void stepProgram (int delta);

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

    // crop-to-chrome: the plate's bright bbox inside the generated canvas; the
    // editor shows ONLY this region (backdrop never visible). Scaled copies are
    // cached per resize so per-frame paints are 1:1 blits.
    juce::Rectangle<int> plateCrop;
    juce::Image plateScaled, plateOnScaled;

    // dirty-region repaint bookkeeping (only repaint what changed each tick)
    std::array<double, 6> shownKnob {};
    std::array<juce::String, 6> shownKnobText;
    int shownMode = -1;
    bool shownLimit = false;
    juce::String shownGrText, shownPreset;

    VocalBlendProcessor& proc;
    KnobLookAndFeel laf;

    // branding (painted as a two-tone wordmark; labels anchor the layout)
    juce::Label brand, brandSub;

    // preset capsule (top-right): dropdown + prev/next steppers
    juce::ComboBox presetBox;
    Bouncy<juce::TextButton> prevBtn { "<" }, nextBtn { ">" };

    // three-segment mode pill
    std::array<Bouncy<juce::TextButton>, 3> modeBtns;

    // hero row
    LabeledKnob blendKnob { "blend", true };
    LabeledKnob glueKnob  { "glue", true };
    Bouncy<juce::TextButton> limitBtn { "LIMIT" };
    std::unique_ptr<ButtonAtt> limitAtt;

    // second row of small knobs
    LabeledKnob warmthKnob { "warmth" }, airKnob { "air" };
    LabeledKnob widthKnob { "width" },   outputKnob { "output" };

    // attachments
    std::array<std::unique_ptr<SliderAtt>, 6> knobAtt;

    // painted regions
    juce::Rectangle<int> arrowBox;

    // glue gain-reduction readout (smoothed in the timer)
    float shownGr = 0.0f;

    // Full-editor "enter your license key" overlay (shown until activated).
    ActivationOverlay licenseOverlay { proc.license, "VocalBlend", "https://vocalessential.com",
                                       [] (float h, bool b) { return theme::font (h, b); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalBlendEditor)
};
