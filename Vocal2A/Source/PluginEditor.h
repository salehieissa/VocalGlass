#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/Bounce.h"
#include "ui/KnobLookAndFeel.h"
#include "ui/VintageKnob.h"
#include "ui/ToggleSwitch.h"
#include "ui/VUMeter.h"
#include "../../common/ui/Skin.h"
#include "../../common/Licensing/ActivationOverlay.h"
#include <array>
#include <memory>

//==============================================================================
class Vocal2AEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit Vocal2AEditor (Vocal2AProcessor&);
    ~Vocal2AEditor() override;

    void paint (juce::Graphics&) override;
    void paintPlate (juce::Graphics&);   // baked off-plate + masked on-plate path
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void setChoice (const juce::String& paramID, int index);

    Vocal2AProcessor& proc;
    KnobLookAndFeel laf;

    // branding
    juce::Label brand, brandSub;

    // VU source selector
    juce::Label vuTitle;
    std::array<Bouncy<juce::TextButton>, 3> vuButtons; // input / GR / output

    // main controls
    VintageKnob gainKnob { VintageKnob::Style::Large };
    VintageKnob peakKnob { VintageKnob::Style::Large };
    juce::Label gainCap, peakCap;   // static labels (baked; hidden under chassis)
    juce::Label gainVal, peakVal;   // live numbers above the big knobs

    ToggleSwitch modeSwitch;   // compress / limit
    ToggleSwitch autoSwitch;   // auto makeup
    juce::Label  modeCapL, modeCapR, autoCap;   // flank the bat-lever toggles

    VUMeter vu;

    // bottom strip
    juce::Label analogLabel;
    std::array<Bouncy<juce::TextButton>, 3> analogButtons; // 50Hz / 60Hz / off

    juce::Label  hiFreqLabel, mixLabel, trimLabel, attackLabel, releaseLabel;
    juce::Label  hiFreqCap, mixCap, trimCap, attackCap, releaseCap;
    VintageKnob  hiFreqKnob  { VintageKnob::Style::Small };
    VintageKnob  mixKnob     { VintageKnob::Style::Small };
    VintageKnob  trimKnob    { VintageKnob::Style::Small };
    VintageKnob  attackKnob  { VintageKnob::Style::Small };
    VintageKnob  releaseKnob { VintageKnob::Style::Small };

    std::unique_ptr<SliderAtt> gainAtt, peakAtt, hiFreqAtt, mixAtt, trimAtt;
    std::unique_ptr<SliderAtt> attackAtt, releaseAtt;
    std::unique_ptr<ButtonAtt> autoAtt;

    // layout rects
    juce::Rectangle<int> cardArea, vuCard, vuTitleArea, bottomStrip, analogPill, trayArea;
    juce::Rectangle<int> brandBounds, brandSubBounds, ledBounds, trayDivider;

    // ---- baked-plate helpers (map opaque-plate fractions <-> screen) ----
    juce::Rectangle<float> plateRect() const { return getLocalBounds().toFloat(); }
    juce::Point<float>     plateXY (float fx, float fy) const
    {
        auto p = plateRect();
        return { p.getX() + fx * p.getWidth(), p.getY() + fy * p.getHeight() };
    }
    juce::Rectangle<int>   plateFracRect (float fx, float fy, float fw, float fh) const
    {
        auto p = plateRect();
        return juce::Rectangle<float> (p.getX() + fx * p.getWidth(),
                                       p.getY() + fy * p.getHeight(),
                                       fw * p.getWidth(), fh * p.getHeight()).toNearestInt();
    }
    // Reveal the matching region of the lit plate over the base (pixel-aligned).
    void maskFromOn (juce::Graphics&, juce::Rectangle<int> screenRect);
    void drawKnobHalo (juce::Graphics&, VintageKnob&, float ringRfracW, bool full = false);

    // selector button rects (screen space), filled in resized()
    std::array<juce::Rectangle<int>, 3> vuBtnR, analogBtnR;

    // photoreal skin
    juce::Image chassisImg   { skin::image ("2a-chassis@2x.png") };    // baked static metal, off state
    juce::Image chassisOnImg { skin::image ("2a-chassis-on@2x.png") }; // baked lit state (masked in)
    juce::Image panelImg  { skin::image ("panel@2x.png") };      // deep full-bleed base (preferred)
    juce::Image faceImg   { skin::image ("faceplate@2x.png") };  // flat-card fallback
    juce::Image screwImg  { skin::image ("screw@2x.png") };
    juce::Image screenImg { skin::image ("screen@2x.png") };
    juce::Image ledOffImg { skin::image ("led-red-off@2x.png") };
    juce::Image ledOnImg  { skin::image ("led-red-on@2x.png") };
    void drawScrews (juce::Graphics&, juce::Rectangle<float> panel, float inset, float size);

    // Full-editor "enter your license key" overlay (shown until activated).
    ActivationOverlay licenseOverlay { proc.license, "Vocal2A", "https://vocalessential.com",
                                       [] (float h, bool b) { return theme::font (h, b); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Vocal2AEditor)
};
