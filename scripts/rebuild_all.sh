#!/usr/bin/env bash
# rebuild_all.sh — clean Release rebuild of every VocalEssential plugin
# (VST3 + AU + Standalone). VocalRack is intentionally absent.
set -uo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PLUGINS=(
    VocalGrit VocalEss VocalQ VocalKnob VocalAir VocalComp
    Vocal2A VocalTune VocalVerb VocalDoubler VocalDelay
    VocalGate VocalMod VocalBlend VocalChop VocalClip
)

JOBS="$(sysctl -n hw.ncpu)"
FAILED=()

for p in "${PLUGINS[@]}"; do
    echo "=== [$p] configure ==="
    if ! cmake -S "$REPO_DIR/$p" -B "$REPO_DIR/$p/build" \
         -DCMAKE_BUILD_TYPE=Release > "$REPO_DIR/$p/build-configure.log" 2>&1; then
        echo "!!! [$p] CONFIGURE FAILED"; FAILED+=("$p"); continue
    fi
    echo "=== [$p] build ==="
    if ! cmake --build "$REPO_DIR/$p/build" --config Release -j "$JOBS" \
         > "$REPO_DIR/$p/build-compile.log" 2>&1; then
        echo "!!! [$p] BUILD FAILED"; FAILED+=("$p"); continue
    fi
    echo "=== [$p] OK ==="
done

if [[ ${#FAILED[@]} -gt 0 ]]; then
    echo "REBUILD-RESULT: FAILED: ${FAILED[*]}"
    exit 1
fi
echo "REBUILD-RESULT: ALL-OK"
