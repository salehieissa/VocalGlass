#!/usr/bin/env bash
#
# package_zip.sh — bundle the already-signed + notarized + stapled plugins into
# distributable .zip files (no extra Apple certificate required).
#
# Produces, in dist-zip/:
#   VocalEssential-Suite.zip   → all 11 plugins (VST3 + AU + Standalone) + INSTALL.txt
#   <Plugin>.zip               → one per plugin, for individual sales
#
# Because each bundle is already notarized + stapled, the ticket travels inside
# the bundle, so Gatekeeper accepts them even when delivered as a plain zip.
#
# Usage:  ./scripts/package_zip.sh
# ─────────────────────────────────────────────────────────────────────────────

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
OUT_DIR="$REPO_DIR/dist-zip"
WORK_DIR="$OUT_DIR/.work"

PLUGINS=(
    VocalGrit VocalEss VocalQ VocalKnob VocalAir VocalComp
    Vocal2A VocalTune VocalVerb VocalDoubler VocalDelay
    VocalGate VocalMod VocalBlend VocalChop VocalClip VocalGeek
)

c_blue()  { printf '\033[1;34m%s\033[0m\n' "$*"; }
c_green() { printf '\033[1;32m%s\033[0m\n' "$*"; }

vst3_for() { echo "$VST3_DIR/$1.vst3"; }
au_for()   { echo "$AU_DIR/$1.component"; }
app_for()  { echo "$REPO_DIR/$1/build/${1}_artefacts/Release/Standalone/$1.app"; }

write_install_txt() {
    cat > "$1/INSTALL.txt" <<'TXT'
VocalEssential — Installation (macOS)

Copy the files into these folders, then restart your DAW and rescan plugins:

  • VST3 plugins (.vst3)  →  /Library/Audio/Plug-Ins/VST3
  • AU plugins  (.component, for Logic/GarageBand)  →  /Library/Audio/Plug-Ins/Components
  • Standalone apps (.app)  →  /Applications   (optional — only if you want to
                                                run the plugin without a DAW)

Tip: in Finder, press Cmd+Shift+G and paste the folder path above to open it.
You may be asked for your password the first time you add files to /Library.

All plugins are signed and notarized by Apple, so they'll load without security
warnings.
TXT
}

# Stage one plugin's bundles into a folder, organised by format.
stage_plugin() {
    local name="$1" dest="$2"
    local vst3 au app
    vst3="$(vst3_for "$name")"; au="$(au_for "$name")"; app="$(app_for "$name")"
    [[ -d "$vst3" ]] && { mkdir -p "$dest/VST3";         ditto "$vst3" "$dest/VST3/$(basename "$vst3")"; }
    [[ -d "$au"   ]] && { mkdir -p "$dest/Components";   ditto "$au"   "$dest/Components/$(basename "$au")"; }
    [[ -d "$app"  ]] && { mkdir -p "$dest/Applications"; ditto "$app"  "$dest/Applications/$(basename "$app")"; }
}

main() {
    rm -rf "$OUT_DIR"; mkdir -p "$WORK_DIR"

    # --- Suite zip (all plugins together) -------------------------------------
    c_blue "==> Packaging suite zip…"
    local suite="$WORK_DIR/VocalEssential-Suite"
    mkdir -p "$suite"
    local n
    for n in "${PLUGINS[@]}"; do stage_plugin "$n" "$suite"; done
    write_install_txt "$suite"
    ( cd "$WORK_DIR" && ditto -c -k --sequesterRsrc --keepParent \
        "VocalEssential-Suite" "$OUT_DIR/VocalEssential-Suite.zip" )
    c_green "    dist-zip/VocalEssential-Suite.zip"

    # --- Per-plugin zips ------------------------------------------------------
    c_blue "==> Packaging per-plugin zips…"
    for n in "${PLUGINS[@]}"; do
        local d="$WORK_DIR/$n"
        mkdir -p "$d"
        stage_plugin "$n" "$d"
        if [[ -z "$(/bin/ls -A "$d" 2>/dev/null)" ]]; then
            echo "    (skip $n — no bundles found)"; continue
        fi
        write_install_txt "$d"
        ( cd "$WORK_DIR" && ditto -c -k --sequesterRsrc --keepParent "$n" "$OUT_DIR/$n.zip" )
        echo "    dist-zip/$n.zip"
    done

    rm -rf "$WORK_DIR"
    c_green "Done. Sellable zips are in: $OUT_DIR"
}

main
