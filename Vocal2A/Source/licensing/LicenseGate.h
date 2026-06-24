#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../ui/Theme.h"
#include "LicenseManager.h"
#include "LicenseConfig.h"

//==============================================================================
// A full-window activation overlay that attaches itself to the plugin editor
// and auto-resizes with it. Shown until the plugin is activated; blocks all
// interaction with the GUI behind it so the plugin is genuinely gated.
//
// Integration is three lines in any editor (see PluginEditor.cpp):
//   1) #include "licensing/LicenseGate.h"
//   2) std::unique_ptr<licensing::LicenseGate> licenseGate;   // member
//   3) licenseGate = std::make_unique<licensing::LicenseGate>(*this, "VocalGrit", "VOCALGRIT");
//
// The constructor wires the verifier automatically: Keygen when configured in
// LicenseConfig.h, otherwise an offline checksum so you can test the flow now.
//==============================================================================
namespace licensing
{
    class LicenseGate : public juce::Component,
                        private juce::ComponentListener
    {
    public:
        // productName: shown in the title. entitlement: this plugin's Keygen
        // entitlement code (e.g. "VOCALGRIT"); also used as the offline productId.
        LicenseGate (juce::Component& parentEditor,
                     juce::String productName,
                     juce::String entitlement)
            : parent (parentEditor),
              displayName (std::move (productName)),
              productCode (std::move (entitlement))
        {
            // Pick the verifier: real Keygen if configured, else offline test mode.
            if (keygenConfigured())
                LicenseManager::getInstance().setVerifier (
                    makeKeygenVerifier (kKeygenAccountId, productCode));

            setInterceptsMouseClicks (true, true);

            keyField.setFont (theme::font (18.0f, false));
            keyField.setJustification (juce::Justification::centred);
            keyField.setTextToShowWhenEmpty ("XXXX-XXXX-XXXX-XXXX", theme::inkFaint);
            keyField.setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
            keyField.setColour (juce::TextEditor::outlineColourId, theme::cardLine);
            keyField.setColour (juce::TextEditor::focusedOutlineColourId, theme::accent);
            keyField.setColour (juce::TextEditor::textColourId, theme::ink);
            keyField.onReturnKey = [this] { tryActivate(); };
            addAndMakeVisible (keyField);

            activateBtn.setButtonText ("Activate");
            activateBtn.setColour (juce::TextButton::buttonColourId, theme::accent);
            activateBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
            activateBtn.onClick = [this] { tryActivate(); };
            addAndMakeVisible (activateBtn);

            status.setJustificationType (juce::Justification::centred);
            status.setFont (theme::font (13.0f, false));
            status.setColour (juce::Label::textColourId, theme::inkSoft);
            addAndMakeVisible (status);

            buyLink.setButtonText ("Don't have a key? Buy a license");
            buyLink.setColour (juce::HyperlinkButton::textColourId, theme::accent);
            buyLink.setFont (theme::font (13.0f, false), false);
            buyLink.setURL (juce::URL (kStoreUrl));
            addAndMakeVisible (buyLink);

            // Attach to the editor and track its size.
            parent.addChildComponent (this);
            parent.addComponentListener (this);
            setBounds (parent.getLocalBounds());
            setVisible (! LicenseManager::getInstance().isActivated());
            toFront (false);
        }

        ~LicenseGate() override
        {
            parent.removeComponentListener (this);
        }

        std::function<void()> onActivated;

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colours::black.withAlpha (0.55f));

            theme::elevate (g, card.toFloat(), 18.0f, 1.4f);
            g.setColour (theme::card);
            g.fillRoundedRectangle (card.toFloat(), 18.0f);
            theme::topHighlight (g, card.toFloat(), 18.0f);

            auto inner = card.reduced (28, 24);

            theme::spacedText (g, "ACTIVATE " + displayName.toUpperCase(),
                               inner.removeFromTop (26).toFloat(), theme::ink,
                               17.0f, 2.0f, true, juce::Justification::centred);

            inner.removeFromTop (6);
            g.setColour (theme::inkSoft);
            g.setFont (theme::font (13.5f, false));
            g.drawFittedText ("Enter the license key from your order email to unlock all features.",
                              inner.removeFromTop (38), juce::Justification::centredTop, 2);
        }

        void resized() override
        {
            const int cw = juce::jmin (460, getWidth()  - 48);
            const int ch = 300;
            card = { (getWidth() - cw) / 2, (getHeight() - ch) / 2, cw, ch };

            auto inner = card.reduced (28, 24);
            inner.removeFromTop (26 + 6 + 38 + 18);

            keyField.setBounds (inner.removeFromTop (44));
            inner.removeFromTop (14);
            activateBtn.setBounds (inner.removeFromTop (44).reduced (inner.getWidth() / 2 - 90, 0));
            inner.removeFromTop (10);
            status.setBounds (inner.removeFromTop (20));
            inner.removeFromTop (6);
            buyLink.setBounds (inner.removeFromTop (22));
        }

    private:
        void componentMovedOrResized (juce::Component& c, bool, bool) override
        {
            if (&c == &parent)
            {
                setBounds (parent.getLocalBounds());
                toFront (false);
            }
        }

        void tryActivate()
        {
            auto res = LicenseManager::getInstance().activate (keyField.getText(), productCode);
            status.setColour (juce::Label::textColourId,
                              res.ok ? theme::accentLo : juce::Colour (0xffc0392b));
            status.setText (res.message, juce::dontSendNotification);

            if (res.ok)
            {
                if (onActivated)
                    onActivated();
                setVisible (false);
            }
        }

        juce::Component& parent;

        juce::TextEditor      keyField;
        juce::TextButton      activateBtn;
        juce::Label           status;
        juce::HyperlinkButton buyLink;

        juce::String displayName { "Plugin" }, productCode;
        juce::Rectangle<int>  card;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LicenseGate)
    };
}
