#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
// One place to configure licensing for the whole family. This file is identical
// in every plugin (it's copied into each Source/licensing/ folder).
//
// TO GO LIVE, edit the three marked sections below:
//   1) kKeygenAccountId  -> your Keygen account id
//   2) the products() table -> each plugin's Shopify buy URL
//   3) kSuiteUrl -> the suite product page
//==============================================================================
namespace licensing
{
    //========================================================================
    // 1) KEYGEN ACCOUNT
    //    Found in the Keygen dashboard -> Settings (a UUID), also visible in the
    //    dashboard URL. This is PUBLIC and safe to ship — it only identifies the
    //    account for the unauthenticated validate-key call. NEVER put your secret
    //    Keygen admin token in here.
    //
    //    While this is left as the placeholder, plugins fall back to OFFLINE key
    //    validation so the activation flow still works for local testing.
    //========================================================================
    inline const juce::String kKeygenAccountId { "YOUR_KEYGEN_ACCOUNT_ID" };

    inline bool keygenConfigured()
    {
        return kKeygenAccountId.isNotEmpty()
            && kKeygenAccountId != "YOUR_KEYGEN_ACCOUNT_ID";
    }

    //========================================================================
    // 2) PER-PRODUCT WIRING
    //    Each plugin validates a key SCOPED to its entitlement `code`, so these
    //    codes must match the entitlement codes you created in Keygen exactly.
    //    `buyUrl` is the Shopify page the "Buy a license" button opens for that
    //    plugin. The same `code` strings are passed to LicenseGate in each editor.
    //========================================================================
    struct ProductInfo
    {
        const char* code;    // Keygen entitlement code (must match Keygen)
        const char* buyUrl;  // Shopify product page for this plugin
    };

    inline juce::Array<ProductInfo> products()
    {
        return {
            { "VOCAL2A",      "https://your-store.myshopify.com/products/vocal2a"      },
            { "VOCALAIR",     "https://your-store.myshopify.com/products/vocalair"     },
            { "VOCALCOMP",    "https://your-store.myshopify.com/products/vocalcomp"    },
            { "VOCALDELAY",   "https://your-store.myshopify.com/products/vocaldelay"   },
            { "VOCALDOUBLER", "https://your-store.myshopify.com/products/vocaldoubler" },
            { "VOCALESS",     "https://your-store.myshopify.com/products/vocaless"     },
            { "VOCALGRIT",    "https://your-store.myshopify.com/products/vocalgrit"    },
            { "VOCALKNOB",    "https://your-store.myshopify.com/products/vocalknob"    },
            { "VOCALQ",       "https://your-store.myshopify.com/products/vocalq"       },
            { "VOCALTUNE",    "https://your-store.myshopify.com/products/vocaltune"    },
            { "VOCALVERB",    "https://your-store.myshopify.com/products/vocalverb"    },
        };
    }

    //========================================================================
    // 3) SUITE
    //    The full-suite license carries this entitlement. When a suite key is
    //    entered into ANY plugin, every plugin unlocks at once (see LicenseManager).
    //    In Keygen, give the Suite policy/license an entitlement with this code.
    //    kSuiteUrl is the buy page used as the fallback "Buy a license" target.
    //========================================================================
    inline const juce::String kSuiteEntitlement { "VOCALSUITE" };
    inline const juce::String kSuiteUrl { "https://your-store.myshopify.com/products/vocalglass-suite" };

    //========================================================================
    // Helpers (no need to edit below).
    //========================================================================

    // Every per-plugin entitlement code — used to unlock the whole family when a
    // suite key is validated.
    inline juce::StringArray allProducts()
    {
        juce::StringArray codes;
        for (const auto& p : products())
            codes.add (p.code);
        return codes;
    }

    // The Shopify page the "Buy a license" button should open for a given plugin.
    // Falls back to the suite page if the code isn't found.
    inline juce::String buyUrlFor (const juce::String& code)
    {
        for (const auto& p : products())
            if (code == p.code)
                return juce::String (p.buyUrl);
        return kSuiteUrl;
    }
}
