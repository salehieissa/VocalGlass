#!/usr/bin/env python3
"""
keygen_setup.py — provision the entire VocalEssential licensing setup on
Keygen.sh via the API, then (optionally) write the resulting IDs into
common/Licensing/LicenseConfig.h.

It creates, idempotently (safe to re-run — it reuses anything already there):
  • a "SUITE" entitlement
  • 12 products: the 11 plugins + "VocalEssential Suite"
  • a perpetual, 2-machine, floating policy for each of the 11 plugins  ($39.99)
  • two suite policies — "Suite – Monthly" (30-day) and "Suite – Lifetime" —
    both 2-machine + floating + tagged with the SUITE entitlement   ($14.99/mo, $149)

Usage:
  export KEYGEN_ACCOUNT="your-account-id-or-slug"
  export KEYGEN_TOKEN="admin-xxxxxxxx"          # admin token from Keygen dashboard
  python3 scripts/keygen_setup.py               # provision + patch LicenseConfig.h
  python3 scripts/keygen_setup.py --no-patch    # provision only, just print IDs
  python3 scripts/keygen_setup.py --dry-run     # show what it would do, change nothing

The admin token is read from the environment and never written anywhere.
"""

import json
import os
import re
import sys
import urllib.error
import urllib.request

PLUGINS = [
    "VocalGrit", "VocalEss", "VocalQ", "VocalKnob", "VocalAir", "VocalComp",
    "Vocal2A", "VocalTune", "VocalVerb", "VocalDoubler", "VocalDelay",
    "VocalGate", "VocalMod", "VocalBlend", "VocalChop",
]
SUITE_PRODUCT = "VocalEssential Suite"
SUITE_CODE = "SUITE"
MAX_MACHINES = 2
MONTH_SECONDS = 30 * 24 * 60 * 60

ACCOUNT = os.environ.get("KEYGEN_ACCOUNT", "").strip()
TOKEN = os.environ.get("KEYGEN_TOKEN", "").strip()
BASE = f"https://api.keygen.sh/v1/accounts/{ACCOUNT}"

DRY_RUN = "--dry-run" in sys.argv
NO_PATCH = "--no-patch" in sys.argv or DRY_RUN

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONFIG_H = os.path.join(REPO, "common", "Licensing", "LicenseConfig.h")


def die(msg):
    print(f"\033[1;31mERROR: {msg}\033[0m", file=sys.stderr)
    sys.exit(1)


def api(method, path, payload=None):
    """Make a Keygen API request and return parsed JSON (or {} on 204)."""
    url = path if path.startswith("http") else BASE + path
    data = json.dumps(payload).encode() if payload is not None else None
    req = urllib.request.Request(url, data=data, method=method)
    req.add_header("Authorization", f"Bearer {TOKEN}")
    req.add_header("Accept", "application/vnd.api+json")
    if data is not None:
        req.add_header("Content-Type", "application/vnd.api+json")
    try:
        with urllib.request.urlopen(req) as resp:
            body = resp.read().decode()
            return json.loads(body) if body else {}
    except urllib.error.HTTPError as e:
        body = e.read().decode()
        try:
            errs = json.loads(body).get("errors", [])
            detail = "; ".join(f"{x.get('title')}: {x.get('detail')}" for x in errs)
        except Exception:
            detail = body
        raise RuntimeError(f"{method} {url} -> {e.code}: {detail}")


def list_all(path):
    """Fetch a collection (single page, limit 100 — plenty for this)."""
    return api("GET", f"{path}?limit=100").get("data", [])


# ---- idempotent get-or-create helpers --------------------------------------
def ensure_entitlement(code, name):
    for e in list_all("/entitlements"):
        if (e["attributes"].get("code") or "").upper() == code.upper():
            print(f"  entitlement '{code}' exists ({e['id']})")
            return e["id"]
    if DRY_RUN:
        print(f"  [dry-run] would create entitlement '{code}'")
        return "DRYRUN_ENTITLEMENT"
    res = api("POST", "/entitlements",
              {"data": {"type": "entitlements",
                        "attributes": {"name": name, "code": code}}})
    eid = res["data"]["id"]
    print(f"  created entitlement '{code}' ({eid})")
    return eid


def ensure_product(name):
    for p in list_all("/products"):
        if p["attributes"].get("name") == name:
            print(f"  product '{name}' exists ({p['id']})")
            return p["id"]
    if DRY_RUN:
        print(f"  [dry-run] would create product '{name}'")
        return f"DRYRUN_{name}"
    res = api("POST", "/products",
              {"data": {"type": "products",
                        "attributes": {"name": name,
                                       "distributionStrategy": "LICENSED"}}})
    pid = res["data"]["id"]
    print(f"  created product '{name}' ({pid})")
    return pid


def ensure_policy(name, product_id, duration):
    for p in list_all("/policies"):
        if p["attributes"].get("name") == name:
            print(f"  policy '{name}' exists ({p['id']})")
            return p["id"]
    if DRY_RUN:
        print(f"  [dry-run] would create policy '{name}'")
        return f"DRYRUN_POLICY_{name}"
    attrs = {
        "name": name,
        "duration": duration,           # null = perpetual; seconds otherwise
        "maxMachines": MAX_MACHINES,
        "floating": True,               # one key, up to maxMachines computers
        "authenticationStrategy": "LICENSE",
        "machineUniquenessStrategy": "UNIQUE_PER_LICENSE",
        "machineMatchingStrategy": "MATCH_ANY",
        "expirationStrategy": "RESTRICT_ACCESS",
        "transferStrategy": "KEEP_EXPIRY",
    }
    res = api("POST", "/policies",
              {"data": {"type": "policies", "attributes": attrs,
                        "relationships": {
                            "product": {"data": {"type": "products",
                                                 "id": product_id}}}}})
    pid = res["data"]["id"]
    print(f"  created policy '{name}' ({pid})")
    return pid


def attach_entitlement(policy_id, entitlement_id):
    if DRY_RUN or policy_id.startswith("DRYRUN"):
        print(f"  [dry-run] would attach SUITE to policy {policy_id}")
        return
    # Already attached? Skip to stay idempotent.
    existing = list_all(f"/policies/{policy_id}/entitlements")
    if any(e["id"] == entitlement_id for e in existing):
        print(f"  SUITE already on policy {policy_id}")
        return
    api("POST", f"/policies/{policy_id}/entitlements",
        {"data": [{"type": "entitlements", "id": entitlement_id}]})
    print(f"  attached SUITE to policy {policy_id}")


# ---- patch LicenseConfig.h --------------------------------------------------
def patch_config(account, product_ids, suite_product_id):
    if not os.path.isfile(CONFIG_H):
        print(f"  (skip patch — {CONFIG_H} not found)")
        return
    text = open(CONFIG_H).read()

    text = re.sub(r'(kKeygenAccountId\s*=\s*)"[^"]*"',
                  rf'\1"{account}"', text)

    if suite_product_id and not suite_product_id.startswith("DRYRUN"):
        text = re.sub(r'(kSuiteProductId\s*=\s*)"[^"]*"',
                      rf'\1"{suite_product_id}"', text)

    for name, pid in product_ids.items():
        # Replace the placeholder value next to each plugin name in the map.
        text = re.sub(rf'(\{{\s*"{re.escape(name)}",\s*)"[^"]*"',
                      rf'\g<1>"{pid}"', text)

    open(CONFIG_H, "w").write(text)
    print(f"  patched {os.path.relpath(CONFIG_H, REPO)}")


# ---- main -------------------------------------------------------------------
def main():
    if not ACCOUNT:
        die("set KEYGEN_ACCOUNT (your Keygen account id or slug).")
    if not TOKEN:
        die("set KEYGEN_TOKEN (an admin token from the Keygen dashboard).")

    print(f"==> Keygen setup for account '{ACCOUNT}'"
          + (" [DRY RUN]" if DRY_RUN else ""))

    print("-- entitlement --")
    suite_ent = ensure_entitlement(SUITE_CODE, "Suite")

    print("-- products --")
    product_ids = {name: ensure_product(name) for name in PLUGINS}
    suite_product = ensure_product(SUITE_PRODUCT)

    print("-- per-plugin perpetual policies --")
    for name in PLUGINS:
        ensure_policy(f"{name} \u2013 Perpetual", product_ids[name], None)

    print("-- suite policies --")
    monthly = ensure_policy("Suite \u2013 Monthly", suite_product, MONTH_SECONDS)
    lifetime = ensure_policy("Suite \u2013 Lifetime", suite_product, None)
    attach_entitlement(monthly, suite_ent)
    attach_entitlement(lifetime, suite_ent)

    print("\n==> Product IDs (these go into LicenseConfig.h):")
    for name in PLUGINS:
        print(f"   {name:<14} {product_ids[name]}")

    if not NO_PATCH:
        print("\n-- patching LicenseConfig.h --")
        patch_config(ACCOUNT, product_ids, suite_product)

    print("\n\033[1;32mDone.\033[0m")
    if NO_PATCH and not DRY_RUN:
        print("(ran with --no-patch: IDs printed above, config left untouched.)")


if __name__ == "__main__":
    main()
