# Keygen setup — products & policies (15 singles + 2 suite = 17 policies)

> Update Jul 6 (later): VocalChop added — product + perpetual policy provisioned.
> Update Jul 6: VocalGate, VocalMod and VocalBlend were added to the suite.
> Their Keygen products + perpetual policies are now PROVISIONED (via
> scripts/keygen_setup.py) — ids below and in keygen-policy-singles.json.
> Remaining storefront work: add their SKUs to the Shopify catalog and their
> policy ids to KEYGEN_POLICY_SINGLES in the storefront .env.

Account: `salehieissa`  ·  API base: `https://api.keygen.sh/v1/accounts/`

## Products (already exist — do not recreate; verify ids)
SUITE product id: `85d247b8-7fee-47e2-84f5-4fd89c90f29a`  (suite entitlement code: SUITE)

Per-plugin product ids:
| Plugin       | Product id                                |
|--------------|-------------------------------------------|
| VocalGrit    | 84df0f9d-ba27-4f30-b579-626c34288df0      |
| VocalEss     | 77d1118c-b9d5-42db-bb61-76e85f7b7e31      |
| VocalQ       | 3767ae07-b0c1-4ab6-a56e-9d25fcfec674      |
| VocalKnob    | 15fc7a20-f639-4364-b1fe-7cd05629f9db      |
| VocalAir     | 3cdacf19-820f-4a6b-9d39-d9944a0c7358      |
| VocalComp    | b334a051-268a-4f33-845d-60eda180374e      |
| Vocal2A      | cd07f476-94d9-45ab-ac7d-2de7a2d95da3      |
| VocalTune    | b993e784-c708-4e75-bc13-900855a262ad      |
| VocalVerb    | cc3d4350-7448-4295-8a84-8d4605115046      |
| VocalDoubler | 858ac599-6e69-4a1b-b25e-bc9ba486b5b6      |
| VocalDelay   | eaa60ffe-d141-42d9-8949-1e351704cd7e      |
| VocalGate    | 7250a37e-d98d-45ab-b794-61d5f2898a1c      |
| VocalMod     | 5f7fb530-ea61-4001-9bd6-5339d2c2b515      |
| VocalBlend   | 3cf141d1-e3e2-432b-8ae1-c5c77509acfc      |
| VocalChop    | 57f02040-993f-4b79-924a-d064ea199b7b      |

## Policies to create (13 total)

### Common settings for ALL 13
- floating: true
- maxMachines: 2
- machineUniquenessStrategy: UNIQUE_PER_LICENSE
- requireFingerprintScope: true        (so the plugin's POST /machines path triggers)
- machineLeasingStrategy: PER_LICENSE
- authenticationStrategy: LICENSE        (plugin auths with the license key)

### 1–13: Single-plugin policies (one per plugin product)  — PERPETUAL
- product: the matching per-plugin product id above
- expiry: none (perpetual)
- name suggestion: "VocalGrit — Single (perpetual)", etc.

### 12: Suite — Lifetime policy  — PERPETUAL
- product: SUITE (85d247b8-…)
- expiry: none

### 13: Suite — Monthly policy  — EXPIRING (subscription)
- product: SUITE (85d247b8-…)
- requires expiry: YES (webhook stamps expiry = billing date + 35 days)
- expirationStrategy: RESTRICT_ACCESS   (or REVOKE_ACCESS)
  → ensures validate-key returns code EXPIRED once the expiry passes, which is
    what the plugin keys off to lock + show "This license has expired."
- Do NOT use MAINTAIN_ACCESS / ALLOW_ACCESS (license would keep validating after
  expiry and never lock).

## After creating, record the policy ids into env
- KEYGEN_POLICY_SUITE   = <policy 12 id>
- KEYGEN_POLICY_PRO     = <policy 13 id>
- KEYGEN_POLICY_SINGLES = the 14 single policy ids (see keygen-policy-singles.json —
  now includes vocalgate 2c83714b-87d3-4889-bde8-0d1a17d66819,
  vocalmod 8dd81145-4bc0-45d1-b48d-29060c4f35a1 and
  vocalblend 66670ecf-ca18-438a-817c-e09466c67836)

## Pre-launch cleanup
- Delete the internal `TEST - Suite` license (key 21AA74-…-V3) before going live.
