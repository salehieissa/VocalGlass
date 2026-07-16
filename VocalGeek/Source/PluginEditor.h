#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/Bounce.h"
#include "ui/KnobLookAndFeel.h"
#include "ui/LabeledKnob.h"
#include "ui/GeekDisplay.h"
#include "../../common/Licensing/ActivationOverlay.h"
#include "../../common/ui/Skin.h"
#include <array>
#include <memory>

//==============================================================================
// A momentary candy-dome pad: draws its theme-tinted dome sprite and holds a
// bool parameter true while pressed. The lit halo underneath is blitted from
// the ON plate by the editor.
class HitPad : public juce::Component
{
public:
    explicit HitPad (juce::RangedAudioParameter* param = nullptr) : par (param) {}

    void setParam (juce::RangedAudioParameter* param) { par = param; }
    void setSprite (const juce::Image& img) { sprite = img; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        if (down) r = r.reduced (r.getWidth() * 0.02f);
        if (sprite.isValid())
        {
            skin::drawInRect (g, sprite, skin::containRect (sprite, r));
            return;
        }
        // vector fallback: glossy accent dome
        g.setColour (down ? theme::accentLo : theme::accent);
        g.fillEllipse (r.reduced (4.0f));
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillEllipse (r.reduced (r.getWidth() * 0.22f).translated (0.0f, -r.getHeight() * 0.12f));
    }

    void mouseDown (const juce::MouseEvent&) override { down = true;  push (true);  repaint(); }
    void mouseUp   (const juce::MouseEvent&) override { down = false; push (false); repaint(); }

    bool isDown() const noexcept { return down; }

private:
    void push (bool state)
    {
        if (par == nullptr) return;
        par->beginChangeGesture();
        par->setValueNotifyingHost (state ? 1.0f : 0.0f);
        par->endChangeGesture();
    }

    juce::RangedAudioParameter* par = nullptr;
    juce::Image sprite;
    bool down = false;
};

//==============================================================================
// The chrome D-pad. Four hit zones nudge texture (up/down) and space
// (left/right) in 10% steps. Draws the extracted sprite; presses dip it.
class DPad : public juce::Component
{
public:
    std::function<void (int dx, int dy)> onNudge;

    void setSprite (const juce::Image& img) { sprite = img; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        if (down) r = r.reduced (r.getWidth() * 0.012f);
        if (sprite.isValid())
        {
            skin::drawInRect (g, sprite, skin::containRect (sprite, r));
            return;
        }
        // vector fallback: flat chrome cross
        const float arm = r.getWidth() / 3.0f;
        juce::Path cross;
        cross.addRoundedRectangle (r.getCentreX() - arm * 0.5f, r.getY(), arm, r.getHeight(), 8.0f);
        cross.addRoundedRectangle (r.getX(), r.getCentreY() - arm * 0.5f, r.getWidth(), arm, 8.0f);
        g.setColour (theme::capLo);
        g.fillPath (cross);
        g.setColour (theme::cardLine);
        g.strokePath (cross, juce::PathStrokeType (1.2f));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        down = true;
        const auto c = getLocalBounds().getCentre().toFloat();
        const float dx = (float) e.x - c.x, dy = (float) e.y - c.y;
        if (onNudge != nullptr)
        {
            if (std::abs (dx) > std::abs (dy)) onNudge (dx > 0 ? 1 : -1, 0);
            else                               onNudge (0, dy > 0 ? 1 : -1);
        }
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override { down = false; repaint(); }

private:
    juce::Image sprite;
    bool down = false;
};

//==============================================================================
// The cartridge bay. Shows the loaded dose (theme content sprite, overflow
// included); clicking it ejects the current cartridge and slots the next one.
class CartridgeBay : public juce::Component
{
public:
    std::function<void()> onCycle;

    void setSprite (const juce::Image& img) { sprite = img; repaint(); }

    void paint (juce::Graphics& g) override
    {
        if (sprite.isValid())
        {
            skin::drawInRect (g, sprite, getLocalBounds().toFloat());
            return;
        }
        // vector fallback: recessed pill slot
        auto r = getLocalBounds().toFloat().reduced (4.0f);
        theme::recess (g, r, r.getHeight() * 0.5f);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (getLocalBounds().contains (e.getPosition()) && onCycle != nullptr)
            onCycle();
    }

    juce::Image sprite;
};

//==============================================================================
class VocalGeekEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit VocalGeekEditor (VocalGeekProcessor&);
    ~VocalGeekEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void applyTheme (int themeIndex);
    void cycleTheme();
    void tapClicked();
    void togglePrint();

    // ---- baked-plate (photoreal skin) mode ----
    void setupPlateMode();
    void paintPlate (juce::Graphics&);
    void layoutPlate();
    juce::Rectangle<int> platePxRect (float x0, float y0, float x1, float y1) const;
    void maskFromOn (juce::Graphics&, juce::Rectangle<int> rect);
    void maskFromOnFeathered (juce::Graphics&, juce::Rectangle<int> rect, int featherPx);
    void drawRingWedge (juce::Graphics&, juce::Slider&, float cxPx, float cyPx,
                        float domeRPx, float solidRPx, float maxRPx);

    juce::Image chassisImg, chassisOnImg;
    bool plateBaked = false;

    juce::Rectangle<int> plateCrop;
    juce::Image plateScaled, plateOnScaled;

    // dirty-region repaint bookkeeping
    std::array<double, 2> shownKnob { -1.0, -1.0 };
    bool shownPrint = false, shownRec = false, shownHitA = false, shownHitB = false;
    int shownMeter = -1;
    juce::int64 tapFlashUntil = 0;
    bool shownTapLit = false;

    VocalGeekProcessor& proc;
    KnobLookAndFeel laf;

    int currentTheme = -1;

    juce::Label brand;

    GeekDisplay display { proc.engine };

    LabeledKnob doseKnob   { "dose" };
    LabeledKnob outputKnob { "output" };
    std::array<std::unique_ptr<SliderAtt>, 2> knobAtt;

    HitPad hitA { nullptr }, hitB { nullptr };
    DPad dpad;
    CartridgeBay bay;
    Bouncy<juce::TextButton> tapBtn { "TAP" }, printBtn { "PRINT" };

    ActivationOverlay licenseOverlay { proc.license, "VocalGeek", "https://vocalessential.com",
                                       [] (float h, bool b) { return theme::font (h, b); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalGeekEditor)
};
