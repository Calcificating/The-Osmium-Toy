# ParallelCore
*11th of July, '26*
*Calc*

A research prototype answering one question: can The Powder Toys simulation
be redesigned so it parallelizes naturally, instead of bolting threads onto
an update loop that was never built to allow them?

This is a standalone spike that
never touched the codebase - the goal was to prove the
architecture on a toy grid first, cheaply, before ever proposing a rewrite
of the real thing.

## project status, plainly

**Done:**
- read/compute/resolve/apply pipeline
- decide phase runs in parallel across persistent thread pool
- consistent across thread/chunk counts, across seeds, across seven
  different element types working together, tested
- 9 elements ported, including one (portals) that stress-tested
  whether the architecture bends or fights
- multicore bench data collected on my 6c/12t machine

**Partial:**
- the resolver is still entirely single-threaded. to note, this is not a minor 
  gap

**Not started:**
- anything from `far_reaching_reacs.md` (pressure, electricity, photons,
  SOAP, STKM, WIFI) is only classified
- no save file parser, no GUI, no rendering beyond ascii debug output, this is still a prototype
- no GPU work of any kind - this was always stated as a "possible later
  payoff", which i still believe would be true

skip to *"# State"* near the bottom if you dont wanna read all this

---

## why

TPTs simulation, as far as i understand (ive not
read Simulation.cpp much) updates particles by mutating the live 
world directly as each one is processed. that means correctness depends 
on update ORDER. order dependence is the thing that makes naive multithreading 
dangerous; two particles racing to touch the same cell isnt a hypothetical, its 
the default failure of that model the moment you add threads without
rethinking the update itself

The five questions this project was built to answer:

| Q | A |
|---|---|
| Can the sim be made order-independent? | Yes. commands are resolved by a fixed, predictable arbitration rule, not by whichever thread got there first |
| Can particles update without directly mutating the live world? | Yes. compute phase holds a `const World&`, cannot write to it, enforced by the compiler not just convention |
| Can the world be split into chunks or passes? | Yes for decide. not yet for resolve/apply. see benchmarks |
| Can CPU/GPU work on different parts cleanly? | Untested. the architectures read/compute/resolve/apply split is GPU-shaped *in principle* (compute phase has no shared mutable state), but i havent written a single line of GPU code against it |
| Can the engine keep TPT-like behavior while becoming parallel-friendly? | Partially true. 7 elements working together, including one non-local one (portals), stayed inside the same pipeline without needing a rewrite of the pipeline itself. far-reaching systems are still just a classification document, duh |

---

## architecture

```
Simulation Tick

Current World (read-only during compute)
|
Portal Index Build single-threaded
(scan for PRTO cells, build a small read-only lookup table.
cheap today, a third serial phase worth knowing about)
|
Parallel Compute is DONE
(world split into row-chunks, a persistent thread pool runs
ComputeIntent per cell, each cell emits a list of commands
into a per-chunk buffer, nothing is written to World here)
|
Resolver is PARTIAL
(conflict detection: groups commands by which cell they touch,
picks one deterministic winner per cell. correct, tested,
but runs on a single thread using std::unordered_map)
|
Apply Commands is PARTIAL
(single-threaded, writes the winning commands into the next
buffer)
|
Next world (buffers swap)
```

### the four-stage pipeline

1. **Read** : `const World&`, compiler-enforced read-only
2. **Compute intents** : each element is a stateless namespace function,
   `Namespace::ComputeIntent(const World&, x, y, Rng&) --> vector<Cmd>`.
   no element knows threads, chunks, or other particles exist beyond what
   it reads.
3. **Resolve conflicts** : `resolver.cpp` groups commands by which grid
   cell they write to, picks exactly one winner per contested cell via a
   predictable tiebreak (priority, then source coordinate). only knows 
   "N commands want this cell, heres the rule.".
4. **Apply** : copies the winning commandss effects into the next buffer.
   buffers swap, tick ends.

### command types

`MOVE`, `SWAP`, `DELETE`, `CREATE`, `HEAT`, `AGE`. 
`HEAT`/`AGE` modify a field on whatever already occupies 
a cell and don't contest ownership. everything else does. 

### double buffer

`World::cur` / `World::nxt`. compute reads `cur` exclusively. resolver
writes `nxt`. `clearNext()` copies `cur` into `nxt` as a baseline so
particles that get no command just persist. swap at the end of the tick.

### thread pool

`threadpool.h`. Built once, reused every tick. this mattered more than
expected as per benchmarks.

### auxiliary read-only state

Portals needed something beyond "read your neighbors": a global,
per-tick, read-only lookup table (`portal_index.h`) built once before the
parallel region starts. thi sis the general pattern for anything that
needs whole-world context during compute without becoming a race: build
it once, single-threaded, hand it down as another `const&` alongside
`World`

---

## average benchmark results

ran on a ryzen 5 3600 (6c/12t).

```
World: 64x64
  1 chunks: 12312 ticks/sec
  2 chunks: 12935 ticks/sec
  4 chunks: 15459 ticks/sec
  6 chunks: 15623 ticks/sec    peak, 1.27x over 1 chunk
  8 chunks: 14403 ticks/sec
  12 chunks: 10111 ticks/sec   overhead wins
  16 chunks: 8298 ticks/sec

World: 128x128
  1 chunks: 2185 ticks/sec
  8 chunks: 3254 ticks/sec     peak, 1.49x
  16 chunks: 3094 ticks/sec

World: 256x256
  1 chunks: 320 ticks/sec
  16 chunks: 428 ticks/sec     still climbing at the largest chunk count, 1.34x

World: 512x512
  1 chunks: 33 ticks/sec
  ...noisy, 24-39, no clean peak
```

### this is real scaling 

A 6-core CPU parallelizing something embarrassingly parallel should get
close to 6x, not 1.3-1.5x. Before assuming the thread pool is broken,
used `statsdemo.exe` (per-tick compute/resolve/apply
timing) to check at tick 20 on the 120x90 demo scene:
```
Compute: 0.082 ms   (paralell)
Resolve: 0.115 ms   (serial)
Apply:   0.003 ms   (serial)
```

resolve alone is already bigger than the entire parallel compute phase, on
a small grid, before any of this got contested. by *"Amdhals law"*, if the
serial fraction is `(0.115 + 0.003) / 0.200 = 0.59`, the maximum possible
speedup from parallelizing compute ALONE, with infinite threads, is 
 1 / 0.59 ~ 1.7x

The peaks (1.27x, 1.49x, 1.34x) sit right below that ceiling,
which is exactly the shape youd expect onceyou account for
threading overhead on top of a theoretical bound. this isnt a vague
"needs more threads" problem, the resolver is the bottleneck, not the 
compute phase, and no amount of chunk tuning fixes that.

this single stats sample is illustrative (taken at a different grid size
than the bench sweep, and only at one tick), not a rigorous proof for every
config. so, the structural story (resolve+apply serial, non-trivial cost,
growing with command count) should generalize. confirming that properly is
the single highest-leverage remaining task.

### the actionable fix

`resolver.cpp` uses `std::unordered_map<int64_t, int>` for `cellOwner` and
`touchCount`. hash map operations (hashing, bucket allocation, cache
misses on scattered buckets) are expensive relative to whats actually
needed here: cell coordinates are already bounded integers. a flat
`std::vector<int>` sized `WIDTH*HEIGHT`, indexed directly by
`y*WIDTH+x`, replaces every hash lookup with a plain array read. this
would very likely cut resolves cost significantly BEFORE even attempting
to parallelize it, and a cheaper serial phase directly raises the Amdahl
ceiling on its own. idd do this before attempting to parallelize resolve
itself; its lower risk and the benchmark already exists to prove whether
it worked

---

## determinism

tested:

- same seed, same chunk count, repeated runs: hash-matched (`detcheck`)
- same seed, 12 different seeds, 40 ticks each, on randomly scattered
  worlds: hash-matched (`property_determinism_holds_across_many_seeds`)
- same seed, different chunk counts (1/2/8) each checked against itself:
  hash-matched (`property_determinism_holds_across_different_chunk_counts_too`)
  - note: different chunk counts are NOT expected to match EACH OTHER,
    only to reproduce themselves. per-chunk rng streams are seeded by
    chunk index, so a different chunk layout is legitimately a different
    run
- the full capstone scene (all 7 elements, portals included), 400 ticks,
  at chunk counts 1/4/8: hash-matched at each, particle counts stable and
  sane (1411/1394/1398 - different because different chunk counts ARE
  different runs, not a bug)
- 30 fuzz trials, 200-2000 deliberately adversarial commands each (garbage
  types, out-of-bounds coordinates), thrown directly at the resolver: zero
  crashes, zero invalid types left in the grid

### bugs this determinism testing actually caught

This thang wasnt decorative. two mass-conservation bugs
got found this way, both documented in in `notes.md`:

1. **SWAP could collide with an unrelated particles independent MOVE**,
   because MOVEs implicit source-cell clear was never tracked as a
   conflict. old resolver duplicated a particle and lost another
   for a completely ordinary sand-sinks-through-water scenario.
   found by noticing a hash that shouldnt have changed and refusing to
   shrug it off

2. **STEAMs aging counter effectively never incremented while steam was moving** 
   (which is most of the time), because the AGE command targeted
   a cell the same ticks MOVE was about to vacate. Steam never reliably
   converted back to water. this one had been in the codebase since steam
   was first written. found only while building PLSM, which turned out
   to have the identical bug before it ever shipped lol.

both should be fixed, both have permanent regression tests
(`swap_target_beats_an_independent_move_from_the_same_cell`,
`steam_still_ages_when_it_also_moves_same_tick`,
`plsm_still_ages_when_it_also_moves_same_tick`).

---

## ported elements

| Element | What it tests | Outcome |
|---|---|---|
| SAND, WATR, STEAM, FIRE | can the architecture express ordinary local cellular-automaton behavior at all | yes, this is what the whole thing was originally built around |
| ACID | can a local element affect a NEIGHBORs cell, not just its own | yes, zero resolver changes needed. `DELETE` was never actually "delete yourself", just "clear this cell", so this worked immediately |
| PLSM | a fully random-walk mover instead of gravity or a fixed rise/fall direction | yes, but exposed the steam aging bug in the process |
| PRTI / PRTO | the kinda hard one. arbitrary-distance action, needs global read-only state beyond neighbors | yes, with a correction along the way. first implementation (DELETE+CREATE pair) duplicated particles under contention. fixx: portal teleport is just a `MOVE` with a far-away destination instead of an adjacent one; `MOVE` was already atomic (source+destination tracked together), so this inherited that safety for nothin instead of needing a new mechanism. This also matches the exact hypothesis `far_reaching_reacs.md` made before any of this was built |

Picked ACID and PRTI/PRTO over SPRK/Pressure - SOAP-like object-graph elements, 
and SPRK/Pressures whole-network or whole-grid dependencies, are  
a different category (see `far_reaching_reacs.md`). Testing "does a local element bend naturally" 
needed a local element; testing "does a far-reaching one bend or fight"
needed something whose difficulty was really about action-at-a-distance,
not about needing an entirely separate simulation subsystem. pressure and
SPRK both need new infrastructure (a second diffusion grid, a graph
traversal,) thats a bigger investment than justified. 
portals turned out to be gettable with "one lookup table", which is exactly why it
was the better first target.

---

## comparison to official TPT

**Caveat first:** ive looked at the TPT repos dir
structure, early in this project, and never read
`Simulation.cpp`, `Air.cpp`, or the electricity code. evreything below
about "official TPT" is general/public knowledge.

| | Official TPT | ParallelCore |
|---|---|---|
| Update model | each particle mutates the live world directly during its own update; correctness depends on update order | particles compute intent from a read-only snapshot; a resolver applies changes via a fixed deterministic rule, independent of any particular processing order |
| Particle loop threading | single-threaded | persistent thread pool, decide phase parallel |
| Determinism across thread counts | not applicable, not multithreaded | tested and holds. hash-matched across 7 chunk-count configs, 12+ seeds, a 7-element combined scene |
| Elements | ~250 | 9 |
| Global systems (pressure, gravity, electricity, photons, SOAP, STKM) | implemented and load-bearing | classified only, zero implementation |
| Save format / GUI / rendering | full .cps save/load, GUI, stamps | no parser, no GUI, ascii debug output only |
| Codebase | ~15+ years, production, large codebase | ~1 week, standalone spike, ~25 files |
| Test coverage | not something i have visibility into | 29 tests, ~1M assertions per run |


---
# State

Note:
this project answers a narrow architectural question. 
It does not come close to answering "should TPT be rewritten this
way" on its own. that needs someone who has read Simulation.cpp 
to judge how much of it would actually port cleanly.

---

## known limitations

- **resolve+apply are single-threaded**, and per the benchmark analysis,
  this is the actual scaling ceiling now, not compute. highest-priority
  future work.
- **portal index build is a third serial phase**, currently cheap (single
  grid scan) but would need attention if portal density ever got high
- **world size is a global mutable pair (`WIDTH`/`HEIGHT`), not a per-`World` field.** 
  fine for one world at a time (which is all this
  project ever needed), unsafe if something ever needs two differently-
  sized worlds alive simultaneously
- **far-reaching systems are 100% unimplemented**, classified, not built
- **no save file format support**, the capstone validation scene is
  hand-built, not loaded from a `.cps` file, because no parser
  exists. this was the original plan (load TPT saves for better testing) 
  and the substitution is a scope reduction, not a hidden equivalent
- **priority field on `Cmd` has no default opinion**, exists as a hook
  for "should CREATE beat DELETE beat MOVE" but nothing sets it except one
  test
- **portal teleport carries the whole particle** (type/temp/life/tmp) with
  no validation that the destination "wants" that type; fine for a
  prototype, would need channel-type-filtering for anything real
- **9 elements is not a representative fraction of TPT's actual 250+** -
  the capstone scene proves the pipeline holds up with several elements
  interacting, not that it would hold up at TPTs breadth

---

## future work, in priority order

1. **Replace the resolver's `unordered_map` with a flat array.** 
   Cheapest, lowest-risk, directly attacks the diagnosed bottleneck. 
   This before attempting to parallelize resolve, see whether it alone 
   moves the scaling numbers before adding threading complexity on top.

2. **Parallelize the conflict-detection pass** (pass 1 of resolveCommands)
   using a chunk+merge pattern like decide already has. Apply (pass 2)
   is harder since it writes shared state; would need either a
   cell-ownership partition or a different strategy, worth thinking about
   separately rather than assuming the same pattern works twice.

3. **Give `World` a size field** instead of global mutable
   `WIDTH`/`HEIGHT`, if multi-world scenarios ever matter.

4. **Attempt a far-reaching system**, Pressure is the best candidate.
   same diffusion shape as the existing HEAT propagation,
   "just" needs a second grid and its own double buffer.
   This would be a good example for LBPs question.

5. **Build a `.cps` save file parser** if testing against actual TPT
   saves ever becomes the priority again. binary format work is a genuinely 
   separate project from the simulation architecture work

6. **GPU port investigation.** The architectures
   read/compute/resolve/apply split has no shared mutable state during
   compute, which is GPU-shaped in principle

---

This is a validation prototype, not a library. 
Im usually not a backend kind of guy, but i did what i wanted.