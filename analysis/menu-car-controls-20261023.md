# Vehicle-selection controls

> Captures and image sheets named in this note were removed from the repository (no game content in git); the procedure described here regenerates them locally.

The Wine vehicle-selection screenshot shows left and right arrows after
`SELECTIONNER`. In `menu.WGM`, the three `WMenuNodeCarSelection` records
serialize `m_bSelectLeftRight = 1`; this is direct resource evidence for
horizontal car selection. The font reel also contains dedicated directional
glyphs: byte `0x7d` points left, `0x7c` points right, `0x7e` points up, and
`0x7f` points down. The glyph sheet is `menu-font-glyphs.ppm`.

The native page previously displayed the up/down glyphs and changed cars with
Up/Down. The C menu now uses the left/right glyphs on pages 28 and 61, maps
Left/Right to the ordered `WMenuCarItem` list, and leaves Up/Down unchanged on
those pages. Other list pages retain Up/Down navigation.

I built with `make -C c-port` and launched the current source on a private
640x480 Xvfb display. From the initial Satanas et Diabolo card, one Right press
changed the preview and localized card to Dingo-Limousine; one Up press left
the Dingo card selected. The live native Dingo frame is
`native-selection-horizontal-controls-20261023.png`.
This verifies the native input/render path. The choice of horizontal input is
supported by the serialized flag and Wine's visible hint; a Wine key-event
trace has not been captured.
