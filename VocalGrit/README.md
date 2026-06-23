# VocalGrit

A low-latency vocal **texture** plugin — VST3 + AU + Standalone — built with [JUCE](https://juce.com).
Light / hot-pink UI with a big GRIT dial, level meters, a preset browser, and a
grit chain plus three downstream effects.

**GRIT (+ texture stages) → DOUBLER → DELAY → REVERB**

## UI

- **GRIT dial** — the big ring; overall bite (drives the saturation harder).
- **drive / tone / width** sliders — extra grit harmonics, post tone roll-off, and stereo width.
- **Character pills** — `clean` / `warm` / `dirty` / `blown` waveshaping curves.
- **Texture pills** — real DSP stages: `fuzz` (harder asymmetric stage), `amp` (low-mid
  warmth shelf), `speaker` (guitar-cab band limit), `presence` (high-shelf air).
- **Right card** — live `input` / `output` level meters and a raw↔processed `mix` slider.
- **Preset browser** — header arrows cycle: underground lead, clean double, warm tape,
  blown out, wide ambient.
- **DOUBLER / DELAY / REVERB** — cards along the bottom, each with an `on` toggle.

## Modules & controls

### Grit (distortion / saturation / fuzz)
Oversampled (4×) so it stays clean instead of harsh-aliased.

| Control       | What it does                                                        |
|---------------|---------------------------------------------------------------------|
| **GRIT dial** | Overall bite — input drive into the curve.                          |
| **drive**     | Extra asymmetry/bias → grittier harmonics.                          |
| **tone**      | Low-pass after distortion to tame harsh fizz.                       |
| **width**     | Stereo width (mid/side): 0 = mono, 1 = normal, 2 = wide.            |
| **character** | The curve: `clean` / `warm` / `dirty` / `blown`.                    |
| **mix**       | Raw vocal ↔ processed vocal blend.                                  |
| **pills**     | `fuzz`, `amp`, `speaker`, `presence` — extra texture stages.        |

### Doubler (ADT)
Artificial double-tracking — adds modulated, panned copies so one vocal sounds like two.

| Knob       | What it does                                            |
|------------|---------------------------------------------------------|
| **Detune** | Depth of the pitch wobble (how "separate" the copies feel). |
| **Width**  | Stereo spread of the doubled voices.                    |
| **Mix**    | How much doubling is added.                             |

### Delay (echo)
Feedback echo with repeats that darken over time (analog-ish).

| Knob         | What it does                          |
|--------------|---------------------------------------|
| **Time**     | Echo time (20–1000 ms).               |
| **Feedback** | How many repeats (0–0.95).            |
| **Mix**      | Dry/wet blend.                        |

### Reverb (space)
Freeverb-style reverb.

| Knob     | What it does                       |
|----------|------------------------------------|
| **Size** | Room size / decay.                 |
| **Damp** | High-frequency damping.            |
| **Mix**  | Dry/wet blend.                     |

### Global
| Knob       | What it does                        |
|------------|-------------------------------------|
| **Output** | Final makeup / trim gain (-24 to +6 dB). |

## Why it's "low latency"

Distortion is *memoryless* — each sample is shaped independently, so the core
adds **zero** latency. The only latency is from the 4× oversampling filters
(a fraction of a millisecond), and that amount is reported to the host via
`setLatencySamples()` so your DAW compensates automatically.

## Build

You need **CMake 3.22+** and a C++20 compiler. JUCE is downloaded automatically
on the first configure (needs internet once).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j8
```

The build auto-installs to your user plugin folders (`COPY_PLUGIN_AFTER_BUILD`):

- **VST3** → `~/Library/Audio/Plug-Ins/VST3/VocalGrit.vst3`
- **AU**   → `~/Library/Audio/Plug-Ins/Components/VocalGrit.component`
- **Standalone** → `build/VocalGrit_artefacts/Release/Standalone/VocalGrit.app`

## Try it fast

The standalone app runs without a DAW — open it, pick your audio input/output
in its settings, and turn Drive up:

```bash
open build/VocalGrit_artefacts/Release/Standalone/VocalGrit.app
```

In a DAW (Ableton, Logic, Reaper…), rescan plugins and look for **VocalGrit**
under manufacturer "YourName".

## Where to tweak

- **Grit DSP**: `Source/PluginProcessor.cpp` — `shape()` (curves) and `processBlock()` (chain order).
- **Effect modules**: `Source/dsp/` — `Doubler.h`, `Delay.h`, `ReverbModule.h` (each is a small self-contained class).
- **UI**: `Source/PluginEditor.cpp` — section layout, knobs, colours.
- **Parameters**: `createParameterLayout()` in `PluginProcessor.cpp`.

## Ideas for next steps

- Auto gain compensation on Drive so volume stays steady.
- Ping-pong mode for the delay; tempo sync to the host.
- More grit curves (diode clipper, bit crusher, wavefolder).
- A live transfer-curve / waveform display in the editor.
- De-esser or a one-knob "telephone" lo-fi band-pass.
