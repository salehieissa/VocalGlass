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

## 4. Deliverable artifacts — CLEAN REBUILD Jul 7 2026 (all pkgs fresh)

Every binary and pkg was rebuilt from scratch on Jul 7 2026 (machine cleaned
first — old bundles uninstalled, VocalRack remnants purged). **Every previous
pkg you have attached in Shopify Digital Downloads is stale; replace all of
them with this set.**

- **UPLOAD THESE → `dist-zip/<Name>-1.0.0-mac-win.zip`** (17 zips, one per
  product): each contains the notarized macOS `.pkg`, the Windows `.msi`, and
  a README-install.txt. Attach ONE zip per product in Digital Downloads —
  customers get both platforms in a single download.
- **17 pkgs in `dist/`** (Developer ID-signed, notarized ticket **stapled**,
  all pass `codesign --verify --deep --strict`, `stapler validate`, and
  `spctl --assess --type install` → "accepted / Notarized Developer ID"):
  - 16 × `dist/<Name>-1.0.0.pkg` — one per plugin.
  - `dist/VocalEssential-Suite-1.0.0.pkg` — all 16, **no VocalRack**
    (payload verified rack-free).
- **Swap map for Digital Downloads**: 11 replacements (Vocal2A, VocalAir,
  VocalComp, VocalDelay, VocalDoubler, VocalEss, VocalGrit, VocalKnob,
  VocalQ, VocalTune, VocalVerb) + 5 new attachments (VocalMod, VocalBlend,
  VocalChop, VocalClip, VocalGate) + the Suite replacement. Delete any
  `VocalRack-1.0.0.pkg` attachment — it no longer exists.
- **Manifest**: `dist/manifest-1.0.0.json` — filename, plugin id, version,
  bytes, sha256, and notarization status for all 17 pkgs. Use the sha256 to
  confirm the file you attach is the fresh one.
- Each pkg installs VST3 → `/Library/Audio/Plug-Ins/VST3`, AU →
  `/Library/Audio/Plug-Ins/Components`, Standalone → `/Applications`.
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
