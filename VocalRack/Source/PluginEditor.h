#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/RackLookAndFeel.h"
#include "ui/RackKnob.h"
#include "ui/PowerButton.h"
#include "ui/ActivityBar.h"
#include "ui/VMeter.h"
#include "ui/IconButton.h"
#include "ui/Bounce.h"
#include "../../common/ui/Skin.h"
#include "../../common/Licensing/ActivationOverlay.h"
#include <array>
#include <vector>

using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

//==============================================================================
// The rack: nine horizontal module rows (GATE → DE-ESS → EQ → COMP → HEAT →
// AIR → DELAY send → REVERB send → CLIP) stacked like hardware rack units,
// plus an OUTPUT card with the IN/OUT meter pair and the output gain knob on
// the right.
//==============================================================================
class VocalRackEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit VocalRackEditor (VocalRackProcessor&);
    ~VocalRackEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void stepProgram (int delta);
    void configureKnob (RackKnob&, const juce::String& paramId, std::unique_ptr<SliderAtt>&);
    void initValue (juce::Label&, float height = 12.5f);

    // ---- user presets (editor-side .vepreset XML files) ----
    static juce::File presetDirectory();
    void rebuildPresetItems();
    void promptSavePreset();
    void saveUserPreset (const juce::String& name);
    void loadUserPreset (const juce::File&);
    void importPresetDialog();
    void exportPresetDialog();

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
    // editor shows ONLY this region. Scaled copies cached per resize so
    // per-frame paints are 1:1 blits.
    juce::Rectangle<int> plateCrop;
    juce::Image plateScaled, plateOnScaled;

    // dirty-region repaint bookkeeping (plate mode)
    std::array<double, 19> shownKnobVals {};
    std::array<float, 4>   grFrac {}, shownGrFrac {};   // gate/ess/comp GR + clip
    std::array<float, 2>   ioFrac {}, shownIoFrac {};   // in/out columns

    VocalRackProcessor& proc;
    RackLookAndFeel lnf;

    // ---- header ----
    // ComboBox that rescans the user preset folder every time it opens.
    struct PresetBox : juce::ComboBox
    {
        std::function<void()> onBeforePopup;
        void showPopup() override
        {
            if (onBeforePopup) onBeforePopup();
            juce::ComboBox::showPopup();
        }
    };

    juce::Rectangle<int> brandArea;
    PresetBox presetBox;
    IconButton prevBtn { IconButton::Icon::Prev };
    IconButton nextBtn { IconButton::Icon::Next };
    IconButton saveBtn { IconButton::Icon::Save };
    juce::Rectangle<int> presetPill;
    juce::Array<juce::File> userPresetFiles;
    int shownProgram = -1;
    static constexpr int kUserPresetBaseId = 1000;
    static constexpr int kImportItemId = 900, kExportItemId = 901;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::String shownPresetText;

    // ---- knobs + value labels (order matters only within each module) ----
    RackKnob gateThreshK, gateRelK,
             essAmtK, essFreqK,
             eqHpfK, eqMudK, eqPresK, eqAirK,
             compAmtK,
             heatDriveK, heatToneK,
             airAmtK,
             dlySendK, dlyTimeK, dlyFbK,
             verbSendK, verbSizeK,
             clipAmtK,
             outK;
    std::unique_ptr<SliderAtt> gateThreshAtt, gateRelAtt,
                               essAmtAtt, essFreqAtt,
                               eqHpfAtt, eqMudAtt, eqPresAtt, eqAirAtt,
                               compAmtAtt,
                               heatDriveAtt, heatToneAtt,
                               airAmtAtt,
                               dlySendAtt, dlyTimeAtt, dlyFbAtt,
                               verbSendAtt, verbSizeAtt,
                               clipAmtAtt,
                               outAtt;
    juce::Label gateThreshV, gateRelV,
                essAmtV, essFreqV,
                eqHpfV, eqMudV, eqPresV, eqAirV,
                compAmtV,
                heatDriveV, heatToneV,
                airAmtV,
                dlySendV, dlyTimeV, dlyFbV,
                verbSendV, verbSizeV,
                clipAmtV,
                outV;

    // ---- module power toggles ----
    std::array<Bouncy<PowerButton>, 9> powers;
    std::array<std::unique_ptr<ButtonAtt>, 9> powerAtts;

    // ---- activity meters ----
    ActivityBar gateBar { 25.0f }, essBar { 8.0f }, compBar { 20.0f }, clipBar { 12.0f };

    // ---- output card ----
    VMeter inMeter, outMeter;
    juce::Rectangle<int> outCard, inMeterCapRect, outMeterCapRect;

    // ---- module row descriptors (geometry + child wiring for paint/layout) ----
    struct Module
    {
        juce::String name;
        std::vector<RackKnob*>     knobs;
        std::vector<juce::Label*>  values;
        std::vector<juce::String>  captions;
        ActivityBar*               meter = nullptr;
        juce::String               meterCaption;
        juce::Rectangle<int>       rect;
    };
    std::array<Module, 9> modules;
    std::array<bool, 9> shownOn { true, true, true, true, true, true, true, true, true };

    // Full-editor "enter your license key" overlay (shown until activated).
    ActivationOverlay licenseOverlay { proc.license, "VocalRack", "https://vocalessential.com",
                                       [] (float h, bool b) { return theme::font (h, b); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalRackEditor)
};
