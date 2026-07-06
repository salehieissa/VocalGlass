#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
// VocalEssential licensing configuration (Keygen.sh).
//
// Fill these in after creating your Keygen account + products. The plugin-side
// engine (LicenseManager) reads everything from here so the 11 plugins share a
// single source of truth.
//==============================================================================
namespace licensing
{
    // --- Keygen account -------------------------------------------------------
    // Your Keygen account ID (Keygen dashboard → Settings). REQUIRED.
    inline constexpr const char* kKeygenAccountId = "salehieissa";

    // Keygen API base; the account id is appended at runtime.
    inline constexpr const char* kKeygenApiBase = "https://api.keygen.sh/v1/accounts/";

    // Your account's Ed25519 verify key (hex), used to verify signed responses /
    // cryptographic license keys. Leave empty to skip verification for now
    // (online validation still works; this is an anti-tamper hardening step).
    inline constexpr const char* kKeygenPublicKeyHex = "";

    // --- Policy / behaviour ---------------------------------------------------
    // Max machines per license (must match the Keygen policy; enforced server-side).
    inline constexpr int kMaxMachines = 2;

    // How long a cached, previously-valid activation keeps working with no
    // network before we force a re-check (perpetual licenses).
    inline constexpr int kOfflineGraceDays = 14;

    // How often to re-validate online while activated (catches expired/cancelled
    // subscriptions and revoked licenses). Done in the background, non-blocking.
    inline constexpr int kRevalidateIntervalDays = 3;

    //==========================================================================
    // Per-plugin Keygen product identifier. Each plugin passes its own name to
    // LicenseManager; this maps the plugin name → the Keygen product id/code you
    // created. A "suite" license is recognised via an entitlement (see manager).
    //==========================================================================
    inline juce::String productIdFor (const juce::String& pluginName)
    {
        // Replace the right-hand values with the product IDs (or codes) from
        // your Keygen dashboard once the products exist.
        static const std::map<juce::String, juce::String> map {
            { "VocalGrit",    "84df0f9d-ba27-4f30-b579-626c34288df0"    },
            { "VocalEss",     "77d1118c-b9d5-42db-bb61-76e85f7b7e31"     },
            { "VocalQ",       "3767ae07-b0c1-4ab6-a56e-9d25fcfec674"       },
            { "VocalKnob",    "15fc7a20-f639-4364-b1fe-7cd05629f9db"    },
            { "VocalAir",     "3cdacf19-820f-4a6b-9d39-d9944a0c7358"     },
            { "VocalComp",    "b334a051-268a-4f33-845d-60eda180374e"    },
            { "Vocal2A",      "cd07f476-94d9-45ab-ac7d-2de7a2d95da3"      },
            { "VocalTune",    "b993e784-c708-4e75-bc13-900855a262ad"    },
            { "VocalVerb",    "cc3d4350-7448-4295-8a84-8d4605115046"    },
            { "VocalDoubler", "858ac599-6e69-4a1b-b25e-bc9ba486b5b6" },
            { "VocalDelay",   "eaa60ffe-d141-42d9-8949-1e351704cd7e"   },
            { "VocalGate",    "7250a37e-d98d-45ab-b794-61d5f2898a1c" },
            { "VocalMod",     "5f7fb530-ea61-4001-9bd6-5339d2c2b515" },
            { "VocalBlend",   "3cf141d1-e3e2-432b-8ae1-c5c77509acfc" },
            { "VocalChop",    "57f02040-993f-4b79-924a-d064ea199b7b" },
        };
        auto it = map.find (pluginName);
        return it != map.end() ? it->second : juce::String();
    }

    // Entitlement code that marks a license as covering the whole suite.
    inline constexpr const char* kSuiteEntitlement = "SUITE";

    // The Keygen product id for the "VocalEssential Suite" product. A license
    // whose product is the suite unlocks every plugin. (Keygen's validate-key
    // response reliably includes the license's product id, whereas the SUITE
    // entitlement is only returned via a separate request — so the product id
    // is the primary, reliable signal for "this key covers everything".)
    inline constexpr const char* kSuiteProductId = "85d247b8-7fee-47e2-84f5-4fd89c90f29a";
}
