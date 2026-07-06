#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "../../../common/ui/Skin.h"

//==============================================================================
// ToggleSwitch — a rounded iOS-style switch. Track turns hot pink when on and
// the white thumb slides across. Stays a juce::Button so an APVTS
// ButtonAttachment can drive it.
//
// If the photoreal skin is present (toggle-off/on) a chrome bat-handle switch
// is drawn instead (off + on states cross-faded), otherwise the vector pill.
//==============================================================================
class ToggleSwitch : public juce::Button,
                     private juce::Timer
{
public:
    ToggleSwitch() : juce::Button ("toggle")
    {
        setClickingTogglesState (true);
        // Prefer the mockup's chrome bat-lever art (lever left = off, right = on);
        // fall back to the horizontal chrome pill if the bat art isn't present.
        offImg = skin::image ("bat-off@2x.png");
        onImg  = skin::image ("bat-on@2x.png");
        offName = "bat-off@2x.png"; onName = "bat-on@2x.png";
        if (! offImg.isValid() || ! onImg.isValid())
        {
            offImg = skin::image ("toggle-off@2x.png");
            onImg  = skin::image ("toggle-on@2x.png");
            offName = "toggle-off@2x.png"; onName = "toggle-on@2x.png";
        }
    }

    void paintButton (juce::Graphics& g, bool, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const float radius = r.getHeight() * 0.5f;

        // Photoreal chrome bat-handle. The off/on art has different opaque
        // bounds (the lever leans opposite ways), so stretching the trimmed
        // sprite would make the base jump between states. Instead anchor both
        // sprites by the measured centre of the round chrome base (frac of the
        // canvas) and scale off the base diameter, so the base stays planted
        // and only the lever moves. States snap — a real bat switch has no
        // in-between, so no cross-fade.
        if (offImg.isValid() && onImg.isValid())
        {
            const auto centre = getLocalBounds().toFloat().getCentre();
            // Bounds are deliberately generous so the lever + glow never clip;
            // the round base takes ~65% of the component height.
            const float baseDia = (float) getHeight() * 0.65f;
            const auto& img = getToggleState() ? onImg : offImg;
            // measured: base centre (0.502, 0.497), base dia 0.24 of canvas
            const float scale = baseDia / (0.24f * (float) img.getWidth());
            g.drawImageTransformed (img,
                juce::AffineTransform::translation (-0.502f * (float) img.getWidth(),
                                                    -0.497f * (float) img.getHeight())
                    .scaled (scale)
                    .translated (centre.x, centre.y));
            return;
        }

        // vector fallback animates its sliding thumb
        const float target = getToggleState() ? 1.0f : 0.0f;
        if (std::abs (target - pos) > 0.001f && ! isTimerRunning())
            startTimerHz (60);

        // track (with a hairline + soft inner shade so the OFF state still reads
        // on the light glass panel)
        g.setColour (getToggleState() ? theme::accent
                                      : theme::trackDeep.interpolatedWith (theme::accent, pos));
        g.fillRoundedRectangle (r, radius);
        if (pos < 0.5f)
        {
            g.setColour (juce::Colours::black.withAlpha (0.10f * (1.0f - pos)));
            g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
        }

        // thumb
        const float d = r.getHeight() - 6.0f;
        const float x = r.getX() + 3.0f + pos * (r.getWidth() - d - 6.0f);
        juce::Path thumb;
        thumb.addEllipse (x, r.getY() + 3.0f, d, d);
        juce::DropShadow (juce::Colours::black.withAlpha (0.18f), 5, { 0, 1 }).drawForPath (g, thumb);
        g.setColour (juce::Colours::white);
        g.fillPath (thumb);
    }

private:
    void timerCallback() override
    {
        const float target = getToggleState() ? 1.0f : 0.0f;
        pos += (target - pos) * 0.30f;
        if (std::abs (target - pos) < 0.005f) { pos = target; stopTimer(); }
        repaint();
    }

    float pos = 0.0f;
    juce::Image offImg, onImg;
    juce::String offName, onName;
};
