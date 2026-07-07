#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/Bounce.h"
#include "ui/KnobLookAndFeel.h"
#include "ui/Icons.h"
#include "ui/IconButton.h"
#include "ui/ArcMeter.h"
#include "ui/AirKnob.h"
#include "../../common/Licensing/ActivationOverlay.h"
#include "../../common/ui/Skin.h"
#include <array>

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
    juce::Label midVal, highVal;    // plate mode: live values under the rings
    juce::Label trimVal;            // plate mode: live dB readout under trim
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

    // Baked photoreal plate pair (air-chassis / air-chassis-on). The OFF plate
    // is drawn full-bleed; lit states are masked from the ON plate.
    juce::Image chassisImg, chassisOnImg;

    // crop-to-chrome: the plate's bright bbox inside the generated canvas; the
    // editor shows ONLY this region (backdrop never visible). Scaled copies are
    // cached per resize so per-frame paints are 1:1 blits.
    juce::Rectangle<int> plateCrop;
    juce::Image plateScaled, plateOnScaled;

    // dirty-region repaint bookkeeping (only repaint what changed each tick)
    std::array<double, 3> shownKnob { -1.0e9, -1.0e9, -1.0e9 };
    int shownAB = -1;
    bool shownLink = false, shownPower = false;

    juce::Rectangle<int> plateFracRect (float fx, float fy, float fw, float fh) const;
    void maskFromOn (juce::Graphics&, juce::Rectangle<int> screenRect);
    void maskFromOnFeathered (juce::Graphics&, juce::Rectangle<int> screenRect, int featherPx);
    void drawRingWedge (juce::Graphics&, juce::Slider&, float cxFrac, float cyFrac,
                        float domeRFrac, float solidRFrac, float maxRFrac);
    void drawMeterArc (juce::Graphics&);
    void paintPlate (juce::Graphics&);

    // Full-editor "enter your license key" overlay (shown until activated).
    ActivationOverlay licenseOverlay { proc.license, "VocalAir", "https://vocalessential.com",
                                       [] (float h, bool b) { return theme::font (h, b); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalAirEditor)
};
