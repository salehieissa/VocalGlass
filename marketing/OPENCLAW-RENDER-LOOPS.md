# HANDOFF → OpenClaw — /vst hero render loops (17 plugins)

Subtle animated 3D render loops for the plugins page. Only the plugin in the
hero plays its loop; picker strip + thumbnails stay stills. One loop per
plugin. You have the UI/3D sources for the first 16; **VocalGeek is new — its
reference package is in `marketing/vocalgeek-refs/` and detailed at the bottom
of this doc.**

Vibe for all 17: premium hardware idling on a shelf, not a car-dealership spin.
Slow, looping, hypnotic — a few degrees of float/drift, a light sweep crossing
the brushed metal, knobs and meters glowing. No camera cuts, no fast rotation,
no text overlays. Shared design language across the suite: frosted white-glass
plate, chrome edge + screws, hot-pink neon accents, pink value arcs around every
knob. Keep the light sweep on the brushed-metal/glass and the glow on the pink
neon unless a plugin's prompt says otherwise (Vocal2A amber, VocalGeek per
cartridge).

The `DELIVERY` block is identical at the bottom of every prompt — copy it
verbatim into each render job so output drops into the site with zero rework.

---

## 1. VocalTune — `vocaltune`

Animate the VocalTune plugin UI as a seamless hero loop.
Signature motion: the unit floats up ~4° and settles back over 7s; on the
pitch display a single note-blob drifts slightly sharp then visibly SNAPS onto
the nearest scale line and holds, repeating once per loop so it reads
"auto-tune / pitch-locked."
Light: a soft specular highlight drifts diagonally across the brushed-metal
faceplate once per loop, catching the chrome edge as it passes.
Live elements: the Retune Speed knob's pink arc pulses faintly on each snap;
the scale-grid lines glow brighter for a beat when the note locks.
[DELIVERY]

## 2. VocalQ — `vocalq`

Animate the VocalQ plugin UI as a seamless hero loop.
Signature motion: the device floats gently ~3°; the EQ curve display starts
with a harsh resonant bump and a boomy low hump, then both smoothly settle
flat into a clean gentle curve, then ease back — a slow "cleaning" breath that
loops seamlessly.
Light: a light sweep glides left-to-right across the glass once per loop,
briefly lighting up the EQ curve as it crosses.
Live elements: the two or three band-node dots glide to their resting
positions; the active band's pink arc glows as the curve smooths.
[DELIVERY]

## 3. VocalEss — `vocaless`

Animate the VocalEss plugin UI as a seamless hero loop.
Signature motion: the unit floats ~3°; on the meter a sharp sibilant "S" spike
flares up, then a de-ess reduction band clamps down over it and the spike
shrinks to a tame level — a duck-and-recover that loops.
Light: a cool specular highlight passes across the faceplate, glinting off the
chrome as the spike is tamed.
Live elements: the Threshold knob's pink arc dips in sympathy with each spike;
the reduction meter breathes down and back.
[DELIVERY]

## 4. VocalComp — `vocalcomp`

Animate the VocalComp plugin UI as a seamless hero loop.
Signature motion: the device floats ~3°; the gain-reduction meter breathes —
needle/segments pull down as an imaginary loud phrase hits, then recover, in a
slow even pulse that reads "leveling / evening out." A gate LED blinks green
between pulses.
Light: a soft highlight sweeps the brushed metal once per loop.
Live elements: GR meter breathes; the Ratio and Threshold knob arcs glow
faintly on each compression pulse; gate LED pulses in the gaps.
[DELIVERY]

## 5. Vocal2A — `vocal2a`

Animate the Vocal2A plugin UI as a seamless hero loop.
Signature motion: the vintage unit floats ~3°; the big VU needle swings gently
and musically left-and-right as if riding a warm vocal, never pinning, always
easing back — slow, hypnotic, analog.
Light: a warm specular highlight drifts across the faceplate; the whole plate
carries a subtle warm amber glow (this unit reads vintage/tube, not pink).
Live elements: the VU needle is the star; the Peak Reduction and Gain knobs
sit still or turn a hair; a soft amber pilot lamp glows and breathes.
[DELIVERY]

## 6. VocalKnob — `vocalknob`

Animate the VocalKnob plugin UI as a seamless hero loop.
Signature motion: the device floats ~3°; the single large central knob rotates
slowly a few degrees up and back, and with it the whole plate gains a subtle
"polished" shimmer — the before/after of one-knob instant polish, looping.
Light: a bright clean highlight sweeps across the glass as the knob turns,
peaking at the top of the turn.
Live elements: the one big knob and its thick pink arc are the focus — arc
fills a touch as the knob turns, eases back; faint sparkle blooms on the plate
at peak.
[DELIVERY]

## 7. VocalAir — `vocalair`

Animate the VocalAir plugin UI as a seamless hero loop.
Signature motion: the unit floats ~3°; a fine shimmer of bright sparkles rises
off the TOP edge of the device and fades, continuously, like airy high-end
lifting off — high-shelf "shine" made visible, looping softly.
Light: a crisp cool-white highlight sweeps the top bezel, feeding the shimmer.
Live elements: the Air/Amount knob arc glows a cool bright tint; tiny specular
sparkles drift upward and dissolve; a high-shelf curve on any mini-display
lifts and settles.
[DELIVERY]

## 8. VocalGrit — `vocalgrit`

Animate the VocalGrit plugin UI as a seamless hero loop.
Signature motion: the device floats ~3° but with attitude — every ~2s a brief
glitch flicker/tear ripples across the faceplate and the plate texture roughens
into a gritty, saturated shimmer, then resolves clean, looping. Keep glitches
short and tasteful, not seizure-y.
Light: an aggressive specular streak snaps across the metal on each glitch
beat, harder-edged than the other plugins.
Live elements: the Drive/Grit knob arc flares hot pink on each glitch;
saturation texture pulses on the plate; a clip/heat LED stutters.
[DELIVERY]

## 9. VocalVerb — `vocalverb`

Animate the VocalVerb plugin UI as a seamless hero loop.
Signature motion: the unit floats ~3°; a soft concentric halo/ripple expands
slowly outward from the center of the plate and fades at the edges, over and
over — clean space blooming without mud, a calm expanding-reverb pulse.
Light: a gentle highlight drifts across the glass, slower than the halo, so the
two never sync harshly.
Live elements: the Decay/Size knob arc glows as each halo is born; a mix/space
readout shimmers faintly with the bloom.
[DELIVERY]

## 10. VocalDelay — `vocaldelay`

Animate the VocalDelay plugin UI as a seamless hero loop.
Signature motion: the device floats ~3°; a bright "tap" pulse appears then
repeats in evenly-spaced echoes that march across the display and fade — each
repeat dimmer, perfectly tempo-spaced, looping so the last echo hands off to
the next tap. Reads "tempo-synced echoes."
Light: a highlight sweep crosses once per loop, timed to land on the first tap.
Live elements: the Time and Feedback knob arcs glow on the initial tap; the
beat/sync LEDs step in time with the echoes.
[DELIVERY]

## 11. VocalDoubler — `vocaldoubler`

Animate the VocalDoubler plugin UI as a seamless hero loop.
Signature motion: the unit floats ~3°; a faint ghost copy of the device (or of
the on-screen waveform) splits left and right into a subtle stereo shimmer,
widening then re-converging to center, looping — width made visible without the
device ever actually breaking apart.
Light: two soft highlights drift symmetrically outward (L/R) then back,
reinforcing the widening.
Live elements: the Width/Detune knob arc glows as the doubles spread; L/R
stereo meters swell outward and settle.
[DELIVERY]

## 12. VocalGate — `vocalgate`

Animate the VocalGate plugin UI as a seamless hero loop.
Signature motion: the device floats ~3°; a gate LED/indicator opens (glows) as
an imaginary line comes in, then snaps shut to silence between lines — a clean
open/close rhythm with a satisfying dark gap, looping.
Light: a highlight sweep crosses only while the gate is OPEN, and the plate
dims slightly when it shuts — light itself gates.
Live elements: the Threshold knob arc glows when open; the gate LED and level
meter drop to black in the closed gap, then re-open.
[DELIVERY]

## 13. VocalClip — `vocalclip`

Animate the VocalClip plugin UI as a seamless hero loop.
Signature motion: the unit floats ~3°; on the transfer-curve display the input
waveform pushes harder into the ceiling and its peaks flatten against a soft
clip line (rounded, not brutal), then relax — loud, in-your-face energy that
swells and eases, looping.
Light: a strong highlight sweeps as the peaks hit the ceiling, punching up the
brightness at max clip.
Live elements: the Drive/Clip knob arc glows hot as peaks flatten; the transfer
curve's knee brightens; an output/ceiling LED holds steady near the top.
[DELIVERY]

## 14. VocalMod — `vocalmod`

Animate the VocalMod plugin UI as a seamless hero loop.
Signature motion: the device floats ~3°; a comb-filter / phaser sweep glides
slowly across the display as swirling notch bands move through, giving a
liquid chorus/flanger "movement & width" feel that loops seamlessly.
Light: the specular highlight itself sways side-to-side in a slow LFO, echoing
the modulation rate.
Live elements: the Rate and Depth knob arcs pulse with the sweep; the modulation
notches drift across any spectrum display; stereo width shimmers.
[DELIVERY]

## 15. VocalChop — `vocalchop`

Animate the VocalChop plugin UI as a seamless hero loop.
Signature motion: the unit floats up ~4° and back over 7s; its live waveform
display scrolls left in a perfect loop, with a few chop-slice markers sweeping
across the beat grid so it reads "rhythmic / tempo-locked."
Light: a soft specular highlight drifts diagonally across the brushed-metal
faceplate once per loop.
Live elements: the Rate and Gate knobs pulse a faint pink glow in time with the
waveform; the beat-grid LEDs step across.
[DELIVERY]

## 16. VocalBlend — `vocalblend`

Animate the VocalBlend plugin UI as a seamless hero loop.
Signature motion: the device floats ~3°; two offset layers/waveforms (a "vocal"
and a "beat") drift apart slightly then glide together and lock into alignment,
settling into the pocket — bus glue made visible, looping.
Light: a single wide highlight sweeps the plate as the two layers lock,
brightening at the moment they align.
Live elements: the Blend/Glue knob arc glows as the layers converge; a
correlation/level meter eases toward center and holds.
[DELIVERY]

---

## DELIVERY block (paste verbatim under EVERY prompt above)

```
DELIVERY (web hero loop — must match exactly):
- 6–8 second SEAMLESS loop (last frame flows into first, no visible cut)
- 720px wide, 24fps, TRANSPARENT background (alpha channel, no backdrop/shadow baked in)
- Export BOTH:
    • HEVC with alpha (hvc1) → <pluginId>.mov   (~0.8–1.5 Mbps)
    • VP9 with alpha (WebM)  → <pluginId>.webm  (~0.8 Mbps)
- First frame MUST match the existing still render (it's used as the poster)
- Motion is subtle — device stays centered, only floats/rotates a few degrees
- No text, no logos, no UI callouts, no captions
```

Filenames (exact plugin ids): vocaltune, vocalq, vocaless, vocalcomp, vocal2a,
vocalknob, vocalair, vocalgrit, vocalverb, vocaldelay, vocaldoubler, vocalgate,
vocalclip, vocalmod, vocalchop, vocalblend, **vocalgeek**.

---

# 17. VocalGeek — `vocalgeek` (NEW — you don't have the sources, so here's everything)

**What it is:** a handheld Game Boy-style "dose console." It's a vocal
performance-FX unit. The faceplate is the same suite design language (frosted
white-glass body, chrome bezel + corner screws, `VocalGeek` wordmark up top)
but shaped like a handheld: a big pixel LCD screen up top, a chrome D-pad
bottom-left, two round candy-jewel "hit" buttons bottom-right (HIT A / HIT B),
two chrome pill switches labeled TAP and PRINT in the middle, a chrome
"cartridge bay" pill above them holding the drug object, and two knobs at the
very bottom (DOSE left, OUTPUT right) sitting either side of a chrome speaker
grille. A small "rec" dot sits left of the screen.

**The hook — cartridges recolor the whole unit.** VocalGeek has 5 swappable
"cartridges," each a different FX chain AND a full color/theme reskin. The
neon accent color, the button color, the cartridge-bay contents, and the pixel
screen scene all change together per cartridge:

| cartridge | accent color | bay object | screen scene |
|---|---|---|---|
| lean | hot pink `#FC22C3` | pink syrup pill/pool | pink wave sagging into heavy syrup drips |
| smoke | green | cannabis nugs | green wave hanging as drips, plumes billowing up |
| acid | orange | sheet of blotter tabs | melting orange→pink→blue rainbow curtain + spirals |
| snow | ice blue | white powder | white mirrored wave, icicles, snow flurry |
| geeked | yellow | pharma pills | glitchy yellow waveform, dropouts/tears |

The pixel screen is a live, code-drawn scene: a reactive waveform in the
theme's color, a header line (e.g. `LEAN . 2OZ`), L/R block meters, a
`TOLERANCE` readout, and when idle two little pixel fighters spar on a ground
line. Treat the screen as a self-animating display.

**Reference stills (in `marketing/vocalgeek-refs/`):**
- `vocalgeek-lean.png` — pink theme (this is the POSTER / first-frame still;
  it's identical to `screenshots/clean/VocalGeek.png`)
- `vocalgeek-smoke.png` — green theme
- `vocalgeek-acid.png` — orange theme
- `vocalgeek-snow.png` — ice-blue theme
- `vocalgeek-geeked.png` — yellow theme
Additional source art if you rebuild the 3D/plate: `assets/ui/geek-*.png`
(chassis on/off plates, per-theme bay + hit-button sprites, D-pad) and
`assets/ui/_refs/geek-reference.png`.

### Prompt

Animate the VocalGeek plugin UI as a seamless hero loop.
Signature motion: the handheld floats up ~4° and back over 7s. Its pixel LCD
is alive — the theme-colored waveform scrolls/reacts continuously and loops
perfectly, and the two idle pixel fighters trade one slow punch per loop. Once
per loop the cartridge "doses": the HIT A / HIT B jewel buttons press-and-glow
in a quick one-two, and a faint scan ripples down the LCD — reads
"performance / playable," not busy.
Light: a soft specular highlight drifts diagonally across the white-glass body
and glints along the chrome bezel and D-pad once per loop.
Live elements: the DOSE and OUTPUT knob neon arcs (theme color) pulse gently
with the on-screen wave; the HIT A/B buttons bloom on the one-two; the LCD's
L/R block meters bounce; the `rec` dot breathes.
Color: match the poster still — **lean / hot-pink `#FC22C3`** is the default
hero theme. (If the site wants theme variants later, re-render per cartridge
using the matching still above; the accent color, button color, bay object and
screen scene all change together — keep everything else identical.)

```
DELIVERY (web hero loop — must match exactly):
- 6–8 second SEAMLESS loop (last frame flows into first, no visible cut)
- 720px wide, 24fps, TRANSPARENT background (alpha channel, no backdrop/shadow baked in)
- Export BOTH:
    • HEVC with alpha (hvc1) → vocalgeek.mov   (~0.8–1.5 Mbps)
    • VP9 with alpha (WebM)  → vocalgeek.webm  (~0.8 Mbps)
- First frame MUST match the existing still render (marketing/vocalgeek-refs/vocalgeek-lean.png — used as the poster)
- Motion is subtle — device stays centered, only floats/rotates a few degrees
- No text, no logos, no UI callouts, no captions
```
