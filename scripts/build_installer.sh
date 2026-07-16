#!/usr/bin/env bash
#
# build_installer.sh — build signed, notarized macOS .pkg installers for the
# VocalEssential plugins: one "suite" installer (all 11) plus one installer per
# plugin (for individual sales).
#
# Each installer drops the three formats into the standard system locations:
#     VST3       → /Library/Audio/Plug-Ins/VST3
#     AU         → /Library/Audio/Plug-Ins/Components
#     Standalone → /Applications
# (system-wide install → the installer asks for an admin password, exactly like
#  every commercial plugin installer.)
#
# ── One-time setup ───────────────────────────────────────────────────────────
#   1. Run scripts/sign_and_notarize.sh FIRST so the bundles are signed +
#      notarized + stapled. The installer just packages those signed bundles.
#   2. You need a "Developer ID Installer" certificate (DIFFERENT from the
#      "Developer ID Application" cert used to sign the bundles — both come from
#      the same Apple account, free to create in Xcode → Settings → Accounts →
#      Manage Certificates → +, or at developer.apple.com). Check with:
#         security find-identity -v
#      and look for a line containing: Developer ID Installer: <Your Name> (TEAMID)
#   3. The same notarytool keychain profile from the signing script is reused.
#
# ── Usage ────────────────────────────────────────────────────────────────────
#   export DEV_ID_INSTALLER="Developer ID Installer: Eissa Salehi (9FNAY8338F)"
#   export NOTARY_PROFILE="VocalEssential"
#   ./scripts/build_installer.sh all          # build → notarize → staple → verify
#
#   Sub-commands: build | notarize | staple | verify | all   (default: all)
#   (If DEV_ID_INSTALLER is unset it is auto-detected from your keychain.)
#
# Override the version with:  VERSION=1.0.1 ./scripts/build_installer.sh all
# ─────────────────────────────────────────────────────────────────────────────

set -euo pipefail

# --- Config -------------------------------------------------------------------
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"

OUT_DIR="$REPO_DIR/dist"
COMP_DIR="$OUT_DIR/components"          # intermediate per-plugin component pkgs
WORK_DIR="$OUT_DIR/.work"               # staging roots + synthesized dist xml

VERSION="${VERSION:-1.0.0}"
PKG_PREFIX="com.vocalessential"
SUITE_NAME="VocalEssential-Suite"

PLUGINS=(
    VocalGrit VocalEss VocalQ VocalKnob VocalAir VocalComp
    Vocal2A VocalTune VocalVerb VocalDoubler VocalDelay
    VocalGate VocalMod VocalBlend VocalChop VocalClip VocalGeek
)

NOTARY_PROFILE="${NOTARY_PROFILE:-VocalEssential}"
DEV_ID_INSTALLER="${DEV_ID_INSTALLER:-}"

# --- Helpers ------------------------------------------------------------------
c_blue()  { printf '\033[1;34m%s\033[0m\n' "$*"; }
c_green() { printf '\033[1;32m%s\033[0m\n' "$*"; }
c_red()   { printf '\033[1;31m%s\033[0m\n' "$*" >&2; }

# Locate the Developer ID Installer identity (auto-detect if not provided).
resolve_installer_identity() {
    if [[ -n "$DEV_ID_INSTALLER" ]]; then return; fi
    DEV_ID_INSTALLER="$(security find-identity -v 2>/dev/null \
        | sed -n 's/.*"\(Developer ID Installer: [^"]*\)".*/\1/p' | head -1)"
    if [[ -z "$DEV_ID_INSTALLER" ]]; then
        c_red "ERROR: no 'Developer ID Installer' certificate found."
        c_red "Create one (free) in Xcode → Settings → Accounts → Manage"
        c_red "Certificates → + → 'Developer ID Installer', then re-run."
        c_red "Available identities:"
        security find-identity -v >&2 || true
        exit 1
    fi
    c_blue "    using installer identity: $DEV_ID_INSTALLER"
}

# Bundle source paths (only the ones that exist are used).
vst3_for() { echo "$VST3_DIR/$1.vst3"; }
au_for()   { echo "$AU_DIR/$1.component"; }
app_for()  { echo "$REPO_DIR/$1/build/${1}_artefacts/Release/Standalone/$1.app"; }

# Stage a single plugin's bundles into a payload root that mirrors the install
# layout, then echo the root path.
stage_plugin() {
    local name="$1"
    local root="$WORK_DIR/$name/root"
    rm -rf "$root"; mkdir -p "$root"

    local vst3 au app
    vst3="$(vst3_for "$name")"; au="$(au_for "$name")"; app="$(app_for "$name")"

    if [[ -d "$vst3" ]]; then
        mkdir -p "$root/Library/Audio/Plug-Ins/VST3"
        ditto "$vst3" "$root/Library/Audio/Plug-Ins/VST3/$(basename "$vst3")"
    fi
    if [[ -d "$au" ]]; then
        mkdir -p "$root/Library/Audio/Plug-Ins/Components"
        ditto "$au" "$root/Library/Audio/Plug-Ins/Components/$(basename "$au")"
    fi
    if [[ -d "$app" ]]; then
        mkdir -p "$root/Applications"
        ditto "$app" "$root/Applications/$(basename "$app")"
    fi

    echo "$root"
}

# Build a component .pkg for one plugin (bundle relocation disabled so files
# always land in the locations above, never on top of an old copy elsewhere).
build_component_pkg() {
    local name="$1"
    local root plist out
    root="$(stage_plugin "$name")"
    out="$COMP_DIR/$name.pkg"
    plist="$WORK_DIR/$name/component.plist"

    if [[ -z "$(/bin/ls -A "$root" 2>/dev/null)" ]]; then
        c_red "    WARNING: no bundles found for $name — skipping."
        return 1
    fi

    pkgbuild --analyze --root "$root" "$plist" >/dev/null
    /usr/bin/python3 - "$plist" <<'PY'
import sys, plistlib
path = sys.argv[1]
with open(path, "rb") as f:
    comps = plistlib.load(f)
for c in comps:
    c["BundleIsRelocatable"]      = False
    c["BundleHasStrictIdentifier"] = True
    c["BundleOverwriteAction"]    = "upgrade"
with open(path, "wb") as f:
    plistlib.dump(comps, f)
PY

    pkgbuild --root "$root" \
             --component-plist "$plist" \
             --identifier "$PKG_PREFIX.$name" \
             --version "$VERSION" \
             --install-location "/" \
             "$out"
    echo "    built component: dist/components/$name.pkg"
    return 0
}

# Wrap one or more component pkgs into a signed product installer.
#   $1 = output pkg path   $2 = installer title   $3.. = plugin names to include
build_product_pkg() {
    local out="$1"; shift
    local title="$1"; shift
    local names=("$@")

    local dist="$WORK_DIR/$(basename "$out" .pkg).dist.xml"
    local pkg_args=()
    local n
    for n in "${names[@]}"; do
        [[ -f "$COMP_DIR/$n.pkg" ]] && pkg_args+=(--package "$COMP_DIR/$n.pkg")
    done

    productbuild --synthesize "${pkg_args[@]}" "$dist"

    # Inject a friendly title + background-free polish into the synthesized XML.
    /usr/bin/sed -i '' "s#<installer-gui-script\(.*\)>#<installer-gui-script\1>\n    <title>$title</title>#" "$dist"

    local sign_args=()
    if [[ -n "$DEV_ID_INSTALLER" ]]; then
        sign_args=(--sign "$DEV_ID_INSTALLER")
    fi

    # "${sign_args[@]+...}" guards macOS's stock bash 3.2 (errors on expanding
    # an empty array under `set -u`).
    productbuild --distribution "$dist" \
                 --package-path "$COMP_DIR" \
                 "${sign_args[@]+"${sign_args[@]}"}" \
                 "$out"
    echo "    built installer: ${out/#$REPO_DIR\//}"
}

# Every product installer we produce (suite + per-plugin).
product_pkgs() {
    echo "$OUT_DIR/$SUITE_NAME-$VERSION.pkg"
    local n
    for n in "${PLUGINS[@]}"; do
        [[ -f "$COMP_DIR/$n.pkg" ]] && echo "$OUT_DIR/$n-$VERSION.pkg"
    done
}

# --- Steps --------------------------------------------------------------------
do_build() {
    resolve_installer_identity
    c_blue "==> Building installers (version $VERSION)…"
    rm -rf "$OUT_DIR"; mkdir -p "$COMP_DIR" "$WORK_DIR"

    c_blue "  -- component packages --"
    local built=()
    local n
    for n in "${PLUGINS[@]}"; do
        if build_component_pkg "$n"; then built+=("$n"); fi
    done

    if [[ "${#built[@]}" -eq 0 ]]; then
        c_red "ERROR: no plugins staged. Build the plugins (Release) + run"
        c_red "scripts/sign_and_notarize.sh first."
        exit 1
    fi

    c_blue "  -- suite installer (all ${#built[@]}) --"
    build_product_pkg "$OUT_DIR/$SUITE_NAME-$VERSION.pkg" "VocalEssential Suite" "${built[@]}"

    c_blue "  -- per-plugin installers --"
    for n in "${built[@]}"; do
        build_product_pkg "$OUT_DIR/$n-$VERSION.pkg" "$n" "$n"
    done

    # Component pkgs are intermediates; keep them out of the shipped folder.
    c_green "    build complete. Installers in: dist/"
}

do_notarize() {
    c_blue "==> Notarizing installers…"
    local p
    while IFS= read -r p; do
        [[ -f "$p" ]] || continue
        echo "    submitting: ${p/#$REPO_DIR\//}"
        xcrun notarytool submit "$p" \
            --keychain-profile "$NOTARY_PROFILE" \
            --wait
    done < <(product_pkgs)
    c_green "    notarization finished."
}

do_staple() {
    c_blue "==> Stapling tickets to installers…"
    local p
    while IFS= read -r p; do
        [[ -f "$p" ]] || continue
        echo "    stapling: ${p/#$REPO_DIR\//}"
        xcrun stapler staple "$p"
    done < <(product_pkgs)
    c_green "    stapling complete."
}

do_verify() {
    c_blue "==> Verifying installers…"
    local p
    while IFS= read -r p; do
        [[ -f "$p" ]] || continue
        echo "  ${p/#$REPO_DIR\//}"
        pkgutil --check-signature "$p" 2>&1 | sed 's/^/      /' || true
        xcrun stapler validate "$p"   2>&1 | sed 's/^/      /' || true
        spctl --assess --type install --verbose=2 "$p" 2>&1 | sed 's/^/      /' || true
    done < <(product_pkgs)
    c_green "    verification done."
}

# --- Main ---------------------------------------------------------------------
cmd="${1:-all}"
case "$cmd" in
    build)    do_build ;;
    notarize) do_notarize ;;
    staple)   do_staple ;;
    verify)   do_verify ;;
    all)      do_build; do_notarize; do_staple; do_verify ;;
    *)        c_red "Unknown command: $cmd  (use: build | notarize | staple | verify | all)"; exit 1 ;;
esac

c_green "Done. Ship the .pkg files in: $OUT_DIR"
