# VocalGlass — Photoreal Asset Board

One shared photoreal kit for all 11 plugins + a few per-plugin signature pieces.
Same white + hot-pink theme, rendered in real material.

## Material kit (locked) — *glass · chrome · neon*
> **Frosted white glass** surfaces · **polished chrome** accents/bevels · **dark
> smoked-glass** screens · **neon hot-pink `#ec0f8f`** light. Soft top lighting.
> Clean, premium, modern. Every asset must read like it's from this same kit.

## The rule
If code can draw it, code draws it. **Assets carry photoreal *material* only.**
Geometry, layout, arcs, numbers, **shadows, glows, and motion** stay in code
(consistent across all 11). So: **no baked shadows/glows, no text** in any asset.

## Workflow
GPT generates → refine in Photoshop → drop into `assets/ui/` with the exact
filename → I wire it into all 11. Every asset is **optional** (missing = current
vector look), so roll them in one at a time.

## Global rules (every asset)
- Transparent PNG, sRGB, 8-bit, exported at the **@2x size** listed.
- Soft **top-down** light. No baked drop shadow, no glow, no text/numbers.
- Cutout tight to the object; centered.
- Transparency trigger: **"die-cut sticker, isolated PNG cutout, fully transparent
  alpha background, NO backdrop."** Regenerate if it adds a background.
- Palette: glass white `#fdfdff` · accent `#ec0f8f` (hi `#f64bad`, lo `#cc0a7c`) ·
  ink `#17171c` · soft ink `#8b8b96` · screen text `#e9ecf2`.

---

# CORE KIT (shared by all 11)

### `knob.png` — 300×300 — *rotated in code*
Glossy white-glass knob with a chrome ring/bevel and a **pink indicator notch at
the top (12 o'clock)**. Highlight soft + centered so it reads right when spun.
```
A photorealistic glossy white glass control knob with a polished chrome ring and bevel, perfectly circular, gently domed, top-lit with soft realistic reflections, and a single small neon hot-pink (#ec0f8f) indicator notch at the very top pointing straight up. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text, no numbers. One object only, centered, head-on. Square image.
```

### `knob-large.png` — 440×440 — *rotated in code* (hero knobs)
Bigger, more detailed version of `knob` for the main/hero controls — richer
chrome bevel + glass reflections. Pink notch at 12 o'clock.
```
A photorealistic large glossy white glass control knob with a wide polished chrome ring and bevel, perfectly circular, gently domed, top-lit with rich realistic reflections and a soft specular highlight, and a single small neon hot-pink (#ec0f8f) indicator notch at the very top pointing straight up. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text, no numbers. One object only, centered, head-on. Square image.
```

### `knob-small.png` — 220×220 — *rotated in code* (utility knobs)
Compact, slightly simpler knob for the small utility rows — reads cleanly at
small sizes. Pink notch at 12 o'clock.
```
A photorealistic small glossy white glass control knob with a thin polished chrome ring, perfectly circular, gently domed, clean top-lit reflection, minimal detail so it stays crisp when small, and a single small neon hot-pink (#ec0f8f) indicator notch at the very top pointing straight up. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text, no numbers. One object only, centered, head-on. Square image.
```

### `faceplate.png` — 600×600 — *9-slice (170)*
```
A photorealistic frosted white glass panel slab, rounded rectangle with large soft corners and a thin polished chrome edge, subtle translucency and gentle internal depth, a faint soft light catch along the top. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text, empty inside. One object only, centered, head-on. Square image.
```

### `btn-off.png` — 360×140 — *9-slice (70)*
```
A photorealistic glossy clear-white glass pill-shaped button with a thin chrome rim, fully rounded ends, soft top reflection, faint bevel. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text. One object only, centered, head-on. Wide landscape capsule.
```

### `btn-on.png` — 360×140 — *9-slice (70)* — match btn-off shape
```
A photorealistic neon hot-pink glowing glass pill-shaped button with a thin chrome rim, fully rounded ends, vertical gradient #f64bad to #cc0a7c, wet glossy top highlight, subtle internal light. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text. One object only, centered, head-on. Wide landscape capsule, same shape as btn-off.
```

### `screen.png` — 600×320 — *9-slice (60)*
```
A photorealistic dark smoked-glass display screen, shallow recessed rounded rectangle with a thin chrome bezel, subtle inner shadow so it looks inset, a faint diagonal glossy reflection across the top. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no text, no contents. One object only, centered, head-on. Landscape.
```

### `fader-cap.png` — 120×120 — *sprite*
```
A photorealistic small glossy white glass fader cap / handle with a thin chrome edge and a single neon hot-pink center line, top-lit reflection. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text. One object only, centered, head-on. Square.
```

### `fader-track.png` — 360×80 — *9-slice (40)*
```
A photorealistic recessed glass channel/groove for a slider, dark smoked glass, thin chrome rim, inner shadow at the top so it looks carved in, fully rounded ends. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no text. One object only, centered, head-on. Wide landscape.
```

### `texture.png` — 1024×1024 — *tiled overlay ~6%* (opaque grayscale)
```
A seamless tileable subtle frosted-glass micro-texture, very fine even grayscale noise with faint soft mottling, extremely low contrast, neutral medium gray, no objects, no text, flat, for a low-opacity overlay. Square, high resolution.
```

### `glow.png` — 256×256 — *sprite* (optional; code can do glows)
```
A soft circular radial glow, neon hot pink #ec0f8f at the center fading smoothly to pure black at the edges, gaussian falloff, perfectly smooth, no banding, no hard edge. Solid black background, centered. No text, no shapes.
```

---

# PER-PLUGIN SIGNATURES

### VocalTune · `tune-keyboard.png` — 360×360 — *one octave, tiled in code*
One octave of glass piano keys (C–B, white + black), chrome edges; lit-key
highlight is drawn in code.
```
A photorealistic one-octave piano keyboard strip, white glass keys with thin chrome edges and small glossy black glass sharp keys, top-lit with soft reflections, seven white keys and five black keys, evenly spaced, front-on flat view. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text, no labels. One keyboard only, centered. Wide landscape.
```

### Vocal2A · `2a-vu-face.png` — 600×400 — *sprite (rectangle)*
Rectangular white-glass VU faceplate — printed scale + numbers baked in (the one
asset where numbers are allowed, since the VU scale is fixed). No needle (code swings it).
```
A photorealistic analog VU meter face on a rectangular panel of frosted pure white glass with a thin chrome edge, a printed curved VU scale arc across the upper half with fine tick marks and small printed numbers (-20 -10 -7 -5 -3 0 +3) and a red zone past 0, "VU" printed small below the arc, vintage studio meter look, no needle. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow. One rectangular plate only, centered, head-on. Wide landscape rectangle.
```

### Vocal2A · `2a-vu-needle.png` — 60×420 — *pivots at bottom in code*
A thin needle drawn **pointing straight up**, pivot at the bottom-center.
```
A single thin tapered analog VU meter needle, deep red with a tiny polished chrome hub at the base, pointing straight up, pivot at the bottom. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text. One needle only, vertical, centered. Tall narrow image.
```

### VocalAir · `air-grille.png` — 512×512 — *tiled overlay ~50%*
```
A seamless tileable photorealistic fine perforated metal speaker-grille mesh, tiny round holes in a regular grid, soft chrome highlights, on a transparent background between the holes if possible, low contrast, flat top-down. No text. Square, high resolution.
```

### VocalGrit · `grit-wear.png` — 1024×1024 — *tiled overlay ~10%* (opaque grayscale)
```
A seamless tileable grunge wear overlay, subtle fine scratches, scuffs and dust on a neutral gray field, grayscale, low-to-medium contrast, for a low-opacity overlay to add wear. No objects, no text, flat. Square, high resolution.
```

> Comp, Q, Verb, Delay, Doubler reuse the shared `screen` for their visualizers —
> no extra assets needed.

---

## How code uses it
9-slice drawer (faceplate/buttons/screen/track), image **rotate** (knob) and
**pivot** (needle), **tile** (keyboard/grille/wear/texture), and code keeps doing
shadows + glows + arcs + numbers + the live meter/curve motion. Missing asset →
vector fallback. Layout unchanged.

## Suggested order
1. `knob`, `faceplate`, `btn-on/off` (core look) →
2. `screen` (all the display plugins) →
3. signatures (`2a-vu-*`, `tune-keyboard`, `air-grille`, `grit-wear`) →
4. `texture`, faders, `glow` (polish).

## Drop-in checklist
- [ ] Exact filename in `assets/ui/`, transparent PNG, @2x size
- [ ] Material only — no baked shadow/glow, no text
- [ ] `knob` indicator points up; `2a-vu-needle` points up (pivot at bottom)
- [ ] `btn-off` and `btn-on` identical shape/size
- [ ] 9-slice: corners/material inside the inset region

---

# WAVE 2 — extra hardware, overlays & signatures

Same rules as above (transparent PNG, @2x, **material only**, no baked
shadow/glow, no text) unless a prompt says otherwise. All optional → vector
fallback. Drop into `assets/ui/` with the exact filename.

## Shared hardware (reused across all 11)

### `toggle-off.png` — 200×280 — *sprite* — chrome flip switch, lever down
```
A photorealistic miniature chrome bat-handle toggle switch, polished metal lever tilted to the OFF (down) position, seated in a small round chrome base collar, glass-white surround, top-lit with crisp metallic reflections. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text. One switch only, centered, head-on. Tall portrait image.
```

### `toggle-on.png` — 200×280 — *sprite* — same switch, lever up, pink-lit
```
A photorealistic miniature chrome bat-handle toggle switch flipped to the ON (up) position, polished metal lever, round chrome base collar with a faint neon hot-pink (#ec0f8f) light ring at its base, glass-white surround, crisp metallic reflections. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text. One switch only, centered, head-on, same size and framing as toggle-off. Tall portrait image.
```

### `led-off.png` — 120×120 — *sprite* — dark glass dome
```
A photorealistic small round glass LED indicator dome, unlit, dark translucent glass with a thin chrome rim and a soft top highlight, looks like a tiny inset jewel. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text. One dome only, centered, head-on. Square.
```

### `led-on.png` — 120×120 — *sprite* — same dome, glowing pink
```
A photorealistic small round glass LED indicator dome, brightly lit neon hot-pink (#ec0f8f), glowing translucent glass with a hot bright center, thin chrome rim, glossy top highlight. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text. One dome only, centered, head-on, same size as led-off. Square.
```

### `screw.png` — 80×80 — *sprite* — corner hardware
```
A photorealistic small polished chrome Phillips-head screw seen straight from above, slightly recessed in a tiny chrome washer, crisp metallic reflection, neutral cross slot. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text. One screw only, centered, head-on. Square.
```

### `frame.png` — 600×400 — *9-slice (60)* — chrome bezel around meters/screens
```
A photorealistic empty polished chrome bezel frame, rounded-rectangle ring only with a hollow transparent center, brushed-then-polished metal with soft top-lit reflections and a thin inner and outer edge. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text, hollow middle. One frame only, centered, head-on. Landscape.
```

### `nameplate.png` — 600×160 — *9-slice (80)* — engraved metal strip (blank)
```
A photorealistic brushed aluminum nameplate strip, rounded-rectangle plaque with two tiny chrome rivets near the short ends, fine horizontal brushed grain, soft top-lit sheen, blank engravable surface. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text, blank face. One plate only, centered, head-on. Wide landscape.
```

### `seg-off.png` — 80×160 — *tile-x* — one unlit meter segment
```
A photorealistic single rectangular LED meter segment, unlit, dark smoked glass with a thin chrome divider edge, slightly recessed, faint top reflection. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text. One segment only, centered, head-on. Tall narrow.
```

### `seg-on.png` — 80×160 — *tile-x* — one lit segment (match seg-off)
```
A photorealistic single rectangular LED meter segment, brightly lit neon hot-pink (#ec0f8f), glowing glass with a thin chrome divider edge, hot even fill. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text. One segment only, centered, head-on, same size as seg-off. Tall narrow.
```

### `knob-pointer.png` — 300×300 — *knob-rotate* — pointer/chickenhead variant
```
A photorealistic glossy white glass control knob with a pronounced chrome pointer/chickenhead style indicator at the top (12 o'clock), polished chrome skirt, gently domed body, top-lit reflections, neon hot-pink (#ec0f8f) tip on the pointer. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text, no numbers. One knob only, centered, head-on, pointer straight up. Square.
```

### `jewel.png` — 120×120 — *sprite* — knob center cap
```
A photorealistic small round faceted jewel cap, polished chrome rim with a glossy neon hot-pink (#ec0f8f) glass center that catches the light, like a premium knob center button. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text. One cap only, centered, head-on. Square.
```

## Glass realism overlays (rendered on BLACK → composited with Screen/Add)
These are light-only, on a solid black field. Code blends them additively
(`"blend": "screen"`) so the black drops out — no alpha keying needed.

### `sheen.png` — 1024×1024 — *overlay ~20%, screen* (black bg)
Diagonal light glint across the glass. Drawn stretched over the whole panel.
```
A soft diagonal streak of white light on a solid pure black background, a single broad gentle highlight band sweeping from top-left to bottom-right, smooth gaussian falloff, soft feathered edges, like sunlight catching a glass surface. White light only on black, no objects, no text, no hard edges. Designed to be used with Screen/Add blend. Square, high resolution.
```

### `smudge.png` — 1024×1024 — *tile-overlay ~5%, screen* (black bg)
```
A seamless tileable fingerprint and smudge overlay on a solid pure black background, faint greasy swirls, soft smears and dust specks rendered as subtle bright grayscale marks catching light, very low contrast, for a barely-visible glass realism overlay. Marks on black only, no objects, no text, flat. Designed for Screen blend. Square, high resolution.
```

### `grain.png` — 512×512 — *tile-overlay ~8%, screen* (black bg)
```
A seamless tileable fine photographic film grain on a solid pure black background, even subtle bright monochrome speckle noise, very fine, low intensity, for a low-opacity Screen-blend overlay on dark screens. Speckle on black only, no objects, no text, flat. Square, high resolution.
```

### `flare.png` — 512×512 — *sprite, screen* — light leak (black bg)
```
A soft warm-to-pink anamorphic light leak on a solid pure black background, a gentle horizontal lens flare streak with a soft bloom core, neon hot-pink (#ec0f8f) blending into warm white, smooth gaussian falloff, no hard edges, no text. Light on pure black only, centered. Designed for Screen/Add blend. Square, high resolution.
```

## Per-plugin signatures (Wave 2)

### VocalComp · `gr-face.png` — 600×400 — *sprite (rectangle)* — gain-reduction meter plate
Rectangular white-glass GR faceplate — printed scale + numbers baked in. No needle (shared 2A needle).
```
A photorealistic analog gain-reduction meter face on a rectangular panel of frosted pure white glass with a thin chrome edge, a printed curved scale arc across the upper half with fine tick marks and small printed numbers reading right-to-left (0 1 2 3 5 7 10 20) for dB of gain reduction, "GR" printed small below the arc, vintage studio meter look, no needle. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow. One rectangular plate only, centered, head-on. Wide landscape rectangle.
```
> Reuses `2a-vu-needle` for the pointer.

### VocalQ · `eq-glass.png` — 600×320 — *9-slice (60)* — frosted graph screen
```
A photorealistic dark smoked-glass display panel with a very faint printed grid of thin horizontal and vertical guide lines, recessed rounded rectangle, thin chrome bezel, subtle inner shadow, faint diagonal reflection. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no text, no curve drawn. One screen only, centered, head-on. Landscape.
```

### VocalQ · `eq-node.png` — 120×120 — *sprite* — draggable band handle
```
A photorealistic small round glass band-control handle, glossy neon hot-pink (#ec0f8f) glass orb with a thin chrome ring and a bright top highlight, like a draggable EQ node. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text. One orb only, centered, head-on. Square.
```

### VocalVerb · `verb-backdrop.png` — 1200×800 — *backdrop (full-bleed, behind glass)*
Goes *behind* the frosted panel for depth. Backdrop allowed (this one isn't a cutout).
```
A soft dreamy out-of-focus bokeh backdrop, deep charcoal-to-black gradient with scattered gentle neon hot-pink and cool white bokeh orbs, heavy blur, cinematic depth, like blurred studio lights behind frosted glass, very smooth, no objects, no text. Full-bleed landscape image, high resolution.
```

### VocalDelay · `delay-reel.png` — 360×360 — *sprite* — tape reel motif
```
A photorealistic chrome-and-dark-glass tape reel seen head-on, polished metal hub with three spokes and a smoked-glass flange, vintage studio tape look, top-lit reflections. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text. One reel only, centered, head-on. Square.
```

### VocalGrit · `grit-tube.png` — 300×420 — *sprite* — glowing vacuum tube
```
A photorealistic small vacuum tube (valve), clear glass envelope with internal metal plates and a warm amber-to-pink glowing filament, chrome base pins, top-lit reflections on the glass. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text. One tube only, centered, upright, head-on. Tall portrait.
```

### VocalEss · `ess-shimmer.png` — 1024×1024 — *tile-overlay ~12%* (transparent)
```
A seamless tileable sparkle shimmer overlay, scattered tiny soft white and faint pink star-glints of varying size, mostly transparent, very airy, for a subtle high-frequency shimmer overlay. No objects, no text, smooth. Square, high resolution.
```

### VocalDoubler · `doubler-orbs.png` — 600×300 — *sprite* — twin-voice motif
```
A photorealistic pair of glossy glass orbs side by side, one tinted cool white and one neon hot-pink (#ec0f8f), slightly overlapping, thin chrome rims, top-lit reflections, suggesting two stereo voices. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text. Two orbs only, centered, head-on. Wide landscape.
```

---

# MARKETING / STORE (text + backdrops ALLOWED — not in-plugin)

These break the in-UI rules on purpose. Use for the website, DAW browser icon,
and sales page. Higher res, baked lighting/shadow is fine.

### `icon-<plugin>.png` — 1024×1024 — DAW + store icon (one per plugin)
Swap the bracketed motif per plugin. Template:
```
A premium app icon: a rounded-square tile of frosted white glass with a polished chrome edge and a soft neon hot-pink glow, floating on a clean light-gray studio background, centered on it a single glossy [SIGNATURE: knob for VocalKnob / piano keys for VocalTune / VU meter for Vocal2A / EQ curve for VocalQ / speaker grille for VocalAir / glowing tube for VocalGrit / waveform for VocalEss / two orbs for VocalDoubler / tape reel for VocalDelay / soft reverb cloud for VocalVerb / compressor needle for VocalComp] rendered in white glass and chrome with pink light. Soft realistic shadows and reflections, 3D, photorealistic, modern, clean. No text. Square, high resolution.
```

### `logo-suite.png` — 2048×1024 — suite wordmark (text allowed)
```
A premium 3D logo of the word "VocalGlass" rendered in frosted white glass with polished chrome edges and a soft neon hot-pink (#ec0f8f) inner glow, glossy and translucent, floating on a clean light-gray gradient studio background with soft reflections beneath. Photorealistic, modern, sleek, centered. High resolution, landscape.
```

### `hero-<plugin>.png` — 2048×1280 — sales-page beauty shot (one per plugin)
```
A photorealistic 3D hero product render of an audio plugin interface as a sleek frosted white glass and polished chrome panel with neon hot-pink accents and glowing controls, floating at a slight three-quarter angle in a dark cinematic studio with soft pink rim light and a gentle reflection on a glossy floor, dramatic premium lighting, shallow depth of field. Photorealistic, high-end product photography style. No readable text. Landscape, high resolution.
```

### `box-suite.png` — 2048×2048 — bundle / box art (text allowed)
```
A premium 3D box/bundle render for an audio plugin suite, a stack of glossy frosted white glass and chrome plaques with neon hot-pink edge glow arranged in a fan, floating on a dark cinematic studio background with soft pink rim lighting and reflections, high-end product photography. Photorealistic, modern, luxurious. Centered, square, high resolution.
```

---

# ★ MOCKUP-MATCH RECIPE (Vocal2A reference — clone to all 11)

The Vocal2A layout now matches the approved mockup structurally in code:
**charcoal backdrop → floating white-glass plate (screws baked in) → recessed
bottom tray → framed VU → soft code-drawn neon value-halos.** The only remaining
gaps are *material* assets. Generate these exact files (die-cut transparent PNG,
top-down/orthographic, centered, NO baked glow/shadow/text, NO pink unless noted)
and drop them in `assets/ui/`. They are already wired + in the CMake candidate
list, so they light up on the next build.

### `knob-large@2x.png` — 2048×2048 — *rotating dome* — SHARED
Replaces the white-glossy knob with the mockup's brushed-metal dome.
```
A photorealistic audio knob seen PERFECTLY TOP-DOWN, straight-on orthographic, dead center. A domed brushed dark-aluminium cap with fine CONCENTRIC lathe-turned brushing (circular grain, not radial spokes), softly lit from the top so a gentle highlight sits high on the dome, ringed by a bright polished chrome bevel with a crisp specular edge. Neutral metal only — silver/graphite. No pointer, no marker, no colored dot, no pink. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no drop shadow, no text. Knob fills ~88% of the frame, centered, circular. Square, high resolution.
```

### `knob-small@2x.png` — 2048×2048 — *rotating dome* — SHARED
Same knob, tuned for the small utility row (slightly thinner chrome bevel).
```
A photorealistic small audio knob seen PERFECTLY TOP-DOWN, straight-on orthographic, dead center. A domed brushed dark-aluminium cap with fine concentric lathe-turned brushing, soft top light, ringed by a thin polished chrome bevel. Neutral silver/graphite metal only. No pointer, no marker, no pink. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text. Knob fills ~88% of the frame, centered, circular. Square, high resolution.
```

### `bat-off@2x.png` / `bat-on@2x.png` — 1024×512 (2:1) — *toggle* — SHARED
Miniature chrome bat/lever switch in a capsule housing. Off = paddle LEFT,
On = paddle RIGHT. Identical housing/lighting between the two so they cross-fade.
```
(bat-off) A photorealistic miniature chrome bat-handle toggle switch seen straight-on, horizontal capsule housing of polished chrome with a soft brushed-metal inset, a short angled metal paddle lever flipped to the LEFT position. Neutral chrome/steel only, top-lit, crisp reflections. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text, no glow. Centered, fills ~90% width. Wide landscape 2:1.
```
```
(bat-on) Same photorealistic miniature chrome bat-handle toggle switch, identical housing, lighting and size, with the metal paddle lever flipped to the RIGHT position. Neutral chrome/steel only, top-lit. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text, no glow. Centered, fills ~90% width. Wide landscape 2:1.
```

### `led-red-off@2x.png` / `led-red-on@2x.png` — 512×512 — *indicator* — SHARED
Small round jewel LED beside `auto makeup`. (Code adds the bloom for -on.)
```
(led-red-off) A photorealistic tiny round indicator LED seen top-down, a dark ruby-red glass jewel in a thin polished chrome bezel, unlit, subtle top reflection. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text. Centered, circular, fills ~80%. Square.
```
```
(led-red-on) A photorealistic tiny round indicator LED seen top-down, a bright glowing red glass jewel in a thin polished chrome bezel, lit and saturated with a hot white-red center, subtle top reflection. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no baked outer glow, no text. Centered, circular, fills ~80%. Square.
```

### `seg-off@2x.png` / `seg-on@2x.png` — 1024×512 (2:1) — *selector* — SHARED  *(REGEN — off was too white)*
Chrome-bordered selector pill. Off must read on white glass (silver-grey).
```
(seg-off) A photorealistic selector button, a rounded-rectangle pill with a polished chrome bevel border and a brushed silver-grey inset face, soft top light, clearly darker than white so it reads on a white panel. Neutral metal only, no pink. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no shadow, no text. Centered, fills ~92% width. Wide landscape 2:1.
```
```
(seg-on) A photorealistic selector button, a rounded-rectangle pill with a polished chrome bevel border and a glossy hot-pink (#ec0f8f) inset face, saturated and bright, soft top light. Die-cut sticker, isolated PNG cutout, fully transparent alpha background, NO backdrop, no baked outer glow, no text. Centered, fills ~92% width. Wide landscape 2:1.
```

### `2a-vu-face@2x.png` — 2048×1280 (1.6:1) — *screen* — Vocal2A signature  *(optional refine)*
Only if the current frame reads too chunky vs the mockup.
```
A photorealistic analog VU meter unit seen straight-on, a glossy black rounded-rectangle bezel with a bright polished chrome outer trim, a warm cream face inside, a curved scale printed -20 to +3 with the 0..+3 portion in red, small "VU" text, two tiny screws bottom-corners, a small chrome center hub at the bottom. No needle. Die-cut sticker, isolated PNG cutout, fully transparent alpha background around the rounded unit, NO backdrop, no drop shadow. Centered, landscape 1.6:1, high resolution.
```

**Acceptance:** side-by-side with the mockup at 100% — brushed knobs, neon halos,
bat toggles + red LED, chrome segments, floating glass on charcoal, recessed tray.
Once 2A passes, the shared assets (`knob-*`, `bat-*`, `led-red-*`, `seg-*`) and the
code recipe drop straight into the other 10.
