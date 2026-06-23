#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <utility>
#include <cmath>

//==============================================================================
// Bouncy<ButtonType> — quick press animation: scales down on mouse-down and
// springs back. Stays a juce::Button so APVTS attachments still work.
//==============================================================================
template <typename ButtonType>
class Bouncy : public ButtonType,
               private juce::Timer
{
public:
    template <typename... Args>
    explicit Bouncy (Args&&... args) : ButtonType (std::forward<Args> (args)...) {}

    void mouseDown (const juce::MouseEvent& e) override
    {
        scale = 0.88f;
        applyScale();
        startTimerHz (60);
        ButtonType::mouseDown (e);
    }

private:
    void timerCallback() override
    {
        scale += (1.0f - scale) * 0.30f;
        if (std::abs (1.0f - scale) < 0.004f) { scale = 1.0f; stopTimer(); }
        applyScale();
    }

    void applyScale()
    {
        const auto b = this->getBounds().toFloat();
        this->setTransform (juce::AffineTransform::scale (scale, scale,
                                                          b.getCentreX(), b.getCentreY()));
    }

    float scale = 1.0f;
};
