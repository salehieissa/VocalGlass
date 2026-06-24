#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "licensing/LicenseGate.h"
#include "ui/Theme.h"
#include "ui/Bounce.h"
#include "ui/KnobLookAndFeel.h"
#include "ui/Knob.h"
#include <array>
#include <memory>

//==============================================================================
// Small flat icon button (link / 3-dot) drawn with the soft-ink palette.
//==============================================================================
class IconButton : public juce::Button
{
public:
    enum Kind { Link, Dots };
    explicit IconButton (Kind k) : juce::Button ({}), kind (k) {}

    void paintButton (juce::Graphics& g, bool highlighted, bool) override
    {
        auto r = getLocalBounds().toFloat();
        const auto col = (highlighted ? theme::accent : theme::inkSoft);

        if (kind == Link)
        {
            // hairline pill background
            g.setColour (juce::Colours::white);
            g.fillRoundedRectangle (r.reduced (1.0f), 9.0f);
            g.setColour (theme::cardLine);
            g.drawRoundedRectangle (r.reduced (1.0f), 9.0f, 1.2f);

            // two crossing capsule links
            const auto c = r.getCentre();
            g.setColour (col);
            juce::PathStrokeType st (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
            for (float dir : { -1.0f, 1.0f })
            {
                juce::Path link;
                link.addRoundedRectangle (-7.5f, -3.5f, 11.0f, 7.0f, 3.5f);
                g.strokePath (link, st, juce::AffineTransform::rotation (juce::degreesToRadians (40.0f))
                                            .translated (c.x + dir * 3.0f, c.y));
            }
        }
        else // Dots
        {
            const auto c = r.getCentre();
            g.setColour (col);
            for (int i = -1; i <= 1; ++i)
                g.fillEllipse (c.x - 1.7f, c.y + (float) i * 6.0f - 1.7f, 3.4f, 3.4f);
        }
    }

private:
    Kind kind;
};

//==============================================================================
class VocalVerbEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit VocalVerbEditor (VocalVerbProcessor&);
    ~VocalVerbEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void stepProgram (int delta);

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    Knob& attach (Knob& k, const char* id);

    VocalVerbProcessor& proc;
    KnobLookAndFeel laf;

    // branding
    juce::Label brand, brandSub;

    // top-right controls
    Bouncy<juce::TextButton> bypassBtn { "Bypass" };
    IconButton linkBtn { IconButton::Link };
    IconButton dotsBtn { IconButton::Dots };
    std::unique_ptr<ButtonAtt> bypassAtt;

    // knobs
    Knob mixKnob   { "mix" },        predelayKnob { "predelay" };
    Knob decayKnob { "decay", true };
    Knob hiFreqKnob { "high freq" }, hiShelfKnob { "high shelf" };
    Knob bassFreqKnob { "bass freq" }, bassMultKnob { "bass mult" };
    Knob sizeKnob  { "size" },       attackKnob { "attack" };
    Knob earlyKnob { "early" },      lateKnob { "late" };
    Knob rateKnob  { "rate" },       depthKnob { "depth" };
    Knob hiCutKnob { "high cut" },   loCutKnob { "low cut" };

    std::array<std::unique_ptr<SliderAtt>, 15> knobAtts;
    int attIndex = 0;

    // host-tempo sync controls (mod rate + predelay)
    Bouncy<juce::TextButton> modSyncBtn { "sync" }, preSyncBtn { "sync" };
    juce::ComboBox modDivBox, preDivBox;
    std::unique_ptr<ButtonAtt> modSyncAtt, preSyncAtt;
    std::unique_ptr<ComboAtt>  modDivAtt, preDivAtt;

    // bottom bar
    juce::Label modeLabel, colorLabel, presetLabel;
    juce::ComboBox modeBox, colorBox, presetBox;
    std::unique_ptr<ComboAtt> modeAtt, colorAtt;
    Bouncy<juce::TextButton> prevBtn { "<" }, nextBtn { ">" };

    // painted regions
    juce::Rectangle<int> leftCard, dampCard, shapeCard, diffCard, modCard, eqCard;
    juce::Rectangle<int> bottomBar, arrowBox;
    int leftDividerY = 0;

    std::unique_ptr<licensing::LicenseGate> licenseGate;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalVerbEditor)
};
