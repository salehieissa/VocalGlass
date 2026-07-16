# VocalEssential — Windows Build & Release Handoff

You already know the suite — this is the **update handoff**. The catalog is now
**17 plugins**; the update started with the photoreal UI rework, five plugins
were new in the last sync, and **VocalGeek is brand new in this one**. The
codebase is cross-platform JUCE 8 / CMake; nothing in the DSP or UI is
macOS-specific (AU, universal-binary flag, notarization are guarded off on
Windows).

## UPDATES — Jul 16 2026 (read this first)

### YOUR TASK LIST for this sync (in order)

1. `git pull` — the VocalGeek source, its `geek-*` skin assets, and the
   updated `common/Licensing/LicenseConfig.h` (VocalGeek product id) are all
   required; a stale checkout builds a plugin that can't activate.
2. Build **VocalGeek** (VST3 + Standalone, Release, x64) with the same CMake
   command as every other plugin. Project version is 1.0.0.
3. QA it against the checklist at the bottom (item 6 is VocalGeek-specific);
   activate with the QA key in §2 below.
4. Produce `VocalGeek-1.0.0.msi` (same Inno/MSI pattern as the other singles:
   VST3 → `{commoncf64}\VST3`, standalone optional component). Sign it.
5. **Rebuild the suite installer** as `VocalEssential-Suite-1.0.0.msi` with
   VocalGeek added (17 plugins). Sign it. The macOS side already shipped its
   17-plugin suite pkg — the Windows suite is the missing half.
6. Send back: `VocalGeek-1.0.0.msi`, the updated
   `VocalEssential-Suite-1.0.0.msi`, a pluginval report for VocalGeek, and
   `screenshots/windows/VocalGeek.png`. The mac side will splice them into the
   combined `-mac-win.zip` downloads (suite + single) and hand the refreshed
   files to the Shopify agent. Until your files arrive, the store's suite zip
   ships a 17-plugin pkg + 16-plugin msi with a README note — so treat this
   as time-sensitive.

### What's new in detail

1. **New plugin: VocalGeek** (`Vgek`) — the handheld "dose console". A Game
   Boy-style vocal performance FX unit: five cartridges (lean / smoke / acid /
   snow / geeked, each a different DSP chain + full UI recolor), a code-drawn
   pixel screen with per-theme reactive waveform scenes, HIT A stutter /
   HIT B tape-stop pads, PRINT freeze with drag-to-DAW WAV export, tempo-synced
   AUTO pilot, D-pad texture/space nudges, and a hidden 6th cartridge behind a
   D-pad cheat code (up up down down left left right right).
   - Same per-folder CMake pattern; added to the build lists below.
   - `NEEDS_MIDI_INPUT TRUE` (unique in the suite): C1 holds HIT A, D1 holds
     HIT B, E1 taps the rate. Verify the VST3 receives MIDI in FL/Ableton.
   - Skin assets are the `geek-*` PNGs in `assets/ui/` (embedded like all
     others). Pull latest — the pixel-screen code and assets are new.
   - Its CMakeLists also defines a small console target `GeekScreenPreview`
     (offscreen screen-renderer used for visual QA on mac; it writes to
     `/tmp`). It will compile on Windows if you build the whole solution —
     ignore it, don't ship it, or build the specific plugin targets only.
   - Per-cartridge settings memory: each cartridge remembers its own
     dose/texture/space/rate/auto/output within a session and in saved DAW
     state — worth a quick QA pass (set dose on two cartridges, swap back and
     forth, reload the project).
   - Drag the PRINT pill to the DAW/desktop to export the frozen loop as WAV
     (JUCE `performExternalDragDropOfFiles` — cross-platform, but verify on
     Windows).
2. **VocalGeek licensing is provisioned** (nothing for you to create):
   Keygen product `28c55338-b57a-4749-bb0f-d90496bfc3f4`, policy "VocalGeek –
   Perpetual" `38f91492-884c-4ee1-8e51-af33203be12d`, product id already
   patched into `common/Licensing/LicenseConfig.h`. Internal QA key you can
   activate with (2 machines, floating):
   `ACC160-604AA4-FF6FE4-B362B2-6F90B2-V3`. Suite keys unlock VocalGeek too.
3. **Version note**: VocalGeek's project version is currently `0.1.0` — ship
   its first Windows installer as `1.0.0` to match the catalog (that is also
   the plan for the next macOS pkg pass).

## UPDATES — Jul 7 2026

1. **New plugins (5)**: VocalMod (`Vmod`), VocalBlend (`Vbld`), VocalChop
   (`Vchp`), VocalClip (`Vclp`), VocalGate (`Vgat`). Same per-folder CMake
   pattern as the rest; they're in the build lists below.
2. **UI rework (everything)**: every plugin now renders a photoreal
   white-glass/chrome/pink-neon plate (crop-to-chrome, edge-to-edge). Skin
   PNGs live in `assets/ui/` and are embedded by each plugin's CMakeLists
   (`VG_SKIN_CANDIDATES` → `juce_add_binary_data`) — nothing to install at
   runtime, but **pull latest before building** so the embedded art is current.
3. **VocalRack is TOSSED** — it may still exist in the repo, but do **not**
   build, package, or ship it. It is not part of the 16-plugin catalog.
4. **Images — what's already made vs. what Windows must produce**:
   - All store/marketing images are platform-independent and already rendered:
     `marketing/renders/` (`<plugin>-float@2x.png` dark cinematic floating
     render + `<plugin>-hero@2x.png` clean hero, per plugin), the Apple-style
     white ad set in `marketing/renders/ads/` (`<plugin>-product.png` per
     plugin, extra hero shots for the hero plugins VocalTune / VocalGrit /
     VocalChop, and `suite-product-*.png`). **Reuse these — do not re-render
     on Windows.**
   - To regenerate any render at higher quality: per-asset prompts in
     `marketing/HIGGSFIELD-PROMPTS.md` (base style block + per-plugin subject
     line; feed the matching `screenshots/clean/<Name>.png` as the reference
     image).
   - What Windows DOES need to capture: **one clean UI screenshot per plugin
     standalone on Windows** — for QA parity against `screenshots/clean/` and
     any "runs on Windows" proof. Launch each standalone, dismiss the
     audio-settings banner, capture the plugin window only (Win+Shift+S, or a
     window-handle capture in CI), 100% DPI scaling, save as
     `screenshots/windows/<Name>.png`. Layouts should match the macOS shots
     pixel-for-pixel (the UI is a scaled bitmap plate).

## The suite (17 plugins)

| Plugin | Code | What it is |
|---|---|---|
| VocalGrit | `Vgrt` | saturation / texture — **hero plugin** |
| VocalEss | `Vess` | de-esser |
| VocalQ | `Vqeq` | dynamic vocal EQ |
| VocalKnob | `Vknb` | one-knob vocal enhancer |
| VocalAir | `Vair` | air / presence exciter |
| VocalComp | `Vcmp` | vocal compressor |
| Vocal2A | `V2al` | opto leveler (LA-2A style) |
| VocalTune | `Vtun` | pitch correction — **hero plugin** |
| VocalVerb | `Vrvb` | vocal reverb |
| VocalDoubler | `Vdbl` | doubler |
| VocalDelay | `Vdly` | delay |
| VocalGate | `Vgat` | gate — **new** |
| VocalMod | `Vmod` | chorus / flanger / phaser — **new** |
| VocalBlend | `Vbld` | master-bus vocal/beat glue — **new** |
| VocalChop | `Vchp` | tempo-synced chopper — **new, hero plugin** |
| VocalClip | `Vclp` | soft clipper (3 shapes, 4x OS, live transfer curve) — **new** |
| VocalGeek | `Vgek` | handheld dose console: cartridge FX + pixel screen — **NEW this sync** |

All share `PLUGIN_MANUFACTURER_CODE Vgls`, company "VocalEssential".

## Prerequisites

- **Visual Studio 2022** (Desktop development with C++ workload) — MSVC v143,
  Windows 10/11 SDK. The code is C++20 (`set(CMAKE_CXX_STANDARD 20)`).
- **CMake ≥ 3.22** (bundled with VS is fine).
- **Git** (JUCE 8.0.4 is pulled automatically via `FetchContent` on first
  configure — needs network access once; it's cached in each build dir after).
- No other dependencies. UI skin PNGs under `assets/ui/` are embedded into the
  binaries by CMake — nothing to install or copy at runtime.

## Build (per plugin)

Each plugin is its own CMake project in its own folder (there is no top-level
CMakeLists). Formats on Windows: **VST3 + Standalone** (AU is macOS-only and
is skipped automatically by JUCE).

```bat
:: from the repo root, for each of the 17 plugin folders:
cmake -S Vocal2A -B Vocal2A\build-win -G "Visual Studio 17 2022" -A x64
cmake --build Vocal2A\build-win --config Release
```

Artifacts land in:

```
<Plugin>\build-win\<Plugin>_artefacts\Release\VST3\<Plugin>.vst3        (folder bundle)
<Plugin>\build-win\<Plugin>_artefacts\Release\Standalone\<Plugin>.exe
```

`COPY_PLUGIN_AFTER_BUILD TRUE` also copies the VST3 into
`C:\Program Files\Common Files\VST3` when the build runs elevated; if not
elevated JUCE copies to the user VST3 dir. For CI, just take the artefacts
paths above.

Batch-build everything (PowerShell):

```powershell
$plugins = "VocalGrit","VocalEss","VocalQ","VocalKnob","VocalAir","VocalComp",
           "Vocal2A","VocalTune","VocalVerb","VocalDoubler","VocalDelay",
           "VocalGate","VocalMod","VocalBlend","VocalChop","VocalClip","VocalGeek"
foreach ($p in $plugins) {
  cmake -S $p -B "$p\build-win" -G "Visual Studio 17 2022" -A x64
  cmake --build "$p\build-win" --config Release
  if ($LASTEXITCODE -ne 0) { throw "$p failed" }
}
```

Notes:
- The `CMAKE_OSX_ARCHITECTURES` block in each CMakeLists is inside `if(APPLE)`
  — ignored on Windows.
- `JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0` are already set; on Windows JUCE
  uses WinINet for HTTPS, so **licensing network calls work with no extra
  libraries**.

## Licensing (already wired — verify, don't change)

- Config lives in `common/Licensing/LicenseConfig.h` (Keygen.sh account
  `salehieissa`, per-plugin product IDs, suite product ID). It is compiled in;
  identical behavior on Windows.
- Machine fingerprinting and the activation overlay are cross-platform JUCE
  code (`common/Licensing/LicenseManager.cpp`, `ActivationOverlay.h`).
- License cache is stored per-user via JUCE's
  `userApplicationDataDirectory` → `%APPDATA%` on Windows. Nothing to set up.
- QA check per plugin: launch the Standalone, confirm the activation overlay
  appears, activate with a test key (Keygen dashboard → create a license under
  the plugin's policy), confirm it unlocks and survives a relaunch.

## Code signing (strongly recommended)

Windows SmartScreen will warn on unsigned installers. Use an OV/EV code-signing
certificate (e.g. Sectigo/DigiCert; EV removes the SmartScreen warning
fastest):

```bat
signtool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 ^
  /f cert.pfx /p <password> <file>
```

Sign, in order: every `<Plugin>.vst3\Contents\x86_64-win\<Plugin>.vst3` DLL,
every Standalone `.exe`, then the installer itself.

## Installer

Recommended: **Inno Setup 6** — one suite installer plus 16 singles, mirroring
the macOS `dist/` layout (`VocalEssential-Suite-1.0.0.pkg` ↔
`VocalEssential-Suite-1.0.0.exe`).

Install locations:
- VST3 → `{commoncf64}\VST3` (i.e. `C:\Program Files\Common Files\VST3`)
- Standalone (optional component) → `{autopf64}\VocalEssential\<Plugin>`

Minimal Inno skeleton per plugin section:

```iss
[Files]
Source: "Vocal2A\build-win\Vocal2A_artefacts\Release\VST3\Vocal2A.vst3\*"; \
  DestDir: "{commoncf64}\VST3\Vocal2A.vst3"; Flags: recursesubdirs createallsubdirs
Source: "Vocal2A\build-win\Vocal2A_artefacts\Release\Standalone\Vocal2A.exe"; \
  DestDir: "{autopf64}\VocalEssential\Vocal2A"; Components: standalone
```

Version all installers `1.0.0` to match the macOS release.

## QA checklist before shipping

1. All 17 build clean in Release (no warnings-as-errors are enabled, but check
   the log for anything alarming).
2. Load each VST3 in a Windows DAW (FL Studio + Ableton minimum). Confirm:
   - UI renders the photoreal plate edge-to-edge (crop-to-chrome), no dark
     slivers at any edge, knobs/domes centered in their grooves, pink value
     arcs track every knob.
   - Automation: move every knob from the DAW side, UI follows.
   - VocalChop/VocalDelay tempo-sync follow host BPM and restart on the grid.
3. `pluginval` (Tracktion, free) at strictness 5+ on all 17 VST3s.
4. Activation overlay + license flow on at least 2 plugins (one single, one
   suite key). For VocalGeek use the QA key in UPDATES §2.
5. HiDPI: check 100% / 150% / 200% scaling — the UI is a scaled bitmap plate,
   it should stay crisp (JUCE handles DPI); look for blurry text at 200%.
6. VocalGeek-specific: cartridge click cycles all five themes (bay art, button
   color, screen scene all change together, no swap/loading screen — it's
   instant); pixel screen animates (idle scene after ~5s of silence); MIDI
   C1/D1 trigger the hit pads; PRINT drag exports a WAV; the D-pad cheat code
   (up up down down left left right right) unlocks the overdose cartridge;
   per-cartridge settings memory survives a project reload.

## Deliverables back

- `VocalEssential-Suite-1.0.0.exe` + 17 single installers, all signed.
- A short build log (which VS/SDK versions used).
- pluginval reports (txt) per plugin.
- `screenshots/windows/<Name>.png` per plugin (see UPDATES §4).
