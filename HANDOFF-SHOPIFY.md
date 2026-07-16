# HANDOFF → Shopify Storefront Agent — VocalEssential v1.0.0 (UI-rework update)

> **Copy-paste this prompt to the Shopify agent to kick off ingestion:**
>
> ```
> Ingest HANDOFF-SHOPIFY.md from the VocalGlass plugin repo. You already know
> the VocalEssential catalog and shipped the 16-plugin v1.0.0 store. This is
> the Jul 16 delta handoff: ONE new plugin, VocalGeek — read §0 first and do
> exactly what it says: create the VocalGeek product + SKU, add vocalgeek to
> KEYGEN_POLICY_SINGLES in the storefront env (Oxygen too, not just local),
> REPLACE the Suite download zip (it now contains the 17-plugin macOS pkg)
> on both the Suite-Lifetime and Pro-Monthly products, attach the new
> VocalGeek download zip to the new product, and schedule the Windows
> follow-up swap described there. Everything else from the Jul 7 handoff
> (sections 1-5) is unchanged and already live. When done, update
> HANDOFF-OPENCLAW.md with the new product + policy row.
> ```

---

## 0. Jul 16 2026 UPDATE — VocalGeek (do this now; the rest of this doc is the Jul 7 baseline you already shipped)

**The catalog is now 17 plugins.** VocalGeek is a brand-new plugin — the
handheld "dose console": a Game Boy-style vocal performance FX unit with five
"cartridges" (lean / smoke / acid / snow / geeked) that each load a different
FX chain and recolor the whole console, a live pixel screen, stutter/tape-stop
performance pads, tempo-synced auto mode, print-to-WAV drag export, MIDI-
playable pads, per-cartridge settings memory, and a hidden 6th cartridge
behind a cheat code. It slots after VocalChain-style processing as the
"performance / destruction" plugin. Merchandise it as a drop/limited-feel
release — it is deliberately scroll-stopping.

### 0.1 Files (all fresh, notarized, verified Jul 16)

- **New product download**: `dist-zip/VocalGeek-1.0.0-mac-win.zip`
  (macOS pkg + README; **the Windows msi is NOT in it yet** — see 0.4).
- **REPLACE the Suite download**: `dist-zip/VocalEssential-Suite-1.0.0-mac-win.zip`
  was rebuilt — the macOS pkg inside now installs **all 17 plugins including
  VocalGeek**. Swap this file on **both** products that deliver the suite:
  the Suite (Lifetime) product **and the Pro (Monthly) subscription**. The
  README inside tells Windows users the msi update is imminent (it still
  installs 16 until the Windows agent ships — see 0.4).
- Raw pkgs if you need them: `dist/VocalGeek-1.0.0.pkg`,
  `dist/VocalEssential-Suite-1.0.0.pkg` — both Developer ID-signed,
  notarized, stapled, `spctl` "accepted / Notarized Developer ID".
- `dist/manifest-1.0.0.json` regenerated (18 entries: 17 singles + suite)
  with fresh sha256 for the two changed/new pkgs — verify against it before
  attaching.

### 0.2 Keygen (already provisioned — wire it, don't create it)

- Product **VocalGeek**: `28c55338-b57a-4749-bb0f-d90496bfc3f4`
- Policy **VocalGeek – Perpetual**: `38f91492-884c-4ee1-8e51-af33203be12d`
  (floating, maxMachines 2 — identical settings to every other single).
- `storefront-handoff/keygen-policy-singles.json` updated (17 entries,
  `vocalrack` removed, `vocalgeek` added). **Update `KEYGEN_POLICY_SINGLES`
  in the deployed storefront env (Oxygen)** — the local `.env` in the
  Hydrogen repo has already been updated as a reference.
- Suite + Pro keys already unlock VocalGeek (suite-product check in the
  plugin) — no policy change needed on the suite side.
- QA key if you want to test activation end-to-end:
  `ACC160-604AA4-FF6FE4-B362B2-6F90B2-V3` (internal; don't ship it).

### 0.3 Product listing

- SKU/handle: `vocalgeek`, plugin code `Vgek`, price in line with the other
  singles ($39.99).
- Copy hooks: five drug-cartridge FX modes that recolor the whole console;
  live pixel screen that reacts to the vocal (syrup drips, smoke plumes,
  melting acid rainbow, snow flurry, glitch); stutter + tape-stop hit pads;
  tempo-synced AUTO performance brain; print & drag the frozen loop straight
  into the DAW as WAV; MIDI-playable; every cartridge remembers its own
  settings; hidden overdose cartridge behind a Game Boy cheat code.
- Imagery: `screenshots/clean/VocalGeek.png` is in the repo now. The
  Higgsfield ad-render set (`vocalgeek-product.png` float on white etc.) is
  NOT rendered yet — launch the product with the clean screenshot, and the
  render set will follow via `marketing/HIGGSFIELD-PROMPTS.md` conventions.

### 0.4 Windows follow-up (scheduled swap — don't gate launch on it)

`HANDOFF-WINDOWS.md` (Jul 16 section) tasks the Windows agent with building
`VocalGeek-1.0.0.msi` and rebuilding `VocalEssential-Suite-1.0.0.msi` with all
17. When those land, the mac side re-splices both `-mac-win.zip` files and
tells you — at that point re-attach the refreshed Suite zip (both suite
products) and VocalGeek zip. Until then the READMEs inside the zips cover the
gap honestly.

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
