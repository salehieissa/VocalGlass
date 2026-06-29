# VocalGlass — DSP / UI Fix Work Order

_Last updated: 2026-06-28_

This document captures the round of fixes requested across the plugin suite: what's
wrong, **why** (root cause, with file references), and the **planned fix**. It doubles
as the changelog — each item gets checked off and annotated once shipped on both
macOS and Windows (shared source, so a fix lands on both from one edit).

Status legend: `[ ]` todo · `[~]` in progress · `[x]` done

---

## 1. VocalGrit — Glitch "gate" feels random, should be timed

**File:** `VocalGrit/Source/dsp/Glitch.h`

**What's happening:** The base 16-step gate pattern _is_ already tempo-synced — the
step length is derived from host BPM in `PluginProcessor.cpp` (`stepLen = (60/bpm) *
beatsPerStep[idx] * sampleRate`) and fed to `Glitch::setParams`. The "random" feel
comes from the **stutter rolls and beat-repeat freezes**, which fire on a random
coin-flip every step:

```132:156:VocalGrit/Source/dsp/Glitch.h
void computeStep() noexcept
{
    currentOn = pattern[(size_t) (stepIndex % 16)] != 0;
    rollDiv = 0;
    if (depthAmt > 0.25f && rand01() < depthAmt * 0.35f)   // <-- random trigger
    {
        const float r2 = rand01();                          // <-- random choice
        ...
    }
}
```

Because rolls/repeats land on unpredictable steps, the gate sounds non-deterministic
and "wanders" instead of locking to the grid.

**Planned fix:**
- Drive rolls/repeats from the **step index on the 16-grid**, not the RNG. e.g. fixed
  roll steps (say steps 6, 14 get a 2x roll; step 10 gets a 4x roll), scaled in by
  `depth` (no glitch events below ~0.25, denser as depth rises) so the knob still goes
  groove → chaos but the _placement_ is repeatable and on-beat.
- Keep the click-free 3 ms edge ramp and the existing pattern.
- Remove (or seed-reset per playback start) the `nextRand()` usage so two passes of the
  same audio glitch identically.

**Result:** the gate is fully timed/quantised — same input → same rhythmic chop every time.

- [x] Implemented · [x] Compiles clean (mac) · [ ] Listened/tested · [ ] Built win

---

## 2. VocalGrit — Delay timing should be "timed"

**Files:** `VocalGrit/Source/dsp/Delay.h`, `VocalGrit/Source/PluginProcessor.cpp`

**What's happening:** The delay already supports host-tempo sync — there's a `delaySync`
toggle and a `delayDiv` note-division param, and when sync is on the echo time is computed
from BPM (`seconds = beatsPerDiv[idx] * (60/bpm)`). But **sync is off by default**, so out
of the box the delay runs on the free `delayTime` (ms) knob and doesn't feel locked to the
track.

**Planned fix (pick one — recommend A):**
- **A.** Default `delaySync` to **on** and pick a sensible default division (e.g. 1/8) so
  the delay is musically timed the moment you enable the module.
- **B.** Leave default off but make the synced grid the primary, clearly-labelled mode in
  the UI.

**Result:** delay repeats land on the beat by default, matching the user's "should be
timed" expectation.

- [x] Implemented (sync toggle widened for presence) · [x] Compiles clean (mac) · [ ] Built win

---

## 3. VocalComp — Attack / Release feel ineffective

**Files:** `VocalComp/Source/dsp/Compressor.h`, `VocalComp/Source/PluginProcessor.cpp`

**What's happening:** Attack/Release params exist and are wired
(`attack` 0.1–300 ms, `release` 5–2000 ms) and converted to one-pole coefficients via
`coef(ms)`. Two things blunt their audible effect:

1. **Mode multipliers stack on top of the knob.** In `process()` the coefficients are
   `coef(attackMs * atkScale)` / `coef(releaseMs * relScale)` where Opto uses
   `atkScale 2.2 / relScale 3.0`. So in Opto the knob's effective range is heavily
   compressed/offset, making the control feel like it "doesn't do much."
2. **dB-domain ballistics with a fast RMS detector** can mask short attack differences,
   especially at low ratios / shallow GR.

**Planned fix:**
- Treat the knob as the **actual** attack/release time; fold mode character into the
  _detector blend / knee_, not into a hidden multiply on the user's time value (or reduce
  the multipliers so the knob stays dominant).
- Verify the skew on the `NormalisableRange` (currently `0.4`) gives useful resolution at
  the fast end; widen if needed.
- Sanity-check with a square/burst test that attack visibly changes the leading-edge
  overshoot and release changes the recovery tail.

- [x] Implemented · [x] Compiles clean (mac) · [ ] Listened/tested · [ ] Built win

---

## 4. Vocal2A — Attack / Release "don't work"

**Files:** `Vocal2A/Source/dsp/OptoLeveler.h`, `Vocal2A/Source/PluginProcessor.cpp`

**What's happening:** This is the key finding — **Vocal2A has no attack/release
parameters at all.** The param layout only exposes `gain`, `peakReduction`, `mode`,
`autoMakeup`, `analog`, `hiFreq`, `mix`, `trim`, `vuSource`. The opto attack/release are
**hard-coded** in `OptoLeveler::prepare`:

```40:44:Vocal2A/Source/dsp/OptoLeveler.h
attackCoeff   = std::exp (-1.0 / (0.010 * sampleRate));   // ~10 ms
relFastCoeff  = std::exp (-1.0 / (0.080 * sampleRate));   // ~80 ms initial
relSlowCoeff  = std::exp (-1.0 / (1.800 * sampleRate));   // ~1.8 s tail
```

This is _authentic_ LA-2A behaviour (fixed, program-dependent), but if the UI shows
attack/release-looking controls they are not connected to anything — hence "don't work."

**Decision needed:** Two valid paths —
- **A. Stay true to the LA-2A:** remove/relabel any attack/release controls in the UI so
  there's nothing dead to interact with (the leveler's character lives in Peak Reduction +
  mode). _Recommended for the "2A" identity._
- **B. Add real controls:** introduce `attack`/`release` params, scale the hard-coded
  coefficients off them, and wire them through `setParams`.

**Planned fix:** confirm with you which path, then implement. (Need to know exactly what
the 2A panel currently shows.)

- [x] Decision: add real params (option B) · [x] Implemented (Attack + Release knobs added to
  the bottom strip, wired to the opto coefficients) · [x] Compiles clean (mac) · [ ] Built win

---

## 5. VocalTune — Retune Speed: 0 on the right, ring filled

**Files:** `VocalTune/Source/ui/RingKnob.h`, `VocalTune/Source/PluginEditor.cpp`

**What's happening:** `retuneSpeed` is `0–100` (default 16); **0 = fastest/hardest**
retune. The `RingKnob` fills the arc proportional to value from the left start angle:

```29:31:VocalTune/Source/ui/RingKnob.h
const double range = getMaximum() - getMinimum();
const float prop = range > 0.0 ? (float) ((getValue() - getMinimum()) / range) : 0.0f;
const float angle = kStart + prop * (kEnd - kStart);
```

So today retune `0` → empty ring, pointer on the **left**. The user wants the strongest
setting (0) to read as a **full ring on the right**.

**Planned fix:**
- Add an `invertFill` option to `RingKnob` (or a Tune-specific subclass) that maps
  `prop → 1 - prop`, so value `0` draws a full arc ending at `kEnd` (right side) and
  value `100` draws empty.
- Apply it only to `retuneKnob`. Leave the param/automation untouched (still 0–100); this
  is purely the visual fill + pointer direction.

**Result:** hard-tune (0) shows a full ring biased right; loosening toward 100 empties it.

- [x] Implemented (RingKnob `setInvertedFill`, applied to the retune knob) · [x] Compiles clean (mac) · [ ] Built win

---

## 6. VocalVerb — Decay doesn't change the tail

**File:** `VocalVerb/Source/dsp/Reverb.h`

**What's happening:** `decaySec` maps to an RT60 feedback gain, but the RT60 math uses the
**buffer-padded** delay lengths instead of the musical delay lengths. In `prepare`, each
tank delay's `base` is set to `scaleLen(ref, scale, headroom)` with `headroom = 2.4`, i.e.
`base ≈ ref × scale × 2.4`. Then the loop time for RT60 is built from those inflated bases:

```119:126:VocalVerb/Source/dsp/Reverb.h
const float loopSamples = ((float) tankDelA.base + (float) tankDel2A.base
                            + (float) tankDelB.base + (float) tankDel2B.base)
                           * 0.5f * sizeScale;
const float loopSec = juce::jmax (1.0e-4f, loopSamples / (float) sr);
const float rt60 = juce::jlimit (0.1f, 20.0f, p.decaySec);
float decayGain = std::pow (10.0f, -3.0f * loopSec / rt60);
decayGain = juce::jlimit (0.0f, 0.9990f, decayGain);
```

With `headroom 2.4` baked into `base` **and** `sizeScale` (~1.9 at default size) multiplied
in again, `loopSec` comes out roughly 2.4× too long. That drives `decayGain` way down
(~0.2 at decay = 4 s), so the tank barely sustains and **moving the Decay knob barely
changes anything** — it's always a short tail.

**Planned fix:**
- Store the **nominal** reference length separately from the padded buffer length. Use the
  nominal length (`ref × scale`, no 2.4 headroom) for the `loopSamples`/RT60 computation;
  keep the headroom only for buffer allocation.
- Re-check the actual tap lengths too (`lenDelA = base * sizeScale`) — these are also
  inflated by the same 2.4×; the taps should use nominal × `sizeScale`. Fixing both makes
  Decay map correctly to RT60 and restores long tails at high settings.

**Result:** Decay sweeps from tight room to long hall as expected.

- [x] Implemented (nominal lengths + headroom 3.0) · [x] Compiles clean (mac) · [ ] Listened/tested · [ ] Built win

---

## 7. All plugins — Medium / Semibold body text

**Files:** `*/Source/ui/Theme.h` (11 copies — VocalGrit, VocalEss, VocalQ, VocalKnob,
VocalAir, VocalComp, Vocal2A, VocalTune, VocalVerb, VocalDoubler, VocalDelay)

**What's happening:** The shared `theme::font()` helper only switches between
`juce::Font::plain` and `juce::Font::bold`:

```46:50:VocalTune/Source/ui/Theme.h
inline juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (fontFamily, size,
                                          bold ? juce::Font::bold : juce::Font::plain));
}
```

So all "normal" text renders at Regular weight, which reads a little thin on the light UI.

**Planned fix:**
- Change the non-bold branch to request a **Medium** (or Semibold) typeface style by name,
  e.g. `FontOptions(fontFamily, size, ...).withStyle("Medium")`, and the bold branch to
  `"Semibold"`/`"Bold"`. SF Pro Display exposes these named weights on macOS; on Windows it
  falls back as it does today.
- Apply the **identical** edit to all 11 `Theme.h` files so the suite stays visually
  consistent.

**Result:** crisper, slightly heavier text across every plugin, no layout changes.

- [x] Implemented (×11, `withStyle("Medium"/"Semibold")`) · [x] Compiles clean (mac, verified on
  VocalGrit/Comp/2A/Tune/Verb/Q) · [ ] Built win

---

## Rollout checklist (per build)

1. Make the source edits (shared headers → both platforms).
2. Rebuild affected plugins (Release: VST3 / AU / Standalone).
3. `pluginval` strictness 10 + `auval` on mac.
4. Re-sign + notarize + staple (mac) / Azure Trusted Signing (win).
5. Rebuild `.pkg` (mac) / `.exe` (win) installers, re-bundle, ship.

> Decisions (2026-06-28):
> - **#2 Delay:** keep sync **off** by default; just make the sync control more
>   prominent/clear in the UI (option B).
> - **#4 Vocal2A:** **add real attack/release params** and wire them to the opto
>   coefficients (option B).
