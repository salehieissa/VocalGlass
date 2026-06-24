#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LicenseManager.h"
#include <functional>

//==============================================================================
// Full-editor overlay shown while a plugin is NOT activated (hard lock).
// Add it as a top child of your editor, size it to the editor bounds, and it
// shows/hides itself from the LicenseManager status. Self-styled (light
// neumorphic, pink accent, SF Pro Display) so all 11 plugins share one look.
//==============================================================================
class ActivationOverlay : public juce::Component
{
public:
    // fontProvider lets the host plugin supply its UI font (e.g. a bundled
    // typeface on Windows). When null, falls back to the "SF Pro Display" system
    // font name (correct on macOS). Signature: (height, bold) -> Font.
    ActivationOverlay (LicenseManager& lm, juce::String pluginDisplayName,
                       juce::String storeUrl = "https://vocalessential.com",
                       std::function<juce::Font (float, bool)> fontProvider = {})
        : license (lm), pluginName (std::move (pluginDisplayName)), store (std::move (storeUrl)),
          fontFn (std::move (fontProvider))
    {
        lnf.fontFn = fontFn;
        setLookAndFeel (&lnf);

        title.setText (pluginName, juce::dontSendNotification);
        title.setJustificationType (juce::Justification::centred);
        title.setFont (sf (26.0f, true));
        title.setColour (juce::Label::textColourId, ink);
        addAndMakeVisible (title);

        subtitle.setText ("Enter your license key to activate", juce::dontSendNotification);
        subtitle.setJustificationType (juce::Justification::centred);
        subtitle.setFont (sf (14.0f));
        subtitle.setColour (juce::Label::textColourId, inkSoft);
        addAndMakeVisible (subtitle);

        keyBox.setJustification (juce::Justification::centred);
        keyBox.setFont (sf (16.0f));
        keyBox.setTextToShowWhenEmpty ("XXXXXX-XXXXXX-XXXXXX-XXXXXX", inkFaint);
        keyBox.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        keyBox.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        keyBox.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        keyBox.setColour (juce::TextEditor::textColourId, ink);
        keyBox.setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.22f));
        keyBox.setColour (juce::CaretComponent::caretColourId, accent);
        keyBox.onReturnKey = [this] { doActivate(); };
        addAndMakeVisible (keyBox);

        activateBtn.setButtonText ("Activate");
        activateBtn.getProperties().set ("primary", true);
        activateBtn.onClick = [this] { doActivate(); };
        addAndMakeVisible (activateBtn);

        buyBtn.setButtonText ("Buy a license  \u2192");
        buyBtn.onClick = [this] { juce::URL (store).launchInDefaultBrowser(); };
        addAndMakeVisible (buyBtn);

        status.setJustificationType (juce::Justification::centred);
        status.setFont (sf (13.0f));
        status.setColour (juce::Label::textColourId, inkSoft);
        addAndMakeVisible (status);

        license.onStatusChange = [this] { refresh(); };
        refresh();
    }

    ~ActivationOverlay() override
    {
        license.onStatusChange = nullptr;
        setLookAndFeel (nullptr);
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        // Translucent veil so the plugin stays visible behind the card, just dimmed.
        g.fillAll (juce::Colours::black.withAlpha (0.45f));

        auto card = getCardBounds().toFloat();

        // Soft drop shadow under the card.
        {
            juce::Path p; p.addRoundedRectangle (card, 18.0f);
            juce::DropShadow (juce::Colours::black.withAlpha (0.35f), 34, { 0, 12 })
                .drawForPath (g, p);
        }

        // Card body + hairline border + thin pink top accent.
        g.setColour (card_);
        g.fillRoundedRectangle (card, 18.0f);
        g.setColour (cardLine);
        g.drawRoundedRectangle (card.reduced (0.5f), 18.0f, 1.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (card.getCentreX() - 22.0f, card.getY() + 22.0f, 44.0f, 3.0f, 1.5f);

        // Recessed key field.
        auto f = keyFieldBounds.toFloat();
        g.setColour (track);
        g.fillRoundedRectangle (f, 9.0f);
        g.setColour (trackDeep);                            // top inner shade
        g.drawLine (f.getX() + 9, f.getY() + 1.0f, f.getRight() - 9, f.getY() + 1.0f, 1.2f);
        g.setColour (cardLine);
        g.drawRoundedRectangle (f.reduced (0.5f), 9.0f, 1.0f);
    }

    void resized() override
    {
        auto card = getCardBounds().reduced (30);
        card.removeFromTop (20);                  // room for the pink accent bar
        title      .setBounds (card.removeFromTop (32));
        card.removeFromTop (2);
        subtitle   .setBounds (card.removeFromTop (20));
        card.removeFromTop (22);
        keyFieldBounds = card.removeFromTop (46);
        keyBox     .setBounds (keyFieldBounds.reduced (12, 0));
        card.removeFromTop (14);
        activateBtn.setBounds (card.removeFromTop (46));
        card.removeFromTop (14);
        status     .setBounds (card.removeFromTop (20));
        card.removeFromTop (4);
        buyBtn     .setBounds (card.removeFromTop (26));
    }

private:
    //==========================================================================
    // Self-contained look & feel so the activation UI matches the plugins.
    struct OverlayLnF : juce::LookAndFeel_V4
    {
        void drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                   const juce::Colour&, bool over, bool down) override
        {
            if (! (bool) b.getProperties()["primary"])
                return;                                     // "Buy" is a text link

            auto r = b.getLocalBounds().toFloat().reduced (0.5f);
            const float rad = r.getHeight() * 0.5f;         // pill

            g.setColour (juce::Colour (0xffec0f8f).withAlpha (0.28f)); // glow
            g.fillRoundedRectangle (r.expanded (1.5f), rad);

            juce::ColourGradient grad (juce::Colour (0xfff64bad), r.getX(), r.getY(),
                                       juce::Colour (0xffcc0a7c), r.getX(), r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, rad);

            g.setColour (juce::Colours::white.withAlpha (over ? 0.16f : 0.10f));
            g.fillRoundedRectangle (r.withTrimmedBottom (r.getHeight() * 0.5f), rad);
            if (down) { g.setColour (juce::Colours::black.withAlpha (0.10f)); g.fillRoundedRectangle (r, rad); }
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool) override
        {
            const bool primary = (bool) b.getProperties()["primary"];
            const float h = primary ? 16.0f : 13.0f;
            g.setFont (fontFn ? fontFn (h, primary)
                              : juce::Font (juce::FontOptions ("SF Pro Display", h,
                                            primary ? juce::Font::bold : juce::Font::plain)));
            g.setColour (primary ? juce::Colours::white
                                 : (over ? juce::Colour (0xffec0f8f) : juce::Colour (0xff8b8b96)));
            g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
        }

        std::function<juce::Font (float, bool)> fontFn;
    };

    juce::Font sf (float h, bool bold = false) const
    {
        if (fontFn) return fontFn (h, bold);
        return juce::Font (juce::FontOptions ("SF Pro Display", h,
                                              bold ? juce::Font::bold : juce::Font::plain));
    }

    std::function<juce::Font (float, bool)> fontFn;

    juce::Rectangle<int> getCardBounds() const
    {
        const int w = juce::jmin (420, getWidth()  - 48);
        const int h = juce::jmin (320, getHeight() - 48);
        return { (getWidth() - w) / 2, (getHeight() - h) / 2, w, h };
    }

    void doActivate()
    {
        if (keyBox.getText().trim().isEmpty()) { setStatus ("Enter your license key.", true); return; }
        setStatus ("Activating\u2026", false);
        activateBtn.setEnabled (false);
        license.activate (keyBox.getText(), [this] (LicenseManager::Status s, juce::String msg)
        {
            activateBtn.setEnabled (true);
            setStatus (msg, s != LicenseManager::Status::Activated);
            refresh();
        });
    }

    void setStatus (const juce::String& msg, bool isError)
    {
        status.setColour (juce::Label::textColourId, isError ? juce::Colour (0xffd23b3b) : inkSoft);
        status.setText (msg, juce::dontSendNotification);
    }

    void refresh()
    {
        const bool activated = license.isActivated();
        setVisible (! activated);
        if (! activated)
        {
            if (keyBox.getText().isEmpty())
                keyBox.setText (license.getLicenseKey(), juce::dontSendNotification);
            const auto s = license.getStatus();
            const bool err = s == LicenseManager::Status::Invalid
                          || s == LicenseManager::Status::Expired
                          || s == LicenseManager::Status::TooManyMachines
                          || s == LicenseManager::Status::NetworkError;
            setStatus (license.getStatusMessage(), err);
            toFront (false);
        }
    }

    //==========================================================================
    // Theme colours (mirror the plugins' Theme.h).
    const juce::Colour card_     { 0xfffdfdff };
    const juce::Colour cardLine  { 0xffe2e3ea };
    const juce::Colour track     { 0xffe7e8ee };
    const juce::Colour trackDeep { 0xffd9dae2 };
    const juce::Colour accent    { 0xffec0f8f };
    const juce::Colour ink       { 0xff17171c };
    const juce::Colour inkSoft   { 0xff8b8b96 };
    const juce::Colour inkFaint  { 0xffb6b7c1 };

    LicenseManager& license;
    juce::String    pluginName, store;

    OverlayLnF       lnf;
    juce::Label      title, subtitle, status;
    juce::TextEditor keyBox;
    juce::TextButton activateBtn, buyBtn;
    juce::Rectangle<int> keyFieldBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ActivationOverlay)
};
