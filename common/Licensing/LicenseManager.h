#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "LicenseConfig.h"

#include <atomic>
#include <functional>

//==============================================================================
// Shared license engine for all VocalEssential plugins (Keygen.sh backend).
//
//  - Hard lock: a plugin is only usable when getStatus() == Activated.
//  - One shared activation file (~/Library/Application Support/VocalEssential or
//    %APPDATA%\VocalEssential) so a "suite" key activates every plugin once.
//  - Online validate + machine activation (2-machine limit enforced by Keygen).
//  - Offline grace period for previously-valid licenses.
//  - All network work runs off the message thread; callbacks come back on it.
//
// NOTE: this talks to Keygen via JUCE's URL class (no libcurl needed). It is
// not yet wired into the processors/editors — see the integration notes.
//==============================================================================
class LicenseManager
{
public:
    enum class Status
    {
        Unknown,         // not yet checked
        Unlicensed,      // no key stored
        Validating,      // network check in flight
        Activated,       // valid + entitled + machine activated
        Expired,         // subscription/license lapsed
        TooManyMachines, // hit the activation limit
        NetworkError,    // could not reach Keygen (and outside offline grace)
        Invalid          // bad key, or key doesn't cover this plugin
    };

    explicit LicenseManager (juce::String pluginName);
    ~LicenseManager();

    Status       getStatus() const noexcept { return status.load(); }
    bool         isActivated() const noexcept { return status.load() == Status::Activated; }
    juce::String getStatusMessage() const;
    juce::String getLicenseKey() const { return licenseKey; }

    // Load cached key/activation and decide status (honours offline grace),
    // then kick a background re-validation. Call once at startup.
    void loadCachedAndValidate();

    using Callback = std::function<void (Status, juce::String /*message*/)>;

    void activate   (const juce::String& key, Callback done = {}); // validate + activate machine
    void revalidate (Callback done = {});                          // online re-check of stored key
    void deactivate ();                                            // clear local activation

    // Called on the message thread whenever status changes (refresh your UI).
    std::function<void()> onStatusChange;

private:
    struct ValidateResult
    {
        bool valid = false, expired = false, tooMany = false, noMachine = false,
             networkError = false, entitled = false;
        juce::String code, message, licenseId;
    };

    ValidateResult validateKeyBlocking (const juce::String& key) const;
    bool           activateMachineBlocking (const juce::String& key,
                                            const juce::String& licenseId,
                                            juce::String& errOut) const;

    void         setStatus (Status, juce::String message);
    void         saveCache();
    juce::File   storeFile() const;
    juce::String fingerprint() const;
    juce::String endpoint (const juce::String& path) const;

    juce::String pluginName, productId;
    juce::String licenseKey;
    juce::int64  lastValidatedMs = 0;

    std::atomic<Status> status { Status::Unknown };
    juce::String        statusMessage;

    juce::ThreadPool pool { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LicenseManager)
};
