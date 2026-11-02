# Native asset loading

`src/port/game_assets.cpp` supplies owned WGM storage and bounded chunk/resource
views, 24-bit uncompressed BMP loading, and TGAF decoding into top-left RGBA.
All game files are read-only inputs. No image bytes are embedded in the port.

The outer reader checks `WPCG` version `0x02020000`, every chunk's declared
span, duplicate top-level tags/resource `(tag, ID)` keys, and the presence of
`RSRC`. `GTEX` and `GMAT` children start after their eight-byte ID/format-token
prefix. Material links are `TEXP {count=1, "GTEX", ID}`; an absent texture is
represented in the files as a four-byte zero count and cannot be resolved by
`material_texture`. Multiple texture links remain unsupported.

## TGAF framing correction

Direct examination of the installed assets confirms a 20-byte wrapper and an
18-byte embedded TGA header. Both truecolor raster bytes and indexed palette
bytes start at TGAF offset **38** for a zero-length image ID. The indexed
raster follows the palette. The earlier offset-40 variant described in
`tgaf-menu-film.md` is unnecessary for the five footer-bearing indexed menu
textures: all five decode with the standard offset-38 palette position.

The wrapper's embedded-length word bounds the TGA. Up to three bytes outside
that extent align the TGAF record to four bytes. No raster padding or truncated
pixel reconstruction is applied. Supported tails inside the TGA are a standard
26-byte TGA 2.0 footer, optionally preceded by a correctly located 495-byte
extension. Fourteen indexed images in `general.wgm` carry that extension;
descriptive extension metadata is ignored, with no embedded offsets dereferenced.
Other tails and developer areas are rejected.

The reader handles indexed eight-bit/24-bit-palette and truecolor 24/32-bit
uncompressed TGA images. It checks wrapper/header agreement, color-map and
raster spans, palette indices, and orientation flags. BGRA/BGR source bytes
become RGBA. A truecolor pixel's stored alpha is preserved. For indexed pixels,
an all-black palette color is transparent and other colors are opaque; WPC's
linked-resource threshold that can generate intermediate alpha is still
unrecovered. Zero-width/height menu records are preserved as empty images.

## Observed acceptance

Read-only execution of the native decoder on the current installed assets:

| Archive | Resource records | Decoded textures | Empty images | Single texture links | Untextured materials |
| --- | ---: | ---: | ---: | ---: | ---: |
| `menu.WGM` | 2,032 | 662 | 44 | 665 | 2 |
| `SWMenu.WGM` | 2,044 | 670 | 114 | 673 | 0 |
| `general.wgm` | 819 | 275 | 11 | 343 | 7 |
| `swgeneral.wgm` | 879 | 277 | 10 | 399 | 9 |

These counts establish byte framing and successful conversion, not matched
game rendering, runtime alpha thresholds, or complete game behavior. Some
course materials use texture-link forms beyond the single-link reader;
complete world/material loading remains unfinished.

Repeat the synthetic checks and optional corpus inspection with:

```sh
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Isrc/port \
  src/port/game_assets.cpp tests/native_assets.cpp -o /tmp/wpc-native-assets-test
/tmp/wpc-native-assets-test
/tmp/wpc-native-assets-test /path/to/menu.WGM /path/to/general.wgm
```

The synthetic suite currently performs 332 checks, including every truncated
prefix of representative BMP/TGAF/WGM inputs, malicious extents, copied archive
ownership, retained state after failed reload, image origins/channel order,
palette color keying, extension framing, resource joins and dangling links.
Fixtures are generated without copying copyrighted asset bytes.
