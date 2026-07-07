# Higgsfield Regeneration Prompts — VocalEssential Renders

Every render in `marketing/renders/` was produced with these prompts. To regenerate
any asset at higher quality on Higgsfield, paste the **base style block** plus the
plugin's **subject line**, and attach the matching existing render (or the clean UI
screenshot from `screenshots/clean/<Name>.png`) as the image reference.

## Base style block (prepend to every prompt)

> Photorealistic 3D product render of a hardware-style audio plugin unit.
> Materials: frosted white glass faceplate with subtle paper-grain texture,
> polished chrome bezel and corner screws, hot-pink neon accents (glowing rings,
> lit power buttons, neon underglow). Typography: clean SF Pro Display captions
> engraved/printed on the plate. Octane-quality rendering, physically based
> materials, soft studio reflections, crisp edge highlights, 8k detail.

## Variant blocks

**FLOAT variant** (append for `*-float@2x.png`):
> The unit floats weightlessly at a slight 3/4 hero angle in a dark charcoal
> cinematic studio, hot-pink rim light from the left, soft volumetric haze,
> pink neon glow reflecting on a wet dark floor far below, dramatic contrast,
> shallow depth of field.

**HERO variant** (append for `*-hero@2x.png`):
> The unit floats level and face-on over a seamless light-grey studio
> background, soft even lighting, gentle drop shadow directly beneath,
> minimal e-commerce product photography look, generous negative space.

**SUITE HERO FLOAT** (for `suite-hero-float-*.png`):
> All seventeen plugin units of the family arranged as a floating constellation
> at staggered depths, the large VocalRack channel strip centered as the
> flagship, smaller units orbiting around it, unified hot-pink neon glow,
> dark cinematic studio, volumetric pink haze, epic brand-campaign wide shot.

## Per-plugin subject lines

| Plugin | Subject line |
|---|---|
| VocalGrit | Compact saturation unit, one large chrome dome knob center, four neon character pills labeled CLEAN WARM DIRTY BLOWN, glowing pink drive ring |
| VocalEss | De-esser unit, two chrome knobs (AMOUNT, FREQ) with pink neon rings, small gain-reduction meter strip |
| VocalQ | Vocal EQ unit with a wide dark glass display showing a glowing pink EQ curve with chrome node dots, four small chrome knobs below |
| VocalKnob | Minimal one-knob unit, single oversized chrome dome knob with a thick pink neon halo, single POLISH caption |
| VocalAir | Air enhancer, two chrome knobs (AIR, EXCITE), thin pink neon spectrum strip along the top |
| VocalComp | Vocal compressor, three chrome knobs, horizontal pink gain-reduction meter bar, ARC caption |
| Vocal2A | LA-2A-style leveler, large cream-white analog VU meter with chrome surround and pink needle, two big chrome knobs (GAIN, PEAK REDUCTION) |
| VocalTune | Pitch corrector with a dark glass display showing a glowing piano keyboard, lit pink keys, two chrome knobs (SPEED, AMOUNT) |
| VocalVerb | Plate reverb, dark glass display with a pink reverb decay waterfall, three chrome knobs, VINTAGE/MODERN toggle |
| VocalDoubler | Stereo doubler, two chrome knobs (WIDTH, BLEND), mirrored pink stereo-wing motif on the plate |
| VocalDelay | Analog delay, four chrome knobs, dual pink neon delay-tap LEDs, PING-PONG toggle |
| VocalGate | Noise gate, two chrome knobs (THRESH, RELEASE), vertical pink signal LED ladder |
| VocalMod | Modulation FX, three chrome knobs, animated-looking pink LFO sine wave etched in neon across the plate |
| VocalBlend | Master-bus blender, one large center crossfade chrome knob labeled VOCAL / BEAT, two small trim knobs |
| VocalChop | Vocal chopper, dark glass display with a chopped pink waveform grid, RATE knob, tempo-sync pill buttons |
| VocalClip | Soft clipper, dark glass display showing a glowing pink transfer curve, one big chrome CLIP knob, SOFT WARM HARD pills |
| VocalRack | Full-width channel strip: nine stacked module rows (GATE, DE-ESS, EQ, COMP, HEAT, AIR, DELAY, REVERB, CLIP), each with a pink power button and chrome mini-knobs, twin output VU columns on the right |

## Output spec

- PNG, transparent background where the variant allows (FLOAT/HERO cutouts),
  minimum 2048 px on the long edge (rendered at 2x for `@2x` naming).
- Keep the plate aspect close to the real UI (see `screenshots/clean/`).
- No baked-in marketing text, watermarks, or logos other than the on-plate captions.
