#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/Bounce.h"
#include "ui/KnobLookAndFeel.h"
#include "ui/KnobDial.h"
#include "../../common/Licensing/ActivationOverlay.h"
#include "../../common/ui/Skin.h"
#include <array>

//==============================================================================
class VocalKnobEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit VocalKnobEditor (VocalKnobProcessor&);
    ~VocalKnobEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setMode (int mode);
    void stepProgram (int delta);

    VocalKnobProcessor& proc;
    KnobLookAndFeel laf;

    KnobDial dial;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAtt;

    std::array<Bouncy<juce::TextButton>, 4> modeBtns;
    juce::Label subtitle;
    std::array<juce::Label, 4> capLabels;

    juce::Label brand, brandSub;
    Bouncy<juce::TextButton> prevBtn { "<" }, nextBtn { ">" };
    juce::Label presetName;

    juce::Rectangle<int> cardArea, presetPill;

    // Baked photoreal plate pair (knob-chassis / knob-chassis-on). The OFF
    // plate is drawn full-bleed; lit states are masked from the ON plate.
    juce::Image chassisImg, chassisOnImg;

    // crop-to-chrome: the plate's bright bbox inside the generated canvas; the
    // editor shows ONLY this region (backdrop never visible). Scaled copies are
    // cached per resize so per-frame paints are 1:1 blits.
    juce::Rectangle<int> plateCrop;
    juce::Image plateScaled, plateOnScaled;

    // dirty-region repaint bookkeeping (only repaint what changed each tick)
    double shownDial = -1.0e9;
    int shownMode = -1;

    juce::Rectangle<int> plateFracRect (float fx, float fy, float fw, float fh) const;
    void maskFromOn (juce::Graphics&, juce::Rectangle<int> screenRect);
    void maskFromOnFeathered (juce::Graphics&, juce::Rectangle<int> screenRect, int featherPx);
    void drawDialWedge (juce::Graphics&);
    void paintPlate (juce::Graphics&);

    // Full-editor "enter your license key" overlay (shown until activated).
    ActivationOverlay licenseOverlay { proc.license, "VocalKnob", "https://vocalessential.com",
                                       [] (float h, bool b) { return theme::font (h, b); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalKnobEditor)
};
