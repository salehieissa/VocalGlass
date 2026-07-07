# HANDOFF → Shopify Storefront Agent — VocalEssential v1.0.0 (UI-rework update)

> **Copy-paste this prompt to the Shopify agent to kick off ingestion:**
>
> ```
> Ingest HANDOFF-SHOPIFY.md from the VocalGlass plugin repo. You already know
> the VocalEssential catalog — this is the update handoff: the catalog is now
> 16 plugins (VocalRack is tossed — remove/unpublish any VocalRack or
> VocalRack-preset-pack SKUs), five plugins are new (VocalMod, VocalBlend,
> VocalChop, VocalClip, VocalGate) and every plugin got the photoreal UI
> rework. Attach the new ad-image set to every product (§2), add/verify SKUs
> for the five new plugins, reconcile the storefront .env
> (KEYGEN_POLICY_SINGLES / KEYGEN_POLICY_SUITE / KEYGEN_POLICY_PRO) against
> §3, and wire fulfillment (pkg delivery + license mint per policy). When
> done, produce HANDOFF-OPENCLAW.md as described at the end of this document.
> ```

---

## 1. What changed (deltas — you know the rest)

1. **Catalog is 16 plugins.** VocalRack is **tossed**: if a VocalRack product,
   variant, or "VocalRack Signature Chains" preset-pack SKU exists in the
   store, unpublish/remove it. Do not fulfill anything VocalRack.
2. **Five new plugins** (add or verify SKUs): VocalMod (`Vmod`), VocalBlend
   (`Vbld`), VocalChop (`Vchp`), VocalClip (`Vclp`), VocalGate (`Vgat`).
3. **UI rework suite-wide**: every plugin now has the photoreal
   white-glass/chrome/pink-neon plate UI — refresh ALL product imagery (§2);
   old screenshots no longer represent the product.
4. **Hero plugins for merchandising: VocalTune, VocalGrit, VocalChop.**
   Feature these three on the homepage, collections, and paid ads; each gets
   extra hero-shot imagery (§2).
5. Windows handoff refreshed the same day (`HANDOFF-WINDOWS.md`, 16 plugins);
   don't gate the store launch on Windows builds.

## 2. Imagery (new — attach to products)

All paths repo-relative; everything is platform-independent, ready to upload.

- **Primary product image, one per plugin (all 16)**:
  `marketing/renders/ads/<plugin>-product.png` — the plugin floating in 3D on
  **pure white**, Apple-ad style. Use as the first image on each product page.
- **Hero shots (campaign/banner use)** for the three hero plugins:
  `marketing/renders/ads/vocaltune-hero-1/2.png`,
  `…/vocalgrit-hero-1/2.png`, `…/vocalchop-hero-1/2.png`.
- **Suite images**: `marketing/renders/ads/suite-product-1/2/3.png` — family
  floating on white, for the bundle product page + collection banner.
- **Secondary gallery slots** (existing set, still valid):
  `marketing/renders/<plugin>-float@2x.png` (dark cinematic floating render)
  and `marketing/renders/<plugin>-hero@2x.png` (clean light-grey hero), plus
  `marketing/renders/suite-hero-float-1..3.png`.
- **Clean UI screenshots** (per plugin): `screenshots/clean/<Name>.png`.
- **Regeneration**: per-asset Higgsfield prompts in
  `marketing/HIGGSFIELD-PROMPTS.md` (base style block + per-plugin subject
  line; reference image = the plugin's clean screenshot). Use these to pull
  higher-quality re-renders of any asset without drift.

## 3. Keygen licensing (account: `salehieissa`)

- API base: `https://api.keygen.sh/v1/accounts/salehieissa`
- Suite product id: `85d247b8-7fee-47e2-84f5-4fd89c90f29a` (entitlement `SUITE`)
- Suite policies: **Lifetime** `30d094fc-e6c1-4eb0-a55b-461781cfe647`
  (perpetual), **Pro/Monthly** `04277162-4ee7-4c91-8fdb-9b7a96c782fc`
  (expiring; webhook stamps billing date + 35 days, `RESTRICT_ACCESS`).
- Per-plugin product ids: `storefront-handoff/keygen-policies.md`, plus
  VocalClip `c7ada00e-501f-4ff3-aa03-f81d1507ae5d`. **Ignore/archive the
  VocalRack product** (`d97d1149-f058-4c3c-a2e6-914f01d80fac`) and its policy
  entry (`vocalrack` in `keygen-policy-singles.json`) — tossed with the plugin.
- Single-plugin perpetual policy ids (`KEYGEN_POLICY_SINGLES`):
  `storefront-handoff/keygen-policy-singles.json` — use the 16 non-rack
  entries, including `vocalclip`: `b0c16544-ac60-4d95-8dce-6be4b5e2bc69`.
- Policy settings (all): floating, maxMachines 2, unique-per-license
  fingerprints, license-key auth.
- Pre-launch: delete the internal `TEST - Suite` license (key `21AA74-…-V3`).
- **Smoke test you must run** (token isn't in the repo):
  `KEYGEN_ADMIN_TOKEN=… node scripts/test-keygen.mjs` — mints + validates a
  license per policy.

## 4. Deliverable artifacts

- **Installers** (Developer ID-signed, notarized, stapled): `dist/`
  - Per plugin: `dist/<Name>-1.0.0.pkg` — ship the 16 non-rack pkgs.
  - Suite: `dist/VocalEssential-Suite-1.0.0.pkg`. **Note**: the current suite
    pkg was built when VocalRack was still in the lineup — rebuild it without
    rack before launch (`scripts/sign_and_notarize.sh` then
    `scripts/build_installer.sh` with VocalRack removed from the `PLUGINS`
    list; needs the Developer ID certs + notary keychain profile), or ship it
    as-is knowing it installs one extra unsupported plugin.
  - Each pkg installs VST3 → `/Library/Audio/Plug-Ins/VST3`, AU →
    `/Library/Audio/Plug-Ins/Components`, Standalone → `/Applications`.
  - Verified Jul 7 2026: pkgs pass `stapler validate` + `spctl --assess`.
- **Windows**: `HANDOFF-WINDOWS.md` (16 plugins, same layout mirrored to
  Inno Setup installers).

## 5. Product copy hooks (new plugins)

- **VocalMod**: chorus / flanger / phaser modulation FX voiced for vocals.
- **VocalBlend**: master-bus vocal/beat glue — sit the vocal in the beat with
  one knob-set.
- **VocalChop** (hero): tempo-synced, beat-grid phase-locked vocal chopper
  with live waveform display.
- **VocalClip**: soft clipper — 3 curve shapes, 4x oversampled HQ mode, live
  transfer-curve display, "Drake Vocal" factory preset.
- **VocalGate**: hybrid-detector noise gate (peak+RMS, hysteresis, sidechain
  HPF, SC listen).
- Suite-wide copy angle: photoreal glass/chrome/neon hardware look, SF Pro
  type, one visual family across all 16.

## 6. REQUIRED: produce the OpenClaw handoff

After you finish the store setup, **you (the Shopify agent) must write
`HANDOFF-OPENCLAW.md`** for the OpenClaw agent, summarizing:

1. Final store setup: products, variants, prices, collections created
   (16 plugins + suite; note that VocalRack was removed).
2. The complete product → Keygen policy ID mapping as deployed in the
   storefront `.env` (`KEYGEN_POLICY_SINGLES`, `KEYGEN_POLICY_SUITE`,
   `KEYGEN_POLICY_PRO`).
3. Fulfillment webhooks: which webhook mints licenses on purchase, which
   stamps/renews the Pro monthly expiry, delivery mechanics for pkg downloads.
4. Anything OpenClaw needs to operate the store day-to-day (refund → license
   revoke flow, support lookup by license key, adding future products).
