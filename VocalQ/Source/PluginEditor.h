#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "licensing/LicenseGate.h"
#include "ui/Theme.h"
#include "ui/QLookAndFeel.h"
#include "ui/Knob.h"
#include "ui/EQDisplay.h"
#include "ui/Glyph.h"
#include "ui/Bounce.h"

//==============================================================================
// A band-selector tab: shows a number (1-6) or a filter glyph, pink when active.
//==============================================================================
class BandTab : public juce::Button
{
public:
    BandTab() : juce::Button ("tab") {}
    int  number = 0;   // 1..6 → show number, 0 → glyph
    int  type   = 0;   // BandType for glyph
    bool selected = false;
    bool bandOn = true;

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f);
        const float corner = 9.0f;
        if (selected)
        {
            // clean accent fill — no clipped shadow, no dot
            juce::ColourGradient ag (theme::accentHi, r.getX(), r.getY(),
                                     theme::accentLo, r.getX(), r.getBottom(), false);
            g.setGradientFill (ag);
            g.fillRoundedRectangle (r, corner);
            g.setColour (juce::Colours::white.withAlpha (0.30f));
            g.drawLine (r.getX() + corner, r.getY() + 1.2f, r.getRight() - corner, r.getY() + 1.2f, 1.2f);
            g.setColour (theme::accentLo.withAlpha (0.55f));
            g.drawRoundedRectangle (r, corner, 1.0f);
        }
        else
        {
            // clean white pill: subtle top-down sheen + crisp hairline, no shadow
            juce::ColourGradient wg (juce::Colours::white, r.getX(), r.getY(),
                                     juce::Colour (0xfff4f5f8), r.getX(), r.getBottom(), false);
            g.setGradientFill (wg);
            g.fillRoundedRectangle (r, corner);
            g.setColour (juce::Colours::white);
            g.drawLine (r.getX() + corner, r.getY() + 1.0f, r.getRight() - corner, r.getY() + 1.0f, 1.0f);
            g.setColour (over ? theme::accent.withAlpha (0.55f) : theme::cardLine);
            g.drawRoundedRectangle (r, corner, 1.2f);
        }
        const juce::Colour fg = selected ? juce::Colours::white
                                         : (bandOn ? theme::ink : theme::inkSoft);
        if (number > 0)
        {
            g.setColour (fg);
            g.setFont (theme::font (14.0f, true));
            g.drawText (juce::String (number), getLocalBounds(), juce::Justification::centred, false);
        }
        else
        {
            drawBandGlyph (g, (BandType) type, r.getCentre(), fg, 8.0f);
        }
    }
};

//==============================================================================
// Power (band enable) button — draws a power glyph, pink when on.
//==============================================================================
class PowerButton : public juce::Button
{
public:
    PowerButton() : juce::Button ("power") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics& g, bool, bool) override
    {
        auto r = getLocalBounds().toFloat();
        const bool on = getToggleState();
        const auto c = r.getCentre();
        const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.30f;

        const juce::Colour col = on ? theme::accent : theme::inkSoft;
        g.setColour (col);
        juce::Path ring;
        ring.addCentredArc (c.x, c.y, rad, rad, 0.0f,
                            juce::MathConstants<float>::pi * 0.25f,
                            juce::MathConstants<float>::pi * 1.75f, true);
        g.strokePath (ring, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.drawLine (c.x, c.y - rad - 1.0f, c.x, c.y + 1.0f, 2.2f);
    }
};

//==============================================================================
// Filter-type selector button (glyph), radio-style.
//==============================================================================
class TypeBtn : public juce::Button
{
public:
    TypeBtn() : juce::Button ("type") {}
    int type = 0;
    bool selected = false;
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const float corner = 7.0f;
        if (selected)
        {
            juce::ColourGradient ag (theme::accentHi, r.getX(), r.getY(),
                                     theme::accentLo, r.getX(), r.getBottom(), false);
            g.setGradientFill (ag);
            g.fillRoundedRectangle (r, corner);
            g.setColour (juce::Colours::white.withAlpha (0.30f));
            g.drawLine (r.getX() + corner, r.getY() + 1.0f, r.getRight() - corner, r.getY() + 1.0f, 1.0f);
            g.setColour (theme::accentLo.withAlpha (0.55f));
            g.drawRoundedRectangle (r, corner, 1.0f);
        }
        else
        {
            juce::ColourGradient wg (juce::Colours::white, r.getX(), r.getY(),
                                     juce::Colour (0xfff4f5f8), r.getX(), r.getBottom(), false);
            g.setGradientFill (wg);
            g.fillRoundedRectangle (r, corner);
            g.setColour (over ? theme::accent.withAlpha (0.55f) : theme::cardLine);
            g.drawRoundedRectangle (r, corner, 1.2f);
        }
        drawBandGlyph (g, (BandType) type, r.getCentre(), selected ? juce::Colours::white : theme::inkSoft, 7.0f);
    }
};

//==============================================================================
class VocalQEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit VocalQEditor (VocalQProcessor&);
    ~VocalQEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void bindBand (int band);
    void selectBand (int band);
    void nudgeParam (const juce::String& id, float delta);

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    VocalQProcessor& proc;
    QLookAndFeel lnf;

    EQDisplay display;

    std::array<Bouncy<BandTab>, VocalQProcessor::kNumBands> tabs;

    Bouncy<juce::TextButton> outMinus, outPlus;
    juce::Label outName, outVal;

    Bouncy<juce::TextButton> presetPrev, presetNext;
    juce::Label presetName, titleLabel, subLabel;

    // bottom panel — bind to selected band
    Bouncy<PowerButton> powerBtn;
    std::array<Bouncy<juce::TextButton>, 3> chanBtns;   // Stereo / Mid / Side
    std::array<Bouncy<TypeBtn>, 6> typeBtns;            // 6 filter types
    Knob freqK, qK, gainK, rangeK, threshK, atkK, relK;
    Bouncy<juce::TextButton> soloBtn;

    std::unique_ptr<SA> aFreq, aQ, aGain, aRange, aThresh, aAtk, aRel;
    std::unique_ptr<BA> aPower;

    int selectedBand = 5;

    juce::Rectangle<int> panelArea, displayArea;
    int divX1 = 0, divX2 = 0;

    std::unique_ptr<licensing::LicenseGate> licenseGate;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalQEditor)
};
