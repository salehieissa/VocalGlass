#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "Bounce.h"
#include <functional>

//==============================================================================
// A flat icon button: invokes a user-supplied drawing function with the
// current colour (which lightens to the accent on hover).
//==============================================================================
class IconButton : public Bouncy<juce::Button>
{
public:
    using Drawer = std::function<void (juce::Graphics&, juce::Rectangle<float>, juce::Colour)>;

    IconButton() : Bouncy<juce::Button> (juce::String()) {}

    void setDrawer (Drawer d) { drawer = std::move (d); repaint(); }
    void setBaseColour (juce::Colour c) { base = c; repaint(); }

    void paintButton (juce::Graphics& g, bool highlighted, bool) override
    {
        if (! drawer) return;
        const auto c = (highlighted && isEnabled()) ? theme::accent : base;
        drawer (g, getLocalBounds().toFloat(), c.withAlpha (isEnabled() ? 1.0f : 0.4f));
    }

private:
    Drawer drawer;
    juce::Colour base { theme::inkSoft };
};
