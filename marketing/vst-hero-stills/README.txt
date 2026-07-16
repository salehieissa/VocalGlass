VST HERO LOOP — SOURCE STILLS (frame-0 / poster + Seedance seed)
================================================================

These are the canonical still renders for the /vst hero loops. One PNG per
plugin id, matching the loop filenames. Seed Seedance (image-to-video) with the
matching file; the site uses the SAME file as the <video poster>.

These are the Apple-style "floating hardware on pure white" product renders
(same image already used as each Shopify product's primary image). They're the
"premium hardware idling on a shelf" look the loop spec calls for.

  vocaltune.png  vocalq.png     vocaless.png    vocalcomp.png
  vocal2a.png    vocalknob.png  vocalair.png    vocalgrit.png
  vocalverb.png  vocaldelay.png vocaldoubler.png vocalgate.png
  vocalclip.png  vocalmod.png   vocalchop.png   vocalblend.png

Note: these renders sit on WHITE with a soft baked shadow. The loop DELIVERY
spec requires TRANSPARENT alpha, so matte/roto the device after Seedance and
drop the white + shadow on export (the device is a single rigid centered object
with only a few degrees of motion — clean to segment).

VOCALGEEK — needs its product render generated first (GPT Image 2 on fal)
-------------------------------------------------------------------------
VocalGeek never got a float-on-white product render, only flat console
screenshots. Before animating it, generate a matching product still with
GPT Image 2 on fal so it's consistent with the other 16:

  reference image: vocalgeek-SOURCE-console.png (the real UI, LEAN/pink theme)
  GPT Image 2 prompt:
    "Product render of the VocalGeek handheld plugin — a Game Boy-style white
     frosted-glass console with a chrome bezel and corner screws — floating in
     3/4 perspective on a pure white background with a soft realistic drop
     shadow, Apple-ad style, matching the attached reference exactly (same
     layout: big pixel LCD screen up top showing the pink LEAN waveform scene,
     chrome D-pad bottom-left, two hot-pink round HIT A/HIT B buttons
     bottom-right, chrome TAP and PRINT pill switches, a chrome cartridge bay
     with a pink syrup pill, DOSE and OUTPUT knobs with hot-pink #FC22C3 neon
     rings flanking a chrome speaker grille, 'VocalGeek' wordmark at top).
     Photoreal, crisp, studio lighting, no text overlays, no extra props."
  aspect: match the other product renders (roughly square/4:3 float framing).

Save the result as vocalgeek.png here, THEN seed Seedance with it.

Alt source stills (flat front-on UI, if the site wants the UI look instead of
the 3D float): repo screenshots/clean/<Name>.png, and for VocalGeek the five
cartridge stills in marketing/vocalgeek-refs/.
