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
class VocalModEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit VocalModEditor (VocalModProcessor&);
    ~VocalModEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

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

    VocalModProcessor& proc;
    KnobLookAndFeel laf;

    // branding (painted as a two-tone wordmark; labels anchor the layout)
    juce::Label brand, brandSub;

    // preset capsule (top-right): dropdown + prev/next steppers
    juce::ComboBox presetBox;
    Bouncy<juce::TextButton> prevBtn { "<" }, nextBtn { ">" };

    // three-segment mode pill
    std::array<Bouncy<juce::TextButton>, 3> modeBtns;

    // hero row
    LabeledKnob rateKnob  { "rate", true };
    LabeledKnob depthKnob { "depth", true };
    Bouncy<juce::TextButton> syncBtn { "SYNC" };
    juce::ComboBox divBox;
    std::unique_ptr<ButtonAtt> syncAtt;
    std::unique_ptr<ComboAtt>  divAtt;

    // second row of small knobs
    LabeledKnob feedbackKnob { "feedback" }, mixKnob { "mix" };
    LabeledKnob widthKnob { "width" },       toneKnob { "tone" };

    // attachments
    std::array<std::unique_ptr<SliderAtt>, 6> knobAtt;

    // painted regions
    juce::Rectangle<int> arrowBox;

    // Full-editor "enter your license key" overlay (shown until activated).
    ActivationOverlay licenseOverlay { proc.license, "VocalMod", "https://vocalessential.com",
                                       [] (float h, bool b) { return theme::font (h, b); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalModEditor)
};
