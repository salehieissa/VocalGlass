#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <functional>

//==============================================================================
// Lightweight, cross-platform (Windows + macOS) licensing core for the
// VocalGlass family.
//
//   * Local persistence of the activation result (so the user only activates
//     once) in a shared per-user file, so unlocking the Suite unlocks every
//     plugin on the machine.
//   * A pluggable "verifier" seam: by default it does an OFFLINE checksum
//     validation so the activation flow works end-to-end today. When you pick a
//     licensing service (Keygen / Cryptolens / etc.) you swap in an online
//     verifier — see makeHttpVerifier() below for the shape of that call.
//
// NOTE: this file is intentionally header-only to match the repo's existing
// header-only UI modules, so no CMakeLists changes are required.
//==============================================================================
namespace licensing
{
    //==========================================================================
    // Key format: "VG-XXXX-XXXX-XXXX"
    // 12 payload chars from an unambiguous alphabet; the final char is a
    // checksum of the preceding 11. This lets us both validate offline and
    // generate well-formed demo keys.
    //==========================================================================
    inline const juce::String& keyAlphabet()
    {
        static const juce::String a { "ABCDEFGHJKLMNPQRSTUVWXYZ23456789" }; // 32 chars
        return a;
    }

    inline juce::String normaliseKey (const juce::String& raw)
    {
        return raw.toUpperCase().retainCharacters (keyAlphabet());
    }

    // Returns the 13-char body (no dashes / no "VG") for a typed key, or empty.
    inline juce::String keyBody (const juce::String& raw)
    {
        auto body = normaliseKey (raw);
        return body;
    }

    inline bool isWellFormedKey (const juce::String& raw)
    {
        const auto& alpha = keyAlphabet();
        const auto body = normaliseKey (raw);
        if (body.length() != 12)
            return false;

        int sum = 0;
        for (int i = 0; i < 11; ++i)
            sum += alpha.indexOfChar (body[i]);

        const int expected = sum % alpha.length();
        return alpha.indexOfChar (body[11]) == expected;
    }

    // Pretty-print 12 payload chars as VG-XXXX-XXXX-XXXX.
    inline juce::String formatKey (const juce::String& body12)
    {
        jassert (body12.length() == 12);
        return "VG-" + body12.substring (0, 4) + "-"
                     + body12.substring (4, 8) + "-"
                     + body12.substring (8, 12);
    }

    // Generate a well-formed key from a seed (handy for demos / your key server
    // reference implementation).
    inline juce::String generateKey (juce::int64 seed)
    {
        const auto& alpha = keyAlphabet();
        juce::Random r (seed);
        juce::String body;
        int sum = 0;
        for (int i = 0; i < 11; ++i)
        {
            const int idx = r.nextInt (alpha.length());
            sum += idx;
            body << alpha[idx];
        }
        body << alpha[sum % alpha.length()];
        return formatKey (body);
    }

    //==========================================================================
    struct VerifyResult
    {
        bool ok = false;
        juce::String message;
        juce::String normalisedKey;   // canonical form to persist
    };

    // A verifier turns a (key, productId) into a result. Swappable: offline
    // today, online once a service is chosen.
    using Verifier = std::function<VerifyResult (const juce::String& key,
                                                 const juce::String& productId)>;

    inline Verifier makeOfflineVerifier()
    {
        return [] (const juce::String& key, const juce::String&) -> VerifyResult
        {
            VerifyResult r;
            if (isWellFormedKey (key))
            {
                r.ok = true;
                r.normalisedKey = formatKey (normaliseKey (key));
                r.message = "Activated";
            }
            else
            {
                r.message = "That key doesn't look right. Check it and try again.";
            }
            return r;
        };
    }

    // Online verifier backed by Keygen (https://keygen.sh). Calls the public
    // "validate-key" action, optionally scoped to an entitlement so a single
    // plugin's key and the full-suite key both validate correctly:
    //   * an individual VocalGrit license carries the VOCALGRIT entitlement
    //   * the Suite license carries every plugin's entitlement
    //   * a subscription that lapses comes back invalid (code EXPIRED), which
    //     is how the $14.99/mo tier gets revoked automatically.
    //
    // `accountId` = your Keygen account ID; `entitlement` = this plugin's code.
    inline Verifier makeKeygenVerifier (juce::String accountId, juce::String entitlement)
    {
        return [accountId, entitlement] (const juce::String& key, const juce::String&) -> VerifyResult
        {
            VerifyResult r;
            const juce::String endpoint = "https://api.keygen.sh/v1/accounts/"
                                        + accountId + "/licenses/actions/validate-key";

            // Build the JSON:API request body: { "meta": { "key": "...",
            //                                              "scope": { "entitlements": ["..."] } } }
            juce::DynamicObject::Ptr meta = new juce::DynamicObject();
            meta->setProperty ("key", key.trim());

            if (entitlement.isNotEmpty())
            {
                juce::Array<juce::var> ents;
                ents.add (entitlement);
                juce::DynamicObject::Ptr scope = new juce::DynamicObject();
                scope->setProperty ("entitlements", ents);
                meta->setProperty ("scope", juce::var (scope.get()));
            }

            juce::DynamicObject::Ptr root = new juce::DynamicObject();
            root->setProperty ("meta", juce::var (meta.get()));
            const juce::String requestBody = juce::JSON::toString (juce::var (root.get()));

            juce::StringPairArray responseHeaders;
            int statusCode = 0;
            juce::URL url = juce::URL (endpoint).withPOSTData (requestBody);

            auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                               .withExtraHeaders ("Content-Type: application/vnd.api+json\r\n"
                                                  "Accept: application/vnd.api+json")
                               .withConnectionTimeoutMs (8000)
                               .withResponseHeaders (&responseHeaders)
                               .withStatusCode (&statusCode);

            if (auto stream = url.createInputStream (options))
            {
                const auto resp = stream->readEntireStreamAsString();
                auto json = juce::JSON::parse (resp);
                auto m = json.getProperty ("meta", juce::var());
                const bool valid = (bool) m.getProperty ("valid", false);
                const juce::String detail = m.getProperty ("detail", "").toString();

                r.ok = valid;
                r.normalisedKey = key.trim().toUpperCase();
                r.message = valid ? "Activated"
                                  : (detail.isNotEmpty() ? detail : "License not valid.");
            }
            else
            {
                r.message = "Couldn't reach the license server. Check your connection.";
            }
            return r;
        };
    }

    //==========================================================================
    class LicenseManager
    {
    public:
        static LicenseManager& getInstance()
        {
            static LicenseManager inst;
            return inst;
        }

        // Where the shared activation record lives (per-user, all plugins share).
        static juce::File licenseFile()
        {
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("VocalEssential")
                       .getChildFile ("license.json");
        }

        bool isActivated() const { return activated; }
        juce::String getKey() const { return storedKey; }

        void setVerifier (Verifier v) { verifier = std::move (v); }

        // Attempt activation for a given product; persists on success.
        VerifyResult activate (const juce::String& key, const juce::String& productId)
        {
            auto res = verifier (key, productId);
            if (res.ok)
            {
                activated = true;
                storedKey = res.normalisedKey.isNotEmpty() ? res.normalisedKey : key;
                save();
            }
            return res;
        }

        void deactivate()
        {
            activated = false;
            storedKey.clear();
            licenseFile().deleteFile();
        }

    private:
        LicenseManager()
        {
            verifier = makeOfflineVerifier();
            load();
        }

        void load()
        {
            auto f = licenseFile();
            if (! f.existsAsFile())
                return;

            auto json = juce::JSON::parse (f);
            storedKey = json.getProperty ("key", "").toString();
            activated = storedKey.isNotEmpty() && (bool) json.getProperty ("activated", false);
        }

        void save()
        {
            auto f = licenseFile();
            f.getParentDirectory().createDirectory();

            juce::DynamicObject::Ptr obj = new juce::DynamicObject();
            obj->setProperty ("activated", activated);
            obj->setProperty ("key", storedKey);
            obj->setProperty ("savedAt", juce::Time::getCurrentTime().toMilliseconds());
            f.replaceWithText (juce::JSON::toString (juce::var (obj.get())));
        }

        Verifier verifier;
        bool activated = false;
        juce::String storedKey;
    };
}
