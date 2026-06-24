#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include "LicenseConfig.h"
#include <functional>
#include <atomic>
#include <vector>

//==============================================================================
// Cross-platform (Windows + macOS) licensing core for the VocalGlass family.
//
//   * PER-PLUGIN activation state, persisted per-user. An individual key unlocks
//     only its plugin; a suite key unlocks all 11 at once.
//   * A pluggable "verifier" seam: OFFLINE checksum by default (so the flow works
//     today) or the Keygen online verifier once an account ID is configured.
//   * ProductLicense: an audio-thread-safe handle each processor holds so
//     processBlock can mute when unlicensed without touching disk or network.
//
// Header-only to match the repo's existing header-only modules.
//==============================================================================
namespace licensing
{
    //==========================================================================
    // Key format: "VG-XXXX-XXXX-XXXX" (offline demo keys; real keys come from Keygen).
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

    inline bool isWellFormedKey (const juce::String& raw)
    {
        const auto& alpha = keyAlphabet();
        const auto body = normaliseKey (raw);
        if (body.length() != 12)
            return false;

        int sum = 0;
        for (int i = 0; i < 11; ++i)
            sum += alpha.indexOfChar (body[i]);

        return alpha.indexOfChar (body[11]) == (sum % alpha.length());
    }

    inline juce::String formatKey (const juce::String& body12)
    {
        jassert (body12.length() == 12);
        return "VG-" + body12.substring (0, 4) + "-"
                     + body12.substring (4, 8) + "-"
                     + body12.substring (8, 12);
    }

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
        juce::String normalisedKey;
    };

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

    // Online verifier backed by Keygen (https://keygen.sh). Validates the key
    // scoped to the entitlement passed as `productId`, so the same verifier works
    // for every plugin and for the suite check (productId == kSuiteEntitlement).
    inline Verifier makeKeygenVerifier (juce::String accountId)
    {
        return [accountId] (const juce::String& key, const juce::String& productId) -> VerifyResult
        {
            VerifyResult r;
            const juce::String endpoint = "https://api.keygen.sh/v1/accounts/"
                                        + accountId + "/licenses/actions/validate-key";

            juce::DynamicObject::Ptr meta = new juce::DynamicObject();
            meta->setProperty ("key", key.trim());

            if (productId.isNotEmpty())
            {
                juce::Array<juce::var> ents;
                ents.add (productId);
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
    // Interface implemented by ProductLicense so the manager can flip live
    // instances the moment a key is accepted (no reload required).
    struct LicenseListener
    {
        virtual ~LicenseListener() = default;
        virtual juce::String product() const = 0;
        virtual void setLicensed (bool) = 0;
    };

    //==========================================================================
    class LicenseManager
    {
    public:
        static LicenseManager& getInstance()
        {
            static LicenseManager inst;
            return inst;
        }

        static juce::File licenseFile()
        {
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("VocalEssential")
                       .getChildFile ("license.json");
        }

        // Is a specific plugin unlocked on this machine?
        bool isUnlocked (const juce::String& productId) const
        {
            return unlockedProducts.contains (productId);
        }

        juce::String getKey() const { return storedKey; }

        void setVerifier (Verifier v) { verifier = std::move (v); }

        // Attempt activation for a plugin. On success unlocks that plugin; if the
        // key is instead a suite key, unlocks every plugin.
        VerifyResult activate (const juce::String& key, const juce::String& productId)
        {
            auto res = verifier (key, productId);
            if (res.ok)
            {
                storedKey = res.normalisedKey.isNotEmpty() ? res.normalisedKey : key;
                unlock (productId);
                return res;
            }

            // Not valid for this plugin specifically — try the suite entitlement.
            if (keygenConfigured())
            {
                auto suite = verifier (key, kSuiteEntitlement);
                if (suite.ok)
                {
                    storedKey = suite.normalisedKey.isNotEmpty() ? suite.normalisedKey : key;
                    unlockAll();
                    suite.message = "Activated (Suite)";
                    return suite;
                }
            }
            return res;
        }

        void deactivate()
        {
            unlockedProducts.clear();
            storedKey.clear();
            licenseFile().deleteFile();
            const juce::ScopedLock sl (listLock);
            for (auto* l : listeners)
                l->setLicensed (false);
        }

        // ProductLicense registration (called on the message thread).
        void registerListener (LicenseListener* l)
        {
            const juce::ScopedLock sl (listLock);
            listeners.push_back (l);
        }

        void unregisterListener (LicenseListener* l)
        {
            const juce::ScopedLock sl (listLock);
            listeners.erase (std::remove (listeners.begin(), listeners.end(), l), listeners.end());
        }

    private:
        LicenseManager()
        {
            verifier = makeOfflineVerifier();
            load();
        }

        void unlock (const juce::String& productId)
        {
            if (! unlockedProducts.contains (productId))
                unlockedProducts.add (productId);
            save();
            flip (productId);
        }

        void unlockAll()
        {
            for (auto& p : allProducts())
                if (! unlockedProducts.contains (p))
                    unlockedProducts.add (p);
            save();
            const juce::ScopedLock sl (listLock);
            for (auto* l : listeners)
                l->setLicensed (true);
        }

        void flip (const juce::String& productId)
        {
            const juce::ScopedLock sl (listLock);
            for (auto* l : listeners)
                if (l->product() == productId)
                    l->setLicensed (true);
        }

        void load()
        {
            auto f = licenseFile();
            if (! f.existsAsFile())
                return;

            auto json = juce::JSON::parse (f);
            storedKey = json.getProperty ("key", "").toString();

            if (auto* arr = json.getProperty ("products", juce::var()).getArray())
                for (auto& v : *arr)
                    unlockedProducts.addIfNotAlreadyThere (v.toString());
        }

        void save()
        {
            auto f = licenseFile();
            f.getParentDirectory().createDirectory();

            juce::Array<juce::var> arr;
            for (auto& p : unlockedProducts)
                arr.add (p);

            juce::DynamicObject::Ptr obj = new juce::DynamicObject();
            obj->setProperty ("key", storedKey);
            obj->setProperty ("products", arr);
            obj->setProperty ("savedAt", juce::Time::getCurrentTime().toMilliseconds());
            f.replaceWithText (juce::JSON::toString (juce::var (obj.get())));
        }

        Verifier verifier;
        juce::StringArray unlockedProducts;
        juce::String storedKey;

        juce::CriticalSection listLock;
        std::vector<LicenseListener*> listeners;
    };

    //==========================================================================
    // A handle each processor holds. isLicensed() is safe to call from the audio
    // thread (atomic read, no disk/network). Initialised from the cached state at
    // construction and flipped live when the user activates.
    class ProductLicense : public LicenseListener
    {
    public:
        explicit ProductLicense (juce::String productCode)
            : code (std::move (productCode))
        {
            licensed.store (LicenseManager::getInstance().isUnlocked (code));
            LicenseManager::getInstance().registerListener (this);
        }

        ~ProductLicense() override
        {
            LicenseManager::getInstance().unregisterListener (this);
        }

        bool isLicensed() const noexcept { return licensed.load (std::memory_order_relaxed); }

        juce::String product() const override   { return code; }
        void setLicensed (bool b) override       { licensed.store (b, std::memory_order_relaxed); }

    private:
        juce::String code;
        std::atomic<bool> licensed { false };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProductLicense)
    };
}
