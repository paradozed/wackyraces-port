# Voice sound filename lookup

## Static table and call path

`wpc.exe` contains the voice lookup routine at VA `0x00426f20`, called through
thunk `0x00404a6b`. Its caller at `0x00426e0c` checks one numeric argument
against 7, passes two numeric arguments to the lookup, and formats a returned
name into `Voice/%s`.

The lookup has a special range for its second stack argument, 100–103. Those
cases return `vo-N-900.snd` through `vo-N-903.snd`, advancing a shared byte
through the four values. For the table lookup, both numeric arguments must be
less than 7. The routine reads a selector word from the calling object at
`+0xa4 + arg2*2`, then searches a table at VA `0x00527d20` using a 17-byte
entry stride. Entry fields observed in code and data are:

| Offset | Observed use |
| ---: | --- |
| `+0x00` | compared with numeric argument 2 |
| `+0x04` | compared with numeric argument 1 |
| `+0x08` | runtime/cache state; initialized to zero in the file |
| `+0x0c` | pointer to an unprefixed `.snd` basename |
| `+0x10` | one byte at the end of the 17-byte entry |

The routine prepends `vo-` to the selected table basename before the caller
formats the `Voice/` path. The table contains 196 entries: all 49 raw argument
pairs occur four times. The object's selector word affects the starting entry
for the search. The four entries per pair are kept in table order in the TSV;
they are not assigned semantic meanings here.

`analysis/voice-sound-map.tsv` lists each row's table index, raw `arg1` and
`arg2`, position within the repeated pair, basename, constructed path, and
whether that path exists in the installed `Voice` directory. There are 192
unique basenames in the table, of which 158 unique constructed paths exist in
the installation. The four special `vo-N-900.snd` through `vo-N-903.snd` paths
are outside this table and are installed, giving 162 unique statically named
voice files found in `Voice`. Thirty-four table basenames have no matching
installed file. Repeated table entries explain the difference between 196
rows and 192 unique table basenames.

The voice-map report was produced by a bounded parser that validates PE32 x86
headers, maps table and string pointers through the PE section table, and
compares names with installed files. It rejects malformed headers,
out-of-section pointers, invalid names, and unexpected table dimensions. The
native C port does not yet include this inspector.

## Limits

These are exact static argument-to-filename data relationships, not names for
game events. The argument meanings, selector initialization and variation
policy, and the 34 absent files remain unresolved. The path construction does
not prove a particular gameplay scenario loads a particular file. See also
`analysis/snd-observations.md` for the decoder evidence and remaining audio
format assumptions.

Additional caller inspection shows that the lookup is used to form a `Voice/`
path before a voice-resource object is created and linked into a manager list.
The caller has early-return and probability-like gates based on object fields
and a runtime helper. This narrows the context to conditional voice-resource
creation, but does not identify event names or explain the gate values. Details
are in `analysis/disassembly/wpc-voice-lookup.txt`.

`wpc.exe` SHA-256:
`e770a5ea14d19f8d9b3692645f22e6ea83468dc0e14565b7dc7e45cf3640850b`.
