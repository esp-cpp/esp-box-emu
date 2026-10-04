"""Regenerate port/stage_union.c from the per-stage actor tables.

Natively only one _StageCharacterEntries[] can link, so the port carries the
union of every stage it is able to run. Two filters decide what goes in:

  1. The stage's table must be entirely symbolic. 28 of the 93 files in
     source/stage/ still spell entries as raw MIPS addresses (0x800dxxxx)
     because that actor code is not decompiled; those mean nothing on Xtensa.
  2. Every CHARA_ macro must resolve to a real function in charalst.h -- some
     are still written as '?'.

A third filter cannot be decided by reading source: an actor that IS
decompiled may call helpers that live in an overlay this build does not
compile, and that only shows up as a link error. Those stages go in EXCLUDE
below, with the symbol that could not be resolved, so the next person can see
what would have to be added rather than rediscovering it.

    python gen_stage_union.py            # rewrite port/stage_union.c
    python gen_stage_union.py --list     # just print what would be included
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "source")
STAGE = os.path.join(SRC, "stage")

# Stages whose actors link only against overlays this build does not compile.
# Adding the overlay to esp32/main/CMakeLists.txt is the alternative, but the
# overlay trees redefine libdg internals, so each one has to be checked.
EXCLUDE = {
    "s11d": "AN_DamageSmoke1 (overlays/s11d/anime/effect/rope.c)",
}

# The stage blocks the retail disc actually carries, so the union never lists
# a stage the game could not load anyway.
ON_DISC = set("""
brf abst d00a d01a d03a d11c d16e d18a init rank roll s00a s01a s02a s02b s02c
s02d s02e s03a s03b s03c s03d s03e s04a s04b s04c s05a s06a s07a s07b s07c
s08a s08b s08c s09a s10a s11a s11b s11c s11d s11e s11g s11h s11i s12a s12b
s12c s13a s14e s15a s15b s15c s16a s16b s16c s16d s17a s18a s19a s19b s20a
option vr01 vr02 vr03 vr04 vr05 vr06 vr07 vr08 vr09 vr10 camera vrdemo preope
s00aa s04br s08br s08cr s10ar s17ar sound d00aa init_tux title selectvr select
opening change select4 select2 select3 select1 selectd demosel ending
""".split())

# Boot order first so the head of the file stays stable and readable.
HEAD = ["select1", "selectd", "demosel", "roll", "s07a", "s00a", "s01a", "s02a"]


def resolvable(charalst, name):
    m = re.search(r"#define\s+%s\s+\{([^}]*)\}" % name, charalst)
    return bool(m) and m.group(1).split(",")[1].strip() != "?"


def collect():
    charalst = open(os.path.join(SRC, "include", "charalst.h"),
                    encoding="utf-8", errors="replace").read()
    out = []
    for fn in sorted(os.listdir(STAGE)):
        if not fn.endswith(".c"):
            continue
        stage = fn[:-2]
        if stage == "_stage" or stage not in ON_DISC or stage in EXCLUDE:
            continue
        txt = open(os.path.join(STAGE, fn), encoding="utf-8",
                   errors="replace").read()
        if "_StageCharacterEntries[]" not in txt:
            continue
        body = txt.split("_StageCharacterEntries[]", 1)[1]
        if "0x800" in body:            # filter 1: raw MIPS addresses
            continue
        names = [n for n in re.findall(r"\bCHARA_[A-Z0-9_]+\b", body)
                 if n != "CHARA_END"]
        if any(not resolvable(charalst, n) for n in names):   # filter 2
            continue
        out.append((stage, names))
    out.sort(key=lambda p: (HEAD.index(p[0]) if p[0] in HEAD else 99, p[0]))
    return out


HEADER = '''/* One actor table for every stage this build can run, and the list of which
 * stages those are. GENERATED -- edit port/gen_stage_union.py, not this file.
 *
 * On the console each stage overlay carried its own _StageCharacterEntries[]
 * and only one overlay was resident, so the same symbol name never collided.
 * Natively there are no overlays: exactly one table can link, and whichever
 * stage it belonged to was the only one whose actors could spawn -- with
 * s07a's table linked, select1's script asked for the stage-select actor,
 * found nothing, and ended having drawn nothing. That was the black screen
 * after "end scenario".
 *
 * The per-stage split only existed to save PSX memory. Here the union is a
 * couple of hundred pointers, so link them all; GM_GetCharaID scans to
 * CHARA_END and finds whichever class the running stage asks for.
 *
 * Stages left out must never be entered: their tables still hold raw MIPS
 * addresses, and calling one jumps into whatever happens to sit at
 * 0x800dxxxx. Mgs_ResolveStage refuses them by name using the list at the
 * bottom. While only a handful of stages were packed into flash the missing
 * file was refusal enough, but a board reading the full 68.6 MB STAGE.DIR off
 * a card can reach all 96 and needs the explicit answer.
 */

#define DECLARE_NEWCHARA_PROTOS
#include "charalst.h"

CHARA _StageCharacterEntries[] = {
'''

MIDDLE = '''
    CHARA_END
};

/* The stages whose actor table is represented above. "init" is not here: it is
 * the boot stage and spawns no actors of its own, so it is always allowed. */
const char *const Mgs_PlayableStages[] = {
'''

FOOTER = '''
    0
};
'''


def main():
    stages = collect()
    seen, lines = set(), []
    for stage, names in stages:
        fresh = [n for n in names if n not in seen]
        seen.update(fresh)
        if fresh:
            lines.append("    /* %s */" % stage)
            lines += ["    %s," % n for n in fresh]
            lines.append("")
    names = sorted(s for s, _ in stages)
    rows = ["    " + " ".join('"%s",' % s for s in names[i:i + 6])
            for i in range(0, len(names), 6)]

    if "--list" in sys.argv:
        print("%d stages, %d actor classes" % (len(names), len(seen)))
        print(" ".join(names))
        if EXCLUDE:
            print("excluded:")
            for k, v in sorted(EXCLUDE.items()):
                print("   %-8s %s" % (k, v))
        return

    with open(os.path.join(HERE, "stage_union.c"), "w") as f:
        f.write(HEADER + "\n".join(lines).rstrip("\n") + MIDDLE +
                "\n".join(rows) + FOOTER)
    print("stage_union.c: %d stages, %d actor classes" % (len(names), len(seen)))


if __name__ == "__main__":
    main()
