## notes, see parallelcore.md for the real writeup

this is the last session on this project, at least for me. everything below this point in
notes.md is the informal log from getting here, kept as-is since its the
actual reasoning trail. the real deliverable for anyone picking this up
is `parallelcore.md` - architecture, benchmarks (real hardware
numbers this time), determinism evidence, ported elements, comparison to
official TPT, known limitations, and prioritized future work.

quick summary of what happened this sess:

- ported PORTAL (PRTI/PRTO). first attempt (DELETE+CREATE pair) had a
  real duplication bug, caught it by running the demo and noticing sand
  count grow, which is impossible. fixed by switching to a single atomic
  MOVE with an arbitrary-distance destination - which is exactly what
  far_reactions.md predicted portals would need, before any of this was
  built. nice to have that hypothesis actually pan out.
- ported PLSM. while building it, found that its aging counter (and
  STEAM's, which has had the SAME bug since steam was first written)
  effectively never decremented while the particle was moving, because
  AGE targeted a cell the same tick's MOVE was about to vacate. fixed
  both by folding the age delta into MOVE's own apply logic instead of
  emitting a separate command that gets lost.
- my bench.exe numbers (6c/12t) showed real but modest scaling
  (1.27x-1.49x peaks, not near-linear). used the stats overlay data to
  run an actual Amdahl's law calculation instead of just reporting
  numbers - resolve alone was already bigger than compute on a small
  grid, meaning the ~1.7x theoretical ceiling this implies matches the
  observed peaks almost exactly. the resolver's unordered_map is the
  next real bottleneck, not thread count.
- built the capstone scene (all 7 elements incl. portals), ran 400 ticks
  at 3 different chunk counts, hash-matched at each. this substitutes for
  "load real TPT saves" since no save-file parser exists - flagged
  explicitly as a scope reduction, not a hidden equivalent.
- wrote docs/PARALLELCORE.md as the final handoff document.

---

### bugs fixed this round

1. **bench.cpp warmup contamination.** bench showed chunks=1 getting FASTER as total tick count went up across
   separate process runs (2408 -> 5315 -> 5583 for the literal same
   config). that's not noise, that's a real measurement bug: the thread
   pool is a function-local static that constructs lazily on first use,
   and that first use happened to be inside chunks=1's timed window
   (its always the first config in the sweep). spawning ~12 OS threads on
   windows isnt free, so chunks=1 was paying a one-time startup cost that
   nothing else in the sweep had to pay, then that fixed cost got diluted
   more as total tick count grew, which is exactly the pattern in the
   pasted numbers. fixed by forcing one throwaway simTick call before the
   sweep even starts, plus a short untimed warmup per-config to flush
   first-touch page faults too. re-ran on the sandbox afterward and
   chunks=1 no longer looks anomalous relative to the rest, see the
   benchmark section below.

2. **a real mass-conservation bug in the resolver**, found while
   investigating why the demo hash changed for reasons that shouldn't
   have mattered. full writeup further down (not just
   defensive hardening) - short version: SWAP can target a cell thats
   simultaneously the source of some other particle's independent MOVE,
   and that collision was never being arbitrated. confirmed it actually
   duplicated particles in the old resolver, fixed, added a permanent
   regression test.

3. everything else from the "still open" list few days ago is either
   done now (dynamic world size) or explicitly still deferred (apply
   pass parallelization, priority defaults) - not new bugs, just tracked
   status, see the bottom of this file.

## dynamic world size

WIDTH/HEIGHT went from `const int` in types.h to regular mutable globals,
storage now lives in world.cpp, set via `setWorldSize(w, h)` before
constructing any World. rest of the codebase didnt need to change at all -
every file was already just reading WIDTH/HEIGHT as values, none of them
assumed compile-time-constant-ness (no fixed arrays sized off them,
nothing like that). the only contract is "call setWorldSize before you
build a World, dont call it after, we dont support resizing a live
world" - not enforced by anything, just a rule, would be easy to violate
by accident if you're not paying attention. flagging that as a real gap:
right now nothing stops you from creating a World, then calling
setWorldSize, then creating ANOTHER World with a different size while the
first one is still alive and using stale WIDTH/HEIGHT math against a
buffer sized for the old dimensions if you mixed them up. fine for how
its used today (bench.cpp does one size at a time, sequentially), would
want an actual per-World size field instead of a global if this needs to
support multiple differently-sized worlds coexisting.

## benchmark

rewrote bench.cpp to sweep both world size (64/128/256/512) and chunk
count (1/2/4/6/8/12/16), with the warmup fix. ran a short version (60
ticks/config instead of the 300 default, just to keep sandbox runtime
sane) to confirm the fix actually worked:

    hardware_concurrency reports: 1 threads

    World: 64x64
      1 chunks: 5090 ticks/sec
      2 chunks: 4846 ticks/sec
      4 chunks: 4870 ticks/sec
      8 chunks: 4737 ticks/sec
      16 chunks: 4367 ticks/sec

    World: 512x512
      1 chunks: 14 ticks/sec
      2 chunks: 15 ticks/sec
      ...

flat-to-slightly-declining as chunk count rises, which is exactly right
for 1 real core - more chunks here is pure queueing overhead with nothing
to overlap against, and its an HONEST flat instead of the misleading "more
chunks = faster" shape from last round's buggy measurement. this sandbox
cannot produce the number that matters. **please run `build.bat bench` on
the 3600 and paste the output** - that's the actual milestone-1 evidence,
I can't generate it here. if it scales the way your example table hoped
(rising through 6-8 "threads" then flattening/dropping past 12, since the
3600 is 6c/12t), that's real proof, not a hope.

one methodology note for when you run it: the current bench always uses
the SAME 42-seeded random fill regardless of size, and the tick range is
offset (1000+t) between warmup and measurement specifically so the rng
streams dont overlap and skew results. didnt bother varying the seed
across runs since the point here is comparing chunk counts against EACH
OTHER within one fill, not studying variance across different worlds -
if you want to sanity check for noise, just rerun the whole binary a
couple times and eyeball whether the shape holds.

## bigger bug

wrote the MOVE-source-touch change (see resolver.cpp) as what I thought
was pure defense-in-depth, backed by an argument that nothing could ever
legally collide with a MOVEs own source cell. the argument was wrong,
and I only found out because the demo's hash changed between rounds for
a scene that doesn't even use acid or a resized world - could have just
shrugged and said "hashes drift sometimes," but that's not actually a
thing that should happen here, so I bisected it instead of waving it
away.

turned out i forgot about SWAP specifically. MOVE and CREATE
both require their target to be empty in cur before they're ever emitted
- that part of the argument was fine. but SWAP does NOT require its
target to be empty, thats the entire point of it (sand swaps INTO water,
which is by definition occupied). so this is a real, legitimate scenario
that can happen with nothing but the elements weve had since round 1:

    sand sitting directly above water decides to SWAP down into the
    water's cell (sinks through it)

    the water, from its OWN independent decide call on the SAME cur
    snapshot, has no idea sand is about to swap into it, and separately
    decides to slide sideways since its own straight-down neighbor is
    blocked

both decisions are individually correct given what each particle can see.
before this round, water's own source-cell clear (an implicit side effect
of its MOVE, never tracked as a "touch") could land in the SAME nxt cell
that sand's SWAP was also writing to, with the outcome depending on
whichever command happened to come later in the merged command vector -
not resolved by the arbitration system at all, just an accident of
iteration order.

confirmed this concretely by building the old (pre-hardening) resolver
side by side with the current one and running the exact scenario
through both:

    old resolver:  sand count = 0, water count = 2
                   (sand vanished, water duplicated into two cells)

    current:       sand count = 1, water count = 1
                   (sand sank, water rose, water's own move correctly
                    got discarded once the swap took priority)

this bug existed since the very first resolver. it never showed
up in any hash comparison before because it needs water to actually reach
a sand pile from below, which doesnt happen in the demo scene until
somewhere around tick 50-60 - every previous determinism check either ran
short scenes or ran the property tests on random scatter-fills where this
specific geometry (sand directly on top of water, water blocked from
falling further, water able to move sideways) wasn't guaranteed to occur
in the tested tick window. the fuzz test wouldnt have caught it either -
it only asserts particle TYPES stay in valid range, it never checked
particle COUNT/duplication on the fuzzed command soup, which is a real
gap im noting rather than fixing this round (fuzzing with truly random
commands and expecting mass conservation doesnt quite make sense anyway
since CREATE can legitimately manufacture particles; the meaningful
version of that check needs realistic, decide-phase-shaped commands, which
is exactly what the new dedicated regression test does instead).

added `swap_target_beats_an_independent_move_from_the_same_cell` as a
permanent regression test for this exact scenario. this is now the most
important test in the suite as far as im concerned - its the one that
would have caught a real, silent mass-conservation bug that shipped for
multiple rounds without anyone noticing.

takeaway for the "is the rewrite worth it" question: the architecture
made this bug findable (a hash comparison catching an unexpected
divergence) and fixable (one targeted change, one new test, verified
against a hand-built counter-example) in about twenty minutes once the
hash mismatch triggered the investigation. that's a concrete example of
"easier reasoning" and "easier testing" actually paying off, not just
being asserted.

what actually happened when I ported it:

- **didnt need to touch Cmd, touchedCells, or the resolver at all.**
  ACID needs to emit a DELETE targeting a NEIGHBOR's cell, not its own -
  first time anything's done that with DELETE specifically (fire's HEAT
  already targeted neighbors, but DELETE up to now was always
  self-targeted, fire dying). DELETE's actual definition was never
  "delete yourself," it was always just "clear whatever cell fx,fy
  points at." this just worked. that's real evidence for LBPHacker's "is
  the rewrite worth it" question - a genuinely new element usage pattern
  didn't require touching the core system.

- **found non-obvious property while doing this:** if two acid
  particles both target the same sand cell in the same tick, the
  resolver correctly arbitrates which DELETE actually applies (sand
  doesn't get double-deleted, no corruption), but BOTH acids still spend
  their life/potency, because the CMD_AGE decrementing their own potency
  isn't part of the occupancy conflict system - by design, nothing
  should need to ask "did my AGE command win." neither acid can know at
  decide-time whether its dissolve attempt will be the one that survives
  arbitration, since that's resolved later and decide-phase never sees
  the outcome. this is a genuine architectural tradeoff (optimistic /
  speculative decide-phase commands), not a bug, and it's exactly the
  kind of thing this exercise was supposed to surface. wrote a test that
  pins this behavior down explicitly instead of letting it be a surprise
  later (two_acids_targeting_same_sand_both_pay_but_only_one_dissolve_happens).

- ran it on a 40x30 sand block with a 20x5 acid patch on top for 280
  ticks, sand count went 1200 -> 787 -> 441 -> ... -> 167, acid count
  held at 100 until particles started running out of potency around tick
  120 and started dying off (100 -> 90 -> 87). behaves sensibly, nothing
  weird in the numbers.

- also caught a bug in my own first test for this
  (acid_ignores_immune_material originally failed) - turned out to be a
  test bug, not a code bug: I'd placed the acid and the wall on the same
  row instead of vertically, so nothing stopped the acid from just
  falling away before the life check ran, which made it look like
  something was broken. fixed by boxing the acid in with walls on all 4
  sides so it genuinely cant move. worth mentioning since its a good
  example of why actually running a probe/repro before trusting a test
  failure matters - the first read of that failure looked like a real
  bug in the dissolve logic.

added TYPE_ACID, a standalone `acid_demo.cpp` (separate scene from the
main demo so existing hash-tracking stays undisturbed), and 4 dedicated
tests. full suite is now 20 tests, 993,667 checks, still 0 failures.

## far_reaching_reacs.md

wrote `far_reaching_reacs.md` - classification only, not solving anything
per the plan. covers pressure, gravity, electricity (split into normal
SPRK vs INST specifically since they turned out to need genuinely
different solutions), photons, portals, SOAP, STKM/FIGH, WIFI. explicitly
caveated at the top that this is general knowledge, not verified against
the actual forks source for any of these systems.

the one finding worth pulling out here: **portals might already mostly
work.** CMD_MOVEs fx,fy -> tx,ty was never required to be adjacent
cells, the resolver doesn't measure distance, it only asks "is this cell
contested." teleporting might just be a MOVE with a far-away destination
plus a read-only portal-pairing table passed alongside World during
compute. haven't built this, it's a hypothesis, but it's the cheapest
thing on that whole list to go test if the next round wants to actually
build something instead of just classify.

also flagged that "own pass" for the global-systems bucket isn't one
uniform thing - pressure/gravity want to run alongside the local pass,
INST wants to run strictly after local electricity diffusion, photons
might want their own mini-loop. that's a real design question, not
answered here, just written down so its not glossed over later.

## LBPs question 1: is the rewrite worth it

evidence, not just claims:

- **deterministic** - hash-matched across 7 runs last round, across 12
  different seeds and 3 different chunk counts this round
  (property_determinism_holds_across_many_seeds,
  property_determinism_holds_across_different_chunk_counts_too)
- **fuzz-tested** - 30 trials of 200-2000 deliberately malformed commands
  (out-of-bounds coords, garbage element types) fired straight at the
  resolver, caught two real bugs doing this (unvalidated newType,
  unchecked coordinates), both fixed with tests pinning the fix
- **cleaner API, evidenced not asserted** - porting ACID required zero
  changes to Cmd, the resolver, or the conflict-detection system, despite
  using DELETE in a way nothing had used it before (targeting a neighbor
  instead of itself)
- **easier reasoning, including catching my own mistake** - I originally
  argued MOVE's implicit source-clear could never collide with anything
  else, then found out that argument was wrong (it forgot SWAP doesn't
  require its target to be empty) by noticing an unexpected hash drift
  and actually chasing it down instead of shrugging it off. that's a real
  bug that shipped silently for two rounds, found and fixed with a
  targeted change and a permanent regression test in about twenty
  minutes once the mismatch triggered the investigation. see the writeup
  above - this is stronger evidence than a clean track record would have
  been, it shows the architecture makes bugs findable and fixable, not
  just that it happens to have none
- **easier testing** - 20 tests, 993,667 checks, runs in a few seconds,
  regression coverage for every bug found along the way
- **parallel compute** - infrastructure is real (persistent pool, not
  spawn-per-tick), benchmark methodology is now honest, but the actual
  scaling number has to come from your hardware, not this sandbox. this
  is the one item on this list that's "built and measurable" rather than
  "proven" - need your bench.exe output to close it out

five out of six are backed by something concrete already sitting in the
repo. the sixth just needs your 3600.

## LBPs question 2: far-reaching reactions

answered by pointing at far_reactions.md rather than repeating it here.
short version: your proposed local/global split holds up against the
classification - nothing on the far-reaching list fits the local
compute/resolve/apply pipeline as-is, but they don't all need the SAME
kind of "own pass" either (diffusion-shaped vs graph-traversal-shaped vs
raymarch-shaped vs object-graph-shaped vs broadcast-shaped are all
different enough that "own pass" was underselling how different they
are from each other, not just from the local systems).

## still open

- dynamic world size only has "dont call setWorldSize after creating a
  World" as a convention, not an enforced contract. fine for single-size
  usage (bench.cpp), would bite if something ever needed multiple
  differently-sized worlds alive at once
- resolver's apply pass still single threaded
- priority field still has no default opinion for create/delete/move
  ordering
- portals hypothesis not built, just written down as cheap-to-test
- didn't attempt PLSM or a second element - explicitly following "port
  ONE, see how it goes" before doing more
- still haven't touched the real Osmium Toy repo, still standalone,
  matches what's been said about priority

## how to build/run

    build.bat demo        -> demo.exe
    build.bat detcheck     -> detcheck.exe
    build.bat bench        -> bench.exe, world-size x chunk-count sweep
    build.bat stats        -> statsdemo.exe
    build.bat acid         -> acid_demo.exe, watch acid eat through sand
    build.bat tests        -> tests\test_runner.exe
    build.bat all          -> all of the above
