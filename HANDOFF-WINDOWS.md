# VocalEssential — Windows Build & Release Handoff

Everything a Windows engineer (or CI runner) needs to ship the 15-plugin suite
on Windows. The codebase is already cross-platform JUCE 8 / CMake; nothing in
the DSP or UI is macOS-specific. macOS-only pieces (AU format, universal-binary
flag, notarization) are guarded and simply don't apply on Windows.

## The suite (15 plugins)

| Plugin | Code | What it is |
|---|---|---|
| VocalGrit | `Vgrt` | saturation / texture |
| VocalEss | `Vess` | de-esser |
| VocalQ | `Vqeq` | dynamic vocal EQ |
| VocalKnob | `Vknb` | one-knob vocal enhancer |
| VocalAir | `Vair` | air / presence exciter |
| VocalComp | `Vcmp` | vocal compressor |
| Vocal2A | `V2al` | opto leveler (LA-2A style) |
| VocalTune | `Vtun` | pitch correction |
| VocalVerb | `Vrvb` | vocal reverb |
| VocalDoubler | `Vdbl` | doubler |
| VocalDelay | `Vdly` | delay |
| VocalGate | `Vgat` | gate |
| VocalMod | `Vmod` | chorus / flanger / phaser |
| VocalBlend | `Vbld` | master-bus vocal/beat glue |
| VocalChop | `Vchp` | tempo-synced chopper |

All share `PLUGIN_MANUFACTURER_CODE Vgls`, company "VocalEssential".

## Prerequisites

- **Visual Studio 2022** (Desktop development with C++ workload) — MSVC v143,
  Windows 10/11 SDK. The code is C++20 (`set(CMAKE_CXX_STANDARD 20)`).
- **CMake ≥ 3.22** (bundled with VS is fine).
- **Git** (JUCE 8.0.4 is pulled automatically via `FetchContent` on first
  configure — needs network access once; it's cached in each build dir after).
- No other dependencies. UI skin PNGs under `assets/ui/` are embedded into the
  binaries by CMake (`juce_add_binary_data` via each plugin's
  `VG_SKIN_CANDIDATES` list) — nothing to install or copy at runtime.

## Build (per plugin)

Each plugin is its own CMake project in its own folder (there is no top-level
CMakeLists). Formats on Windows: **VST3 + Standalone** (AU is macOS-only and
is skipped automatically by JUCE).

```bat
:: from the repo root, for each of the 15 plugin folders:
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
           "VocalGate","VocalMod","VocalBlend","VocalChop"
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

Recommended: **Inno Setup 6** — one suite installer plus 15 singles, mirroring
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

1. All 15 build clean in Release (no warnings-as-errors are enabled, but check
   the log for anything alarming).
2. Load each VST3 in a Windows DAW (FL Studio + Ableton minimum). Confirm:
   - UI renders the photoreal plate edge-to-edge (crop-to-chrome), no dark
     slivers at any edge, knobs/domes centered in their grooves.
   - Automation: move every knob from the DAW side, UI follows.
   - VocalChop/VocalDelay tempo-sync follow host BPM and restart on the grid.
3. `pluginval` (Tracktion, free) at strictness 5+ on all 15 VST3s.
4. Activation overlay + license flow on at least 2 plugins (one single, one
   suite key).
5. HiDPI: check 100% / 150% / 200% scaling — the UI is a scaled bitmap plate,
   it should stay crisp (JUCE handles DPI); look for blurry text at 200%.

## Deliverables back

- `VocalEssential-Suite-1.0.0.exe` + 15 single installers, all signed.
- A short build log (which VS/SDK versions used).
- pluginval reports (txt) per plugin.
