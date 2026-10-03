# High-resolution bitmap font

`at01-2x.fnt` and `at01-2x.png` preserve the CC0 At01 font by GrafxKid shipped
with the pinned libdragon revision, including black outlines and exact doubled
glyph advances. Pixels use nearest-neighbour doubling. The normal build converts
the BMFont to a compressed RGBA16 font64 and includes it in the ROM filesystem.

To regenerate from that revision's source:

```sh
python3 tools/double-builtin-font.py /path/to/libdragon/src/rdpq/rdpq_font_builtin.c
```

The generator checks the pinned format before decoding its two CI4 layers.
