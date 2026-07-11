## notes

see parallelcore.md for the real writeup.

this is the last session on this project for me. everything below is the informal log kept as is. the real handoff is parallelcore.md with architecture, benchmarks, determinism evidence, ported elements, official tpt comparison, limits, and next steps.

### this session

* ported portal and plsm
* fixed a portal duplication bug by switching from delete plus create to one atomic move
* fixed a long standing age bug in plsm and steam by folding age loss into move
* found the first real scaling numbers from bench.exe, modest but real
* built the capstone scene with all 7 elements including portals and verified hashes across 3 chunk counts
* wrote docs parallelcore.md as the handoff doc

### bugs fixed

* bench warmup was contaminating the first config. fixed with one untimed simtick before the sweep and a short warmup per config
* resolver had a real mass conservation bug involving swap and an independent move from the same source cell. fixed and added a regression test
* dynamic world size is done, but only as a global size contract. it works for one size at a time, not multiple live worlds

### dynamic world size

width and height are now mutable globals stored in world.cpp and set before constructing a world. most code did not need changes. the only rule is set size before world creation and do not change it after. this is fine for the current bench flow, but it would be safer as per world state if multiple sizes ever need to coexist.

### benchmark

bench.cpp now sweeps world size and chunk count with warmup fixed.

short sandbox run shows the right shape for a single core: more chunks just adds overhead, no fake speedup.

this sandbox cannot give the important number. run build.bat bench on the 3600 and paste the output. that is the actual milestone proof.

the bench uses one fixed 42 seeded fill per size and offsets the measured tick range so warmup and measurement do not overlap.

### bigger bug

the swap bug was real. move and create require empty targets, but swap does not. that means a swap can collide with another particles independent move in the same tick.

old resolver: sand vanished, water duplicated
current resolver: sand sank, water rose, conflict handled correctly

this bug had existed since the first resolver. it only showed up once the demo reached the right geometry, so earlier checks missed it. added a permanent regression test for it.

### acid

acid port went cleanly.

* no changes needed to cmd, touched cells, or resolver
* acid uses delete on a neighbor cell, which is a new but valid pattern
* found a useful property: two acids can target the same sand cell, only one delete wins, but both still spend potency
* ran a 40 by 30 sand block with a 20 by 5 acid patch for 280 ticks and it behaved normally
* fixed one bad test that was actually a setup mistake, not a code bug

added type acid, acid_demo.cpp, and 4 tests. total is now 20 tests and 993667 checks with 0 failures.

### far reaching reactions

wrote far_reaching_reacs.md as classification only.

main takeaway: portals might already mostly work. move does not care about distance, only about cell conflicts, so teleporting may just be move with a far destination plus a pairing table. not built yet, just the cheapest thing to test next.

also noted that not all far systems need the same kind of own pass. pressure and gravity want one shape, inst another, photons another.

### answer to is the rewrite worth it

evidence so far:

* deterministic across many seeds and chunk counts
* fuzz tested and caught real bugs
* cleaner api, shown by acid needing no core changes
* easier reasoning, shown by the swap bug being found through hash drift and fixed quickly
* easier testing, with 20 tests and 993667 checks
* parallel compute is real, but the scaling number depends on real hardware, not this sandbox

### answer to far reaching reactions

the local global split holds up. the far systems do not fit the normal local compute resolve apply pipeline as is, and they do not all want the same kind of extra pass.

### still open

* dynamic world size is still just a convention, not enforced
* resolver apply pass is still single threaded
* priority defaults are still not settled
* portal hypothesis is not built yet
* no second element yet
* still standalone, not the real osmium toy repo

### build and run

build.bat demo -> demo.exe
build.bat detcheck -> detcheck.exe
build.bat bench -> bench.exe
build.bat stats -> statsdemo.exe
build.bat acid -> acid_demo.exe
build.bat tests -> tests test_runner.exe
build.bat all -> all of the above
