#!/usr/bin/env bash
#
# sign_and_notarize.sh — code-sign, notarize, and staple all VocalEssential
# plugins (VST3 + AU + Standalone) for macOS distribution.
#
# ── One-time setup ───────────────────────────────────────────────────────────
#   1. Apple Developer account ($99/yr) with a "Developer ID Application"
#      certificate installed in your login keychain. Check with:
#         security find-identity -v -p codesigning
#   2. Create an app-specific password at https://appleid.apple.com (Sign-In &
#      Security → App-Specific Passwords), then store notarization creds once:
#         xcrun notarytool store-credentials "VocalEssential" \
#             --apple-id "you@example.com" \
#             --team-id  "YOURTEAMID" \
#             --password "abcd-efgh-ijkl-mnop"
#
# ── Usage ────────────────────────────────────────────────────────────────────
#   export DEV_ID_APP="Developer ID Application: Your Name (YOURTEAMID)"
#   export NOTARY_PROFILE="VocalEssential"      # keychain profile from step 2
#   ./scripts/sign_and_notarize.sh all          # sign → notarize → staple → verify
#
#   Sub-commands: sign | notarize | staple | verify | all   (default: all)
#
# Run this AFTER your final Release build, since each rebuild overwrites the
# installed bundles with unsigned copies.
# ─────────────────────────────────────────────────────────────────────────────

set -euo pipefail

# --- Config -------------------------------------------------------------------
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENTITLEMENTS="$REPO_DIR/scripts/standalone.entitlements"

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"

PLUGINS=(
    VocalGrit VocalEss VocalQ VocalKnob VocalAir VocalComp
    Vocal2A VocalTune VocalVerb VocalDoubler VocalDelay
    VocalGate VocalMod VocalBlend VocalChop
)

DEV_ID_APP="${DEV_ID_APP:-}"
NOTARY_PROFILE="${NOTARY_PROFILE:-VocalEssential}"

# --- Helpers ------------------------------------------------------------------
c_blue()  { printf '\033[1;34m%s\033[0m\n' "$*"; }
c_green() { printf '\033[1;32m%s\033[0m\n' "$*"; }
c_red()   { printf '\033[1;31m%s\033[0m\n' "$*" >&2; }

require_identity() {
    if [[ -z "$DEV_ID_APP" ]]; then
        c_red "ERROR: set DEV_ID_APP, e.g.:"
        c_red '  export DEV_ID_APP="Developer ID Application: Your Name (TEAMID)"'
        c_red "Available signing identities:"
        security find-identity -v -p codesigning >&2 || true
        exit 1
    fi
}

# Echo the bundle paths that exist for a given plugin (VST3, AU, Standalone).
bundles_for() {
    local name="$1"
    local vst3="$VST3_DIR/$name.vst3"
    local au="$AU_DIR/$name.component"
    local app="$REPO_DIR/$name/build/${name}_artefacts/Release/Standalone/$name.app"
    [[ -d "$vst3" ]] && echo "$vst3"
    [[ -d "$au"   ]] && echo "$au"
    [[ -d "$app"  ]] && echo "$app"
}

all_bundles() {
    local name
    for name in "${PLUGINS[@]}"; do bundles_for "$name"; done
}

# --- Steps --------------------------------------------------------------------
do_sign() {
    require_identity
    c_blue "==> Code-signing (hardened runtime, secure timestamp)…"
    local b
    while IFS= read -r b; do
        [[ -z "$b" ]] && continue
        local extra=()
        # Standalone apps record audio → sign with entitlements.
        if [[ "$b" == *.app ]]; then
            extra=(--entitlements "$ENTITLEMENTS")
        fi
        echo "    signing: ${b/#$HOME/~}"
        # Note: "${extra[@]+...}" guards against macOS's stock bash 3.2, which
        # errors on expanding an empty array under `set -u`.
        codesign --force --timestamp --options runtime \
                 "${extra[@]+"${extra[@]}"}" \
                 --sign "$DEV_ID_APP" \
                 "$b"
        codesign --verify --strict --verbose=1 "$b"
    done < <(all_bundles)
    c_green "    signing complete."
}

do_notarize() {
    c_blue "==> Notarizing (single batch submission)…"
    local staging zip
    staging="$(mktemp -d)"
    zip="$(mktemp -d)/VocalEssential-notarize.zip"

    local b count=0
    while IFS= read -r b; do
        [[ -z "$b" ]] && continue
        # Flatten name collisions (VocalGrit.vst3 vs .component vs .app) by
        # prefixing each with its format so all coexist in the payload dir.
        local fmt="${b##*.}"
        ditto "$b" "$staging/${fmt}__$(basename "$b")"
        count=$((count + 1))
    done < <(all_bundles)

    if [[ "$count" -eq 0 ]]; then
        c_red "ERROR: no signed bundles found to notarize. Build first."
        exit 1
    fi

    echo "    zipping $count bundles → $zip"
    ditto -c -k --sequesterRsrc --keepParent "$staging" "$zip"

    echo "    submitting to Apple (this can take a few minutes)…"
    xcrun notarytool submit "$zip" \
        --keychain-profile "$NOTARY_PROFILE" \
        --wait

    rm -rf "$staging" "$(dirname "$zip")"
    c_green "    notarization finished. Run 'staple' to attach the tickets."
}

do_staple() {
    c_blue "==> Stapling notarization tickets…"
    local b
    while IFS= read -r b; do
        [[ -z "$b" ]] && continue
        echo "    stapling: ${b/#$HOME/~}"
        xcrun stapler staple "$b"
    done < <(all_bundles)
    c_green "    stapling complete."
}

do_verify() {
    c_blue "==> Verifying signatures + notarization…"
    local b
    while IFS= read -r b; do
        [[ -z "$b" ]] && continue
        echo "  ${b/#$HOME/~}"
        codesign --verify --deep --strict --verbose=2 "$b" 2>&1 | sed 's/^/      /' || true
        xcrun stapler validate "$b"           2>&1 | sed 's/^/      /' || true
        # Gatekeeper assessment: 'exec' policy for plugins/apps loaded at runtime.
        spctl --assess --type execute --verbose=2 "$b" 2>&1 | sed 's/^/      /' || true
    done < <(all_bundles)
    c_green "    verification done."
}

# --- Main ---------------------------------------------------------------------
cmd="${1:-all}"
case "$cmd" in
    sign)     do_sign ;;
    notarize) do_notarize ;;
    staple)   do_staple ;;
    verify)   do_verify ;;
    all)      do_sign; do_notarize; do_staple; do_verify ;;
    *)        c_red "Unknown command: $cmd  (use: sign | notarize | staple | verify | all)"; exit 1 ;;
esac

c_green "Done."
