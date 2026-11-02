# Native list-page layout from WGM item coordinates

The native renderer previously placed every non-main menu list at the same
hard-coded X/Y origin and row spacing. It now reads the selected
`WMenuPage`'s serialized X/Y properties (`0x197`/`0x198`) and each direct
`WMenuItem` or `WMenuSetValueItem` X/Y advance (`0x18a`/`0x18b`). It builds
the cursor anchor for each localized label by starting at the page position,
then accumulating each preceding item's own X/Y advance. The label item IDs
from the WGM text-key walk associate those anchors with the displayed strings.

This follows the statically recovered `FUN_0048bab0` loop: WPC lays out each
menu item at the current page cursor, then advances by that item's serialized
coordinates. The member registrations and the main-page Wine/GDB capture
support the property mapping. The main page continues to render at the
previously captured cursor anchors after switching to per-item coordinates.

An isolated native Xvfb capture shows the per-item layout on the single-player
mode page (`0x13`) and the two-player page (`0x2f`):

!Single-player mode page

!Two-player page

These are native captures, not original-runtime comparisons. The positions on
pages other than page 4 now come from their WGM item records, but their exact
Wine cursor positions and text scaling are not yet measured. The page body's
black clear color also remains unverified against WPC.
