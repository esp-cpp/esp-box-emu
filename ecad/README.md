# ESP-BOX-EMU ecad

This folder contains the electronic design of the ESP-BOX-EMU.

The ESP-BOX-EMU contains a single circuit board with the following components:
- USB-C connector
- Conductive / Membrane switches for d-pad, abxy, and start/select buttons which mate with GBC membranes and buttons
- Tactile switches for Volume +/-
- RED LED for charging indication
- DRV2605L haptic driver
- JST-PH connector for the LiPo battery
- AW9523 I/O expander
- MCP73831 LiPo charger
- MAX17048 LiPo state-of-charge monitor (I2C)
- MicroSD card slot
- TPS63020 5V buck-boost converter (battery -> 5V for the box)

These features are supported in two different versions of the electronics, targeting:
- ESP32-S3-Box
- ESP32-S3-Box-3

## Layout of this folder

- `elec/src/` - atopile sources (modern atopile >= 0.15 syntax)
- `elec/src/parts/` - atomic parts (auto-generated from LCSC plus local
  custom parts for the GBC membranes/buttons, board outlines and edge fingers)
- `elec/layout/` - KiCad layouts, one folder per build (netlist-synced by
  `ato build`; placement/routing edits are preserved)
- `scripts/` - layout automation (MCAD position export, scripted placement,
  base/connector layout sync)
- `Box Electronics v*.sch/brd`, `Box-3 Electronics v*.sch/brd` - the
  Fusion 360 / Eagle design the boards were actually manufactured from

The pre-migration atopile 0.2 sources, the old shared footprint library and
the old routed KiCad-6 boards were removed after the 0.15 migration; they
are available in git history (pre-2026-07) if ever needed.

## Setup

``` sh
# one time steps on your machine (any of these; uv shown)
uv tool install --python 3.14 atopile
```

Dependencies (e.g. `atopile/ti-tps63020`) are fetched automatically into
`.ato/modules/` on first build; `ato add <package>` adds new ones.

## Build

``` sh
# build everything
ato build

# build for ESP32-S3-BOX
ato build -b box-emu

# build for ESP32-S3-BOX-3
ato build -b box-3-emu
```

## Continuous integration

`.github/workflows/atopile.yml` (official `atopile/setup-atopile@v2` action)
builds every target from the checked-in sources with
`ato build --frozen --keep-picked-parts --keep-net-names --keep-designators`
-- the build *fails* if it would modify a checked-in layout -- then exports
gerbers, pick & place, BOM, STEP, GLB and a rendered PNG for `box-emu` and
`box-3-emu` (`-t all -t 3d-image`), plus top/bottom PDFs via the kicad-cli
bundled in the atopile container. Artifacts are uploaded per board and
attached to releases as zips. (KiBot is no longer used; the old config is
in git history.)

## Ordering / manufacturing notes

**Surface finish: order ENIG, not HASL.** The GBC button footprints expose
bare-copper interdigitated fingers that the carbon membrane pucks press
against. With HASL those fingers come back coated in solder (uneven, prone
to oxidation) and the buttons work poorly; with ENIG they come back flat
gold, which is the standard finish for carbon-contact keypads. This is a
per-order fab option (JLCPCB: "Surface Finish" -> "ENIG") and cannot be
enforced from the design files.

Related, already handled in the footprints: the buttons' four corner
connection pads are tented (copper-only, no paste, no mask opening), so
PCBA assembly applies no solder anywhere on the button contact areas. If
you regenerate or edit `GBC_*_BUTTON` footprints, keep the pads
`(layers "F.Cu")` only.

## Working on the KiCad layouts

Each build owns one board file: `elec/layout/<build>/<build>.kicad_pcb`
(KiCad 9+ format; there is no `.kicad_pro` — open the `.kicad_pcb` directly
in KiCad's PCB Editor). The `fp-lib-table` next to each board resolves all
footprints from the atomic parts in `elec/src/parts/`, so the boards are
self-contained as long as the repo layout is intact.

The `.ato` sources are the source of truth for everything electrical;
KiCad is only for placement and routing:

``` sh
# build a target and open its layout in KiCad in one step
ato build -b box-3-emu --open

# or open it manually
open "elec/layout/box-3-emu/box-3-emu.kicad_pcb"
```

The edit loop:

1. Change schematics by editing the `.ato` files (never add/remove
   components or change connectivity in KiCad — the next build will
   revert it).
2. Run `ato build -b <build>`. This syncs footprints and nets into the
   board file. Footprints are matched by their `atopile_address` property,
   so your existing placement and routing are preserved; new components
   appear stacked near the origin for you to place.
3. Place/route in KiCad and save. **Close the board (or at least save it)
   before the next `ato build`** — atopile rewrites the file on disk, and
   KiCad's PCB editor will not auto-reload; reopen the file after each
   build. A stale `~<name>.kicad_pcb.lck` file means KiCad still has it
   open.

Useful flags when iterating on layout: `--keep-designators`,
`--keep-net-names`, and `--keep-picked-parts` take those from the existing
PCB instead of regenerating them, and `--frozen` fails the build if the
PCB would change at all (good for CI).

Recovering from common accidents:

- **Deleted a footprint in KiCad?** Harmless to the design -- the `.ato`
  source is the source of truth, and the next `ato build -b <build>`
  re-adds it (near the origin; its placement is lost, its part gets
  re-picked). Restore known positions with `place-parts.py apply` if it
  was a mechanically-placed part. It's worth running
  `place-parts.py dump` into `positions.json` after every good layout
  session (and committing it) so *every* part's placement has a snapshot.
- **"Duplicate designators found in layout"?** Historical cause: the
  custom KiCad-6-era footprints carried hidden legacy
  `(fp_text reference "U5")` blocks, and KiCad 9 migrates those over the
  Reference field on every load -- so any save reset references to
  library defaults. Fixed 2026-07 by stripping the legacy blocks from all
  custom part footprints and boards. If it ever recurs (e.g. from a
  re-imported old footprint):
  `python3 scripts/fix-duplicate-refs.py <board.kicad_pcb>` then
  `ato build -b <build>` to renumber.
- **Deleted tracks** are only recoverable from git -- commit layout
  checkpoints often.

### Layout reuse (shared button clusters)

Every module instance becomes a KiCad *group* in the board, and the child
builds (`gbc-a-b`, `gbc-dpad`, ...) exist so their layouts can be reused by
the parents. When a parent build adds a sub-module group whose footprints
are all new, atopile pulls that group's placement from the child build's
board. So do the layouts bottom-up, running `ato build` between levels:

* box-3-emu / box-emu (top-level: connector position, final routing)
  * box-3-connector / box-connector
  * box-emu-base (most of the placement/routing work)
    * gbc-dpad
    * gbc-start-select
    * gbc-a-b-x-y
      * gbc-a-b

i.e. lay out `gbc-a-b` first, then `gbc-a-b-x-y` (which pulls the a-b
group), then the other button boards, then `box-emu-base`, and finally the
two top-level variants. The automatic pull only happens when the group
first appears in the parent, so later tweaks to a child layout are not
force-propagated over a parent you've already edited.

To re-pull an updated child layout into a parent on demand, either use the
"Pull Group" toolbar button in the PCB editor (select the group first;
atopile installs the plugin into `~/Documents/KiCad/<ver>/scripting/plugins/`
-- as of atopile 0.15.x only up to KiCad 9.0, so for KiCad 10 copy the
`atopile.py` loader from the `9.0` folder into `10.0` and Tools > External
Plugins > Refresh), or run the same thing from the CLI:

``` sh
ato kicad-ipc layout-sync --legacy \
    --board elec/layout/box-3-emu/box-3-emu.kicad_pcb \
    --include-group connector      # omit to just sync group membership
```

### Shared base layout across the two variants

`BoxEmuBase` is one module, so it is one group (`box`) in both top-level
boards, and its layout — placement *and* routing — lives in exactly one
place: `elec/layout/box-emu-base/box-emu-base.kicad_pcb`. The intended
workflow:

1. Place and route everything except the connector in the **box-emu-base**
   build (use `positions.json` + the placement script below for the
   mechanically-constrained parts, then route by hand).
2. Push it into both variants:

   ``` sh
   ./scripts/sync-base-layout.sh   # pulls groups "box" and "connector"
                                   # into box-emu and box-3-emu, then snaps
                                   # the connector group to the base group
                                   # so the two outline pieces close exactly
                                   # (scripts/align-base-connector.py)
   ```

3. In `box-emu` / `box-3-emu`, only route the connector-bound signals --
   group placement and outline closure are handled by the sync script.
   The relative outline offsets were calibrated by fitting all outline
   footprints against the Eagle board outlines (0.000 mm rms) and live in
   `scripts/align-base-connector.py`.

If you drag the `box` or `connector` group while editing a top-level board
(fine for repositioning on the sheet), the two outlines separate -- re-run
`python3 scripts/align-base-connector.py` (safe standalone; it snaps the
connector group back to the base group, prints an outline-closure
verification, and refuses to run while the board is open in KiCad so a
later KiCad save can't silently undo it). Reopen the board in KiCad after
running it.

The connector footprints themselves (TE dock connector on box-3, PMOD /
power edge fingers on box) are placed within their boards from the STEP
assemblies via `elec/layout/box-connector/cad_map.json` and
`elec/layout/box-3-connector/cad_map.json`, same flow as the base board
(export -> place-parts apply -> build). Note the box-3 dock connector's
r=180 is meaningful: it puts pad A1 on the side that mates with the box,
matching the manufactured Eagle board.

Pulls replace the contents of the pulled group, so keep variant-specific
routing (base-to-connector tracks) outside the `box` group — tracks you
draw in the parent are not group members by default, so this is the
natural behavior; just don't add them to the group.

### Scripted placement from MCAD

Mechanically-constrained parts (buttons, membranes, USB-C, uSD, battery
connector, volume switches) have known positions that come from the
enclosure CAD. Those live in `elec/layout/box-emu-base/positions.json`,
keyed by each footprint's stable `atopile_address` (e.g.
`dpad.up_button.btn`, `usb_c.conn`, `uSD.card`) so they survive rebuilds
and designator changes. When the MCAD/STEP changes, update the numbers and
re-apply:

``` sh
# apply positions to the board (position, rotation, front/back side)
python3 scripts/place-parts.py apply \
    elec/layout/box-emu-base/box-emu-base.kicad_pcb \
    elec/layout/box-emu-base/positions.json

ato build -b box-emu-base       # re-sync the board
./scripts/sync-base-layout.sh   # push into both variants
```

`scripts/place-parts.py dump <board>` regenerates the JSON from the
board's current state (useful to bootstrap the file or to capture manual
edits back into it). The optional `transform` block (`dx`/`dy`/`flip_y`)
converts CAD coordinates into KiCad's frame (KiCad y points down). Parts
not listed in the JSON are never touched.

### Generating positions.json from the Fusion 360 assembly

Instead of typing coordinates by hand, positions are extracted from the
STEP exports of the enclosure assemblies (`mcad/esp-box-emu.step` and
`mcad/esp-box-emu-3.step`; the PCB must be present as a component in the
assembly). After re-exporting from Fusion 360:

``` sh
# 1. discover occurrence names in the STEP (only needed when the
#    assembly structure / names change)
uv run scripts/export-positions-from-step.py list ../mcad/esp-box-emu.step

# 2. update elec/layout/box-emu-base/cad_map.json if needed:
#    - board.occurrence: the PCB component's name
#    - board.axes / kicad_origin: the CAD->KiCad frame (already calibrated)
#    - each part's "cad" name (+ one-time r_offset rotation calibration)

# 3. extract placements into positions.json
uv run scripts/export-positions-from-step.py export \
    ../mcad/esp-box-emu.step \
    elec/layout/box-emu-base/cad_map.json \
    elec/layout/box-emu-base/positions.json

# 4. apply / rebuild / propagate as usual
python3 scripts/place-parts.py apply \
    elec/layout/box-emu-base/box-emu-base.kicad_pcb \
    elec/layout/box-emu-base/positions.json
ato build -b box-emu-base
./scripts/sync-base-layout.sh
```

The script is self-contained (`uv run` fetches its OCP dependency on first
use). Front/back side is detected from the part's frame orientation and
can be overridden per entry with `"side"`; `"r_offset"` absorbs the
difference between footprint and CAD 0-degree orientations; `"offset"`
(anchor-frame mm) places bodiless parts relative to a CAD neighbor (used
for the a/b/x/y/start/select carbon pads, anchored to their membranes).
Re-run steps 3-4 whenever the MCAD changes.

All three `cad_map.json` files (box-emu-base, box-connector,
box-3-connector) are fully calibrated (2026-07): the Fusion board
components use the Eagle brd coordinate frames verbatim, all outline
footprints match the Eagle outlines exactly (0.000 mm rms), and the
anchor-convention differences between the old board parts and the new
LCSC parts (uSD, USB-C, battery JST, volume switches) are encoded in each
entry's `"offset"`/`"r_offset"`. Re-exports of the STEPs therefore produce
correct placements with no further calibration unless a part is swapped
for a different physical component.

### KiCad 10 file-format warning

atopile 0.15.x reads/writes the KiCad 9 board format (`version 20241229`).
**KiCad 10 silently upgrades any board it saves to `version 20260206`,
which atopile cannot parse** -- builds of that target fail and layout
pulls silently skip the group ("Group ... not found in layout maps").
Until atopile ships KiCad 10 support, after editing a board in KiCad 10
run:

``` sh
python3 scripts/downgrade-kicad10-pcb.py elec/layout/<build>/<build>.kicad_pcb
ato build -b <build>   # normalizes the file back to atopile's format
```

(or do layout work in KiCad 9, which reads and writes the format natively).

Note: the boards in `elec/layout/` were regenerated during the atopile
0.15 migration. Mechanically-constrained placement is scripted from the
MCAD STEP files (see above); routing must be redone by hand. The old
routed boards (atopile 0.2 era, KiCad 6 format) are in git history
(pre-2026-07) if a routing reference is ever wanted.

## Differences from the manufactured (Fusion 360) boards

The atopile design intentionally deviates from the Eagle/Fusion `v36`/`v11`
boards in a few places, mostly to reduce battery drain when the board sits
idle (see git history for details):

- battery sense divider is 1M + 1M with a 100nF sampling cap (was 10k + 10k,
  which drained ~200uA from the battery continuously)
- 5V rail comes from a TPS63020 buck-boost in power-save (PFM) mode with
  VINA/EN properly tied to VIN and PS/SYNC tied low
- DRV2605L gets its datasheet-required 1uF REG bypass and a VDD decoupler
- the AW9523 interrupt line is routed to the box (IO21) on both variants
