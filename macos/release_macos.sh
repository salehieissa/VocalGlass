#!/usr/bin/env bash
#
# VocalGlass — macOS release pipeline (run this ON YOUR MAC).
#
# It builds Release, code-signs every artefact with your Developer ID, builds
# signed .pkg installers (one per plugin + a full suite), then notarizes and
# staples each installer so Gatekeeper opens them with no warnings.
#
# ── ONE-TIME SETUP (see the chat checklist) ─────────────────────────────────
#   1. Install two certs into your login keychain (Apple Developer site):
#        • "Developer ID Application"   (signs .vst3/.component/.app)
#        • "Developer ID Installer"     (signs .pkg)
#   2. Store a notarytool credential profile once:
#        xcrun notarytool store-credentials "vocalnotary" \
#            --apple-id "you@vocalessential.com" \
#            --team-id  "YOURTEAMID" \
#            --password "app-specific-password"
#      (or use --key / --key-id / --issuer for an App Store Connect API key)
#
# ── FILL THESE IN ───────────────────────────────────────────────────────────
DEV_ID_APP="Developer ID Application: VocalEssential LLC (YOURTEAMID)"
DEV_ID_INSTALLER="Developer ID Installer: VocalEssential LLC (YOURTEAMID)"
NOTARY_PROFILE="vocalnotary"
VERSION="0.1.0"
BUNDLE_PREFIX="com.vocalessential"
DO_BUILD=1   # set to 0 to skip the cmake build and just sign/package existing artefacts
# ─────────────────────────────────────────────────────────────────────────────

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"   # the VocalGlass repo root
OUT="$REPO/macos/dist"
STAGE_ROOT="$REPO/macos/_stage"
PLUGINS=(Vocal2A VocalAir VocalComp VocalDelay VocalDoubler VocalEss VocalGrit VocalKnob VocalQ VocalTune VocalVerb)

rm -rf "$OUT" "$STAGE_ROOT"
mkdir -p "$OUT"

# Resolve the artefact bundle for a plugin + format, regardless of whether the
# generator put a Release/ subfolder in the path.
find_bundle () {  # $1 plugin  $2 ext (vst3|component|app)  $3 subdir (VST3|AU|Standalone)
  find "$REPO/$1/build" -type d -name "$1.$2" -path "*/$3/*" 2>/dev/null | head -n1
}

sign_bundle () {  # $1 path to .vst3/.component/.app
  [ -d "$1" ] || return 0
  echo "  sign: $1"
  codesign --force --options runtime --timestamp --sign "$DEV_ID_APP" "$1"
}

build_plugin () {  # $1 plugin
  echo "==== build $1 ===="
  ( cd "$REPO/$1" && \
    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release >/dev/null && \
    cmake --build build --config Release -j >/dev/null )
}

# Make a signed, notarized .pkg from a staging root.
make_pkg () {  # $1 staging-root  $2 identifier  $3 output.pkg  $4 human name
  local unsigned="$OUT/_unsigned_$(basename "$3")"
  pkgbuild --root "$1" --identifier "$2" --version "$VERSION" \
           --install-location "/" "$unsigned" >/dev/null
  productsign --sign "$DEV_ID_INSTALLER" "$unsigned" "$3"
  rm -f "$unsigned"
  echo "  notarize: $4"
  xcrun notarytool submit "$3" --keychain-profile "$NOTARY_PROFILE" --wait
  xcrun stapler staple "$3"
}

# Build a staging tree that installs a plugin's VST3 + AU to the standard dirs.
stage_plugin () {  # $1 plugin  $2 staging-root
  local vst3 au
  vst3="$(find_bundle "$1" vst3 VST3)"
  au="$(find_bundle "$1" component AU)"
  mkdir -p "$2/Library/Audio/Plug-Ins/VST3" "$2/Library/Audio/Plug-Ins/Components"
  [ -n "$vst3" ] && cp -R "$vst3" "$2/Library/Audio/Plug-Ins/VST3/"
  [ -n "$au"   ] && cp -R "$au"   "$2/Library/Audio/Plug-Ins/Components/"
}

# ── 1) build + sign every plugin's artefacts ────────────────────────────────
for p in "${PLUGINS[@]}"; do
  [ "$DO_BUILD" = "1" ] && build_plugin "$p"
  echo "==== sign $p ===="
  sign_bundle "$(find_bundle "$p" vst3 VST3)"
  sign_bundle "$(find_bundle "$p" component AU)"
  sign_bundle "$(find_bundle "$p" app Standalone)"
done

# ── 2) individual installers (notarized) ────────────────────────────────────
for p in "${PLUGINS[@]}"; do
  echo "==== package $p ===="
  stage="$STAGE_ROOT/$p"
  stage_plugin "$p" "$stage"
  make_pkg "$stage" "$BUNDLE_PREFIX.$p" "$OUT/$p-macOS.pkg" "$p"
done

# ── 3) full suite installer (notarized) ─────────────────────────────────────
echo "==== package SUITE ===="
suite="$STAGE_ROOT/_suite"
for p in "${PLUGINS[@]}"; do stage_plugin "$p" "$suite"; done
make_pkg "$suite" "$BUNDLE_PREFIX.suite" "$OUT/VocalGlass-Suite-macOS.pkg" "VocalGlass Suite"

echo ""
echo "===== DONE — installers in $OUT ====="
ls -1 "$OUT"
