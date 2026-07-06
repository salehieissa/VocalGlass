# VocalEssential — Shopify + Keygen storefront setup (handoff)

You are finishing the e-commerce + licensing setup for the VocalEssential plugin
suite (15 audio plugins). The plugin client and the Keygen validation contract
are already built and shipping; your job is the storefront catalog, the Keygen
policies, and wiring the existing webhook routes so a purchase auto-mints and
emails a license. Do NOT change the plugin↔Keygen contract — match it.

> Jul 6 update: four plugins were added — VocalGate, VocalMod, VocalBlend,
> VocalChop. Their Keygen products + perpetual policies are ALREADY PROVISIONED
> (ids in `keygen-policies.md` / `keygen-policy-singles.json`). Storefront work
> for them: add their SKUs/variants and merge the four new entries into
> `KEYGEN_POLICY_SINGLES` in the storefront .env.

## Pricing / SKUs to build
1. 15 individual plugins — $39.99 each, PERPETUAL (no expiry).
2. VocalEssential Suite — Lifetime — $149.00, PERPETUAL (unlocks all 15).
3. VocalEssential Suite — Monthly — $14.99/month SUBSCRIPTION (unlocks all 15,
   locks when the sub lapses/cancels).

Full catalog, variants, SKUs, and product copy are in `shopify-catalog.md`.
The exact Keygen products, the 17 policies, and their settings are in
`keygen-policies.md`. Env template is `keygen.env.example`; the singles map is
`keygen-policy-singles.json`.

## How the plugin decides "unlocked" (the contract — don't break it)
- Plugin POSTs to Keygen `licenses/actions/validate-key` with a per-machine
  fingerprint scope, then POSTs `/machines` if the license has no machine yet.
- A license unlocks a plugin if its **product id == that plugin's product id**,
  OR == the **SUITE product id** (`85d247b8-7fee-47e2-84f5-4fd89c90f29a`).
  → The product the policy belongs to is the unlock signal. Singles must be
    minted under that plugin's product; Suite (lifetime) and Monthly (sub) must
    BOTH be minted under the SUITE product.
- Machine limit = 2 (floating). Offline grace = 14 days. Re-validation happens on
  plugin load (NOT on a timer — do not assume periodic mid-session re-checks).
- Codes the plugin reacts to: `EXPIRED` → "expired" + lock; `TOO_MANY_MACHINES`
  → "in use on 2 machines"; `NO_MACHINE`/`NO_MACHINES`/`FINGERPRINT_SCOPE_MISMATCH`
  → registers the machine. A SUSPENDED license also locks (shown as generic
  "invalid") — fine for refunds.

## Tasks
1. **Shopify catalog**: create the products/variants/prices in `shopify-catalog.md`.
   Each individual-plugin variant's SKU MUST equal its pluginId (lowercase, e.g.
   `vocalgrit`, `vocalchop`). The orders/paid handler routes SKU-first (singles by SKU, monthly
   by selling_plan presence, suite by `suite-lifetime` SKU) with the existing
   numeric variant_id map as fallback — so set SKUs correctly if you recreate any
   product, and keep the variant_id fallback map in sync for existing products.
2. **Subscriptions**: use Shopify-native Subscriptions. Attach a monthly selling
   plan ($14.99) to the "Suite — Monthly" product. The Lifetime suite is a normal
   one-time product.
3. **Keygen policies**: ALL 17 are already provisioned (15 singles + suite +
   monthly) — ids in `keygen-policies.md`. Just VERIFY settings match:
   floating=true, maxMachines=2, REQUIRE fingerprint/machine scope. Singles +
   Lifetime → no expiry. Monthly → expiring, expiration strategy
   RESTRICT_ACCESS or REVOKE_ACCESS (so validate-key returns EXPIRED on lapse).
4. **Env vars**: fill `keygen.env.example` values into local `.env` and Oxygen.
   `KEYGEN_POLICY_SINGLES` is the 15-entry JSON map (`keygen-policy-singles.json`).
5. **Webhooks** (these routes already exist in the Hydrogen app — just register
   them in Shopify Admin → Settings → Notifications → Webhooks, all JSON, all
   using the same `SHOPIFY_WEBHOOK_SECRET`):
   - `orders/paid`        → /webhooks/shopify/orders-paid
   - `orders/refunded`    → /webhooks/shopify/orders-refunded
   - native sub renewal   → /webhooks/shopify/subscription-renewed
   - native sub cancel    → /webhooks/shopify/subscription-cancelled
6. **Smoke test**: run `node scripts/test-keygen.mjs` with the env values. Every
   line must be green AND each minted license's product.id must match the expected
   product (singles → their product, suite/monthly → SUITE product). A red/mismatch
   means that policy sits under the wrong product — fix before going live.
7. **End-to-end test**: place a real (test-mode) order for a single, the lifetime
   suite, and the monthly sub. Confirm: license minted under the right product,
   key emailed (Klaviyo, render `license.key` verbatim), pasting it in the plugin
   activates. Then refund the single → license suspends → plugin locks on relaunch.

## Notes
- Renewal matching is email-based for launch; `subscriptionContractId` /
  `sellingPlanId` are already captured (status `captured-unused`) so switching to
  contract-id matching later needs no backfill.
- Before going live, delete the internal `TEST - Suite` Keygen license.
