# Common

Anything shared by more than one board.

- **datasheets/** — component datasheets. See its README before adding files.
- **library/** — schematic symbols and PCB footprints shared across boards.

## Library

Both boards use the same ESP32 module, the same DHT22, and the same I2C
pull-up arrangement. Those symbols and footprints belong here, not duplicated
per board, so that a footprint fix made after the first fab run propagates to
the second board instead of being silently missed.

The existing incubator libraries (`incubator.SchLib`, `incubator.PcbLib`) are
still at
[apps/firmware/Hardware/Eggubator/](../../apps/firmware/Hardware/Eggubator/).
They are named for the incubator but hold parts both boards need. Splitting the
shared parts out into this folder is part of the migration noted in
[../CONVENTIONS.md](../CONVENTIONS.md#eda-tool) — until then, treat those files
as the shared library and do not fork a second copy.

## Third-party library (not in git)

`altium-library-master/` is a large third-party symbol/footprint/3D-model
collection — **2.4 GB across ~31,000 files**. It is gitignored and stays local.

Do not commit it. Git stores every version of every binary forever, so a
2.4 GB import is not something a later commit can undo — only a history
rewrite, which breaks every existing clone. If it needs to be shared across
machines, the options are a **git submodule** pointing at its upstream repo, or
**Git LFS**; either is a deliberate decision, not something to do by dragging a
folder in.

What *should* live in `library/` is the small set of parts this project
actually uses — copied out of that collection, checked against the
manufacturer drawing, and committed individually.

## Rules for shared parts

- A footprint here has been checked against the manufacturer drawing, with the
  drawing's revision recorded in the footprint description. "It looked right"
  is how a board comes back unpopulatable.
- Do not change a shared footprint's pad geometry after a board has been
  fabricated against it. Add a new footprint variant and migrate deliberately.
- Every symbol's pin names match the datasheet's names, not the net names of
  whichever board used it first.
