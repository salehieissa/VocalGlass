#include "LicenseManager.h"

using juce::String;
using juce::var;

//==============================================================================
LicenseManager::LicenseManager (String pluginNameIn)
    : pluginName (std::move (pluginNameIn)),
      productId  (licensing::productIdFor (pluginName))
{
}

LicenseManager::~LicenseManager()
{
    pool.removeAllJobs (true, 2000);
}

//==============================================================================
juce::File LicenseManager::storeFile() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    dir = dir.getChildFile ("Application Support");
   #endif
    dir = dir.getChildFile ("VocalEssential");
    dir.createDirectory();
    return dir.getChildFile ("license.json");
}

String LicenseManager::fingerprint() const
{
    // Stable per-machine identifier; Keygen only needs it to be consistent.
    auto id = juce::SystemStats::getUniqueDeviceID();
    if (id.isEmpty())
        id = juce::SystemStats::getComputerName() + "-" + juce::SystemStats::getLogonName();
    return String (id.hashCode64()).toLowerCase();
}

String LicenseManager::endpoint (const String& path) const
{
    return String (licensing::kKeygenApiBase) + licensing::kKeygenAccountId + path;
}

//==============================================================================
juce::String LicenseManager::getStatusMessage() const { return statusMessage; }

void LicenseManager::setStatus (Status s, String message)
{
    status.store (s);
    statusMessage = std::move (message);
    juce::MessageManager::callAsync ([this] { if (onStatusChange) onStatusChange(); });
}

void LicenseManager::saveCache()
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("key", licenseKey);
    obj->setProperty ("lastValidatedMs", lastValidatedMs);
    obj->setProperty ("activated", status.load() == Status::Activated);
    storeFile().replaceWithText (juce::JSON::toString (var (obj)));
}

//==============================================================================
void LicenseManager::loadCachedAndValidate()
{
    auto f = storeFile();
    if (f.existsAsFile())
    {
        auto json = juce::JSON::parse (f.loadFileAsString());
        licenseKey      = json.getProperty ("key", "").toString();
        lastValidatedMs = (juce::int64) json.getProperty ("lastValidatedMs", (juce::int64) 0);
        const bool wasActivated = (bool) json.getProperty ("activated", false);

        if (licenseKey.isEmpty())
        {
            setStatus (Status::Unlicensed, "No license key.");
            return;
        }

        const auto ageDays = (juce::Time::currentTimeMillis() - lastValidatedMs) / (1000.0 * 60 * 60 * 24);

        if (wasActivated && ageDays < licensing::kOfflineGraceDays)
            setStatus (Status::Activated, "Activated."); // optimistic; confirm below
        else
            setStatus (Status::Validating, "Checking license\u2026");

        revalidate(); // confirm online (won't lock an in-grace license on net failure)
        return;
    }

    setStatus (Status::Unlicensed, "No license key.");
}

//==============================================================================
void LicenseManager::activate (const String& key, Callback done)
{
    const auto trimmed = key.trim();
    if (trimmed.isEmpty()) { if (done) done (Status::Invalid, "Enter a license key."); return; }

    setStatus (Status::Validating, "Activating\u2026");

    pool.addJob ([this, trimmed, done]
    {
        auto res = validateKeyBlocking (trimmed);

        if (res.networkError) { setStatus (Status::NetworkError, "Couldn't reach the license server."); if (done) juce::MessageManager::callAsync([done]{ done (Status::NetworkError, "Couldn't reach the license server."); }); return; }

        if (res.valid && res.entitled)
        {
            licenseKey = trimmed; lastValidatedMs = juce::Time::currentTimeMillis();
            setStatus (Status::Activated, "Activated."); saveCache();
            if (done) juce::MessageManager::callAsync([done]{ done (Status::Activated, "Activated."); });
            return;
        }

        if (res.noMachine && res.entitled)
        {
            String err;
            if (activateMachineBlocking (trimmed, res.licenseId, err))
            {
                auto res2 = validateKeyBlocking (trimmed);
                if (res2.valid && res2.entitled)
                {
                    licenseKey = trimmed; lastValidatedMs = juce::Time::currentTimeMillis();
                    setStatus (Status::Activated, "Activated."); saveCache();
                    if (done) juce::MessageManager::callAsync([done]{ done (Status::Activated, "Activated."); });
                    return;
                }
                if (res2.tooMany) { setStatus (Status::TooManyMachines, "License already in use on 2 machines."); if (done) juce::MessageManager::callAsync([done]{ done (Status::TooManyMachines, "License already in use on 2 machines."); }); return; }
            }
            setStatus (Status::TooManyMachines, err.isNotEmpty() ? err : "Couldn't activate this machine.");
            if (done) { auto m = statusMessage; juce::MessageManager::callAsync([done,m]{ done (Status::TooManyMachines, m); }); }
            return;
        }

        if (res.tooMany)  { setStatus (Status::TooManyMachines, "License already in use on 2 machines."); if (done) juce::MessageManager::callAsync([done]{ done (Status::TooManyMachines, "License already in use on 2 machines."); }); return; }
        if (res.expired)  { setStatus (Status::Expired, "This license has expired."); if (done) juce::MessageManager::callAsync([done]{ done (Status::Expired, "This license has expired."); }); return; }
        if (res.valid && ! res.entitled) { setStatus (Status::Invalid, "This license doesn't cover " + pluginName + "."); if (done) { auto m = statusMessage; juce::MessageManager::callAsync([done,m]{ done (Status::Invalid, m); }); } return; }

        setStatus (Status::Invalid, res.message.isNotEmpty() ? res.message : "Invalid license key.");
        if (done) { auto m = statusMessage; juce::MessageManager::callAsync([done,m]{ done (Status::Invalid, m); }); }
    });
}

void LicenseManager::revalidate (Callback done)
{
    if (licenseKey.isEmpty()) { setStatus (Status::Unlicensed, "No license key."); if (done) done (Status::Unlicensed, "No license key."); return; }

    const auto key = licenseKey;
    const bool inGrace = (status.load() == Status::Activated);

    pool.addJob ([this, key, inGrace, done]
    {
        auto res = validateKeyBlocking (key);

        if (res.networkError)
        {
            // Don't lock a license that's still within its offline grace window.
            if (! inGrace) setStatus (Status::NetworkError, "Couldn't reach the license server.");
            if (done) juce::MessageManager::callAsync([done, s = status.load(), m = statusMessage]{ done (s, m); });
            return;
        }

        if (res.valid && res.entitled)
        {
            lastValidatedMs = juce::Time::currentTimeMillis();
            setStatus (Status::Activated, "Activated."); saveCache();
        }
        else if (res.expired)            setStatus (Status::Expired, "This license has expired.");
        else if (res.tooMany)            setStatus (Status::TooManyMachines, "License already in use on 2 machines.");
        else if (res.valid && ! res.entitled) setStatus (Status::Invalid, "This license doesn't cover " + pluginName + ".");
        else if (res.noMachine)          setStatus (Status::Unlicensed, "This machine isn't activated.");
        else                             setStatus (Status::Invalid, res.message.isNotEmpty() ? res.message : "Invalid license key.");

        if (done) juce::MessageManager::callAsync([done, s = status.load(), m = statusMessage]{ done (s, m); });
    });
}

void LicenseManager::deactivate()
{
    licenseKey.clear();
    lastValidatedMs = 0;
    storeFile().deleteFile();
    setStatus (Status::Unlicensed, "No license key.");
}

//==============================================================================
// Networking (blocking — always called from the thread pool, never the audio
// or message thread).
//==============================================================================
static std::unique_ptr<juce::InputStream> postJson (const juce::String& url,
                                                     const juce::String& body,
                                                     const juce::String& licenseKeyAuth,
                                                     int& statusCodeOut)
{
    juce::String headers ("Content-Type: application/vnd.api+json\r\nAccept: application/vnd.api+json");
    if (licenseKeyAuth.isNotEmpty())
        headers << "\r\nAuthorization: License " << licenseKeyAuth;

    // NOTE: ParameterHandling::inAddress (not inPostData) so our explicit JSON
    // body is sent verbatim. With inPostData, JUCE folds any URL query params
    // into the POST data and corrupts the body (Keygen then rejects it as
    // "invalid JSON"). Keep request URLs free of query strings too.
    juce::URL u = juce::URL (url).withPOSTData (body);
    return std::unique_ptr<juce::InputStream> (u.createInputStream (
        juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
            .withExtraHeaders (headers)
            .withConnectionTimeoutMs (12000)
            .withStatusCode (&statusCodeOut)));
}

LicenseManager::ValidateResult LicenseManager::validateKeyBlocking (const String& key) const
{
    ValidateResult r;

    const String body = "{\"meta\":{\"key\":\"" + key + "\","
                        "\"scope\":{\"fingerprint\":\"" + fingerprint() + "\"}}}";

    int code = 0;
    auto stream = postJson (endpoint ("/licenses/actions/validate-key"),
                            body, {}, code);
    if (stream == nullptr) { r.networkError = true; return r; }

    auto json = juce::JSON::parse (stream->readEntireStreamAsString());
    if (! json.isObject()) { r.networkError = true; return r; }

    if (auto* meta = json.getProperty ("meta", var()).getDynamicObject())
    {
        r.valid = (bool) meta->getProperty ("valid");
        r.code  = meta->getProperty ("detail").toString();        // human text
        r.message = meta->getProperty ("detail").toString();
        const auto codeStr = meta->getProperty ("code").toString();

        r.expired   = (codeStr == "EXPIRED");
        r.tooMany   = (codeStr == "TOO_MANY_MACHINES");
        r.noMachine = (codeStr == "NO_MACHINE" || codeStr == "NO_MACHINES"
                       || codeStr == "FINGERPRINT_SCOPE_MISMATCH");
    }

    if (auto* data = json.getProperty ("data", var()).getDynamicObject())
    {
        r.licenseId = data->getProperty ("id").toString();

        // Entitlement check: this plugin's product, or the SUITE entitlement.
        juce::String licProductId;
        if (auto* rels = data->getProperty ("relationships").getDynamicObject())
            if (auto* prod = rels->getProperty ("product").getDynamicObject())
                if (auto* pdata = prod->getProperty ("data").getDynamicObject())
                    licProductId = pdata->getProperty ("id").toString();

        // Entitled if the license is for this exact plugin's product, or for the
        // whole-suite product. (The validate-key response always carries the
        // license's product id, so this is the reliable check.)
        bool entitled = (productId.isNotEmpty() && licProductId == productId)
                        || (licProductId == licensing::kSuiteProductId);

        if (auto inc = json.getProperty ("included", var()); inc.isArray())
            for (auto& item : *inc.getArray())
                if (auto* o = item.getDynamicObject())
                {
                    const auto type = o->getProperty ("type").toString();
                    if (type == "products" && o->getProperty ("id").toString() == productId)
                        entitled = true;
                    if (type == "entitlements")
                        if (auto* attrs = o->getProperty ("attributes").getDynamicObject())
                            if (attrs->getProperty ("code").toString().equalsIgnoreCase (licensing::kSuiteEntitlement))
                                entitled = true;
                }

        r.entitled = entitled;
    }

    return r;
}

bool LicenseManager::activateMachineBlocking (const String& key, const String& licenseId,
                                              String& errOut) const
{
    if (licenseId.isEmpty()) { errOut = "Missing license id."; return false; }

   #if JUCE_MAC
    const char* platform = "macOS";
   #elif JUCE_WINDOWS
    const char* platform = "Windows";
   #else
    const char* platform = "Other";
   #endif

    const String name = juce::SystemStats::getComputerName();
    const String body =
        "{\"data\":{\"type\":\"machines\",\"attributes\":{"
            "\"fingerprint\":\"" + fingerprint() + "\","
            "\"platform\":\"" + platform + "\","
            "\"name\":\"" + name + "\"},"
        "\"relationships\":{\"license\":{\"data\":{"
            "\"type\":\"licenses\",\"id\":\"" + licenseId + "\"}}}}}";

    int code = 0;
    auto stream = postJson (endpoint ("/machines"), body, key, code);
    if (stream == nullptr) { errOut = "Network error."; return false; }

    auto response = stream->readEntireStreamAsString();
    if (code == 201) return true;

    auto json = juce::JSON::parse (response);
    if (auto errs = json.getProperty ("errors", var()); errs.isArray() && errs.getArray()->size() > 0)
        if (auto* e = errs.getArray()->getFirst().getDynamicObject())
            errOut = e->getProperty ("detail").toString();
    if (errOut.isEmpty()) errOut = "Activation failed (" + String (code) + ").";
    return false;
}
