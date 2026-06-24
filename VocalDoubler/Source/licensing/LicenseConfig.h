#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
// One place to configure licensing for the whole family. Copy this file into
// each plugin's Source/licensing/ folder (it's identical across plugins).
//
// EDIT THESE THREE THINGS once your Keygen account + Shopify pages are ready:
//==============================================================================
namespace licensing
{
    // Your Keygen account ID (Keygen dashboard -> Settings -> shown in the URL/account).
    // While this is left as the placeholder, plugins fall back to OFFLINE key
    // validation so you can test the activation flow right now without a server.
    inline const juce::String kKeygenAccountId { "YOUR_KEYGEN_ACCOUNT_ID" };

    // The "Buy a license" button target. Point at your Shopify store (or the
    // specific product page).
    inline const juce::String kStoreUrl { "https://your-store.myshopify.com" };

    // True once you've pasted a real Keygen account ID above.
    inline bool keygenConfigured()
    {
        return kKeygenAccountId.isNotEmpty()
            && kKeygenAccountId != "YOUR_KEYGEN_ACCOUNT_ID";
    }
}
