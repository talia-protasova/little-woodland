# Little Woodland artwork

The supplied illustrations are loaded unchanged. No generated replacement artwork is used.

## Used now

- `tiles/berry.png`, `acorn.png`, `leaf.png`, `mushroom.png`, `star.png`, `letter.png`:
  required 256×256 RGBA sprites, mapped in TileType order.
- The five `characters/hedgehog_*.png` illustrations: optional context-specific poses.
  Letter-holding accompanies play, happy marks delivery, worried accompanies low moves
  and defeat, and celebrating accompanies victory. Neutral is the fallback when a pose
  is unavailable. Missing/corrupt character art is logged; no replacement art is invented.
- `fonts/serif.png` and `fonts/sans.png`: bundled, licensed DejaVu atlases for headings,
  counters and secondary labels. See `fonts/README.md` and `fonts/LICENSE`.

- `backgrounds/post_office.png` (256×256 RGB): optional illustration on the dedicated
  result pages, proportionally fitted in a 256-logical-pixel bookplate with softly feathered
  edges. A single celebrating/happy postmaster accompanies victory; a single worried
  postmaster accompanies defeat. The happy/worried result poses now use prepared authored GIF
  frames, with corresponding PNG fallbacks. Missing illustration art is logged and the character
  receives the illustration area; no replacement assets are generated.

## Loading and rendering

Build and launch from the project root:

```sh
cmake -S . -B build
cmake --build build
./build/little_woodland
```

Launching `./little_woodland` from `build/` also works. Development builds have a checkout fallback path; installed builds first resolve
`../share/little-woodland/assets` relative to their executable. Portable release builds omit
the checkout fallback entirely. No post-build copying is needed. See README.md and RELEASE.md
for the local binary-only TGZ workflow.

`src/rendering/Texture.*` decodes PNGs once at renderer construction with the vendored
PNG-only stb_image implementation. Required texture failures name the exact file and decoder
or GPU error. Texture owners release their OpenGL handles before the window/context dies.

Images become RGBA regardless of the source PNG color mode. RGB channels are premultiplied
by alpha before upload and mipmap generation, then blended with ONE / ONE_MINUS_SRC_ALPHA.
This avoids transparent-edge halos during bilinear/trilinear downscaling. Texture edges
clamp; image rows retain file order and quad UVs use v=0 at the top, with no global flip flag.

Sprites fit proportionally within 82% of a board cell, preserving padding. Selection/match
borders sit behind the artwork. The existing resolution trace controls every sprite's
position and scale, including swaps, removal, gravity, refill, delivery and reshuffle.
Board-edge clipping crops UVs as well as geometry so entering sprites reveal naturally.
The storybook UI uses a warm paper page, a quiet sage/linen cell grid, rust selection
outlines, a persistent delivery objective and readable serif/sans typography. Dedicated
illustrated result pages replace the board only after its final gameplay animation. The character and Restart button share a footer in portrait and an information
column in landscape. The visible button and board use the same responsive coordinates
for rendering and input. Restart remains available during playback and after outcomes; result pages use Play Again
or Try Again with their own shared render/input layout.
The board status indicates when gameplay is locked; R and Escape retain their behavior.

## Verification

```sh
ctest --test-dir build --output-on-failure
# On a machine without an active display:
xvfb-run -a ctest --test-dir build --output-on-failure
```

The existing rule/level/playback suites are preserved, with a sixth suite covering result
transition timing, final summary counters, input locks, restart cleanup and responsive result
button placement. `png_textures` requires a GL 3.3
context and reports a skip if no display is available. Under Xvfb it checks all six actual
tile images, transparent/antialiased pixels, premultiplied RGBA, mipmaps, a known 2×2
orientation/alpha fixture, missing/invalid-file diagnostics and texture deletion by RAII.

Graphical smoke checks cover selection/deselection, clickable restart, swaps, removal, gravity, refill,
cascades, delivery, illustrated victory/defeat pages, replay controls, and portrait/landscape
resizing. Real physical
high-DPI displays remain a manual check. See `third_party/stb/README.md` for decoder version
and license details.

## Audio and authored character motion

The original happy/worried GIFs are preserved unchanged. `tools/prepare_characters.py` uses
Pillow during development to compose disposal and transparency onto 256×256 RGBA canvases.
Prepared frames/manifests are included under `characters/animations/`. Both have 151 frames,
5030 ms per loop and the original 30/40 ms frame durations. `--check` compares every output
pixel, alpha value, timing and canvas with the source GIF. Runtime needs only PNG textures.

The result-page clock begins at page appearance, uses elapsed time, stops on restart, and
chooses a preloaded texture without GPU uploads or file reads per frame. All authored frames
use the existing character bounds. No procedural entrance movement, rotation or scaling is
added. Missing/corrupt prepared animations use happy.png or worried.png instead, one
character at a time. About 101 MiB of GPU storage is used for both loops with mipmaps.

Optional local WAVs live only under the Git-ignored `audio/mixkit/` directory; none were
tracked. See audio/SOURCES.md for the author’s licensing confirmation, file hashes and distribution restrictions.
Miniaudio 0.11.23 decodes once and drives a bounded eight-voice stereo mixer via PulseAudio
or ALSA. Selection, removal, gravity and delivery follow actual playback stage entries;
victory follows entry into Won and fires once. Defeat has no dedicated cue or music.
Delivery/victory take priority, gains are restrained, and mute immediately stops active
effects. No physical listening test has been performed; gain balance needs manual review.

The small upper-right sound control works during gameplay/results, and M toggles once per
press. Mute survives level restart. “No audio” means no device or no optional sounds could
be loaded; gameplay continues normally. No WAVs are bundled with public source files.
