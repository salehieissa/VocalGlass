#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/Bounce.h"
#include "ui/KnobLookAndFeel.h"
#include "ui/LabeledKnob.h"
#include "ui/ClipDisplay.h"
#include "../../common/Licensing/ActivationOverlay.h"
#include <array>
#include <memory>

//==============================================================================
class VocalClipEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit VocalClipEditor (VocalClipProcessor&);
    ~VocalClipEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void stepProgram (int delta);
    void setShape (int shapeIndex);
    void syncShapeButtons();

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
    std::array<double, 4> shownKnob {};
    int shownShape = -1;
    bool shownHq = true;
    juce::String shownPreset;

    VocalClipProcessor& proc;
    KnobLookAndFeel laf;

    // branding (painted as a two-tone wordmark; labels anchor the layout)
    juce::Label brand, brandSub;

    // preset capsule (top-right): dropdown + prev/next steppers
    juce::ComboBox presetBox;
    Bouncy<juce::TextButton> prevBtn { "<" }, nextBtn { ">" };

    // live clip screen — the centrepiece
    ClipDisplay display { proc.engine };

    // hero row: drive + ceiling with the SOFT/WARM/HARD selector between them
    LabeledKnob driveKnob   { "drive", true };
    LabeledKnob ceilingKnob { "ceiling", true };
    Bouncy<juce::TextButton> softBtn { "SOFT" }, warmBtn { "WARM" }, hardBtn { "HARD" };

    // second row: mix + output small knobs, HQ pill
    LabeledKnob mixKnob { "mix" }, outputKnob { "output" };
    Bouncy<juce::TextButton> hqBtn { "HQ 4X" };
    std::unique_ptr<ButtonAtt> hqAtt;

    // attachments
    std::array<std::unique_ptr<SliderAtt>, 4> knobAtt;

    // painted regions
    juce::Rectangle<int> arrowBox, shapeBox;

    // Full-editor "enter your license key" overlay (shown until activated).
    ActivationOverlay licenseOverlay { proc.license, "VocalClip", "https://vocalessential.com",
                                       [] (float h, bool b) { return theme::font (h, b); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalClipEditor)
};
