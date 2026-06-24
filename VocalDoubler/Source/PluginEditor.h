#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/Bounce.h"
#include "ui/KnobLookAndFeel.h"
#include "ui/DoubleDisplay.h"
#include "ui/ValueReadout.h"
#include "ui/EffectOnlyButton.h"
#include "ui/TopBarButton.h"
#include "ui/ModRateControl.h"
#include "../../common/Licensing/ActivationOverlay.h"

//==============================================================================
class VocalDoublerEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit VocalDoublerEditor (VocalDoublerProcessor&);
    ~VocalDoublerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setParam (const char* id, float value0to100);
    void showMenu();
    void showTips();

    VocalDoublerProcessor& proc;
    KnobLookAndFeel laf;

    // branding
    juce::Label brand, brandSub;

    // top-right controls
    Bouncy<TopBarButton> tipsBtn   { "Vocal Tips", TopBarButton::Kind::ExternalLink };
    Bouncy<TopBarButton> bypassBtn { "Bypass",     TopBarButton::Kind::Plain };
    Bouncy<TopBarButton> menuBtn   { "",           TopBarButton::Kind::Menu };

    // centre interactive display
    DoubleDisplay display;

    // left readouts
    ValueReadout separationRO { "Separation" };
    ValueReadout variationRO  { "Variation" };

    // right controls
    Bouncy<EffectOnlyButton> effectOnly;
    juce::Slider amountKnob;
    juce::Label amountLabel;

    // modulation rate (with host-tempo sync)
    ModRateControl modRate;
    void cycleModDivision();

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> effectOnlyAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> modRateAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> modSyncAtt;

    juce::Rectangle<int> cardArea;

    // Full-editor "enter your license key" overlay (shown until activated).
    ActivationOverlay licenseOverlay { proc.license, "VocalDoubler", "https://vocalessential.com",
                                       [] (float h, bool b) { return theme::font (h, b); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalDoublerEditor)
};
