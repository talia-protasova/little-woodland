# UI font atlases

`serif.png` and `sans.png` contain the printable ASCII glyphs of DejaVu Serif and
DejaVu Sans, rasterized at 64 px. The corresponding proportional advances, bearings
and UV rectangles are in `src/rendering/FontMetrics.h`. Both images and that header
must be regenerated together. See LICENSE for font copyright and distribution terms.

The application loads these PNGs once through the existing RAII texture loader.
No font library, font installation, Python, or font download is required to build
or run the game. Missing required atlases produce the same file-specific texture
errors as required tile sprites.

To regenerate during development, install Pillow and the DejaVu fonts, then run:

```sh
python3 tools/bake_fonts.py
```

The tool reads `DejaVuSerif.ttf` and `DejaVuSans.ttf` from
`/usr/share/fonts/truetype/dejavu/`. Font layout deliberately supports printable
ASCII only; unsupported characters become `?`. This is sufficient for the current
English demo UI. Localization, dynamic font sizes, kerning and distance-field fonts
are outside this slice. Very large windows can soften the baked glyph edges.
