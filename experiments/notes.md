## whats new this round

1. resolver now handles SWAP and DELETE as real occupancy conflicts, not
   just MOVE/CREATE. previously SWAP just always applied no matter what,
   which was flagged as a known gap last round and turned out to matter
   (see contested_swap test).
2. added a `priority` field to Cmd. defaults to 0 for everything so
   nothing changes unless something explicitly sets it. exists because
   "which command SHOULD win a conflict" turned out to be a real design
   question, not just a coordinate tiebreak thing. see the
   simultaneous-create-delete-move discussion below, its still mostly
   unresolved, this is just the hook for it.
3. tests/ folder, 8 tests, 17 checks, all passing, compiled with
   -fsanitize=address,undefined so bounds bugs actually get caught by the
   tooling instead of me eyeballing ascii art and hoping.
4. bench.cpp, sweeps chunk count and prints ticks/sec.
5. numChunks pulled out of sim.cpp as a hardcoded const, now a runtime
   param to simTick (defaults to 4 so old call sites dont need to change).

## why the resolver rewrite

old version's conflict map only tracked MOVE and CREATE targets. SWAP
just ran unconditionally in the apply pass with zero arbitration. that
was fine as long as only sand/water swapping into each other ever
happened and nothing else contested those same cells, but its clearly not
a real guarantee, more of an accident of the demo scene not stressing it.

new version: every occupancy-changing command (MOVE, SWAP, DELETE,
CREATE) registers which cell(s) it writes into. for SWAP thats both
endpoints. one pass figures out a single winning command per cell
(priority first, then lowest source-coordinate as a deterministic
tiebreak), second pass figures out per-command whether it won at EVERY
cell it touches (matters for swap - if it only wins one of its two cells
it cant do half a swap, so the whole thing just doesnt happen this tick).
HEAT and AGE dont participate in this at all since they dont change who
occupies a cell, just tweak a field on whoever's already there.

worth writing down why this doesnt need the old "check if nxt is already
occupied" safety check anymore: since every decide fn already checks
w.at(target).type == TYPE_EMPTY off of `cur` before ever emitting a MOVE,
the ONLY way two commands can contest the same destination is if both
read it as empty in the same cur snapshot, which is exactly the case the
new arbitration handles. so nothing sneaks through anymore that wasnt
already an intentional multi-way race. thought about this for a while
before trusting it, then wrote the 500-particle test to make sure I
wasnt fooling myself, which passed clean.

## the simultaneous create/delete/move question (asked to think about this)

built a test for this (priority_field_actually_breaks_ties_when_set) but
honestly didnt land on an opinion about what the DEFAULT priority
ordering between different command types should be. right now everything
defaults to priority 0 and falls back to "lowest source coordinate wins",
which is deterministic but arbitrary, it doesnt encode any actual physics
intent like "an existing particle dying should make room before something
tries to move in" or the opposite. leaving this alone for now, its a
design decision more than a bug, and would rather make it on purpose
later than bake in a guess.

## four-way swap ring

did NOT assert a specific outcome for this one, the test just checks
nothing gets duplicated or destroyed. genuinely dont know what the "right"
answer is for a 4-cycle of contested swaps and didnt want to fake
confidence about it in a test assertion. if this project gets far enough
that a specific swap-ring behavior actually matters gameplay-wise, thats
the point to come back and decide, not now.

## what asan actually caught

nothing, this round - which is sort of the point, it means the earlier
bounds fix holds up under a much stricter check than eyeballing ascii
output. keeping asan in the test build going forward, its basically free
here since tests dont need to be fast.

## benchmark

ran on THIS machine (the sandbox), which reported 1 core. numbers below
are basically meaningless for judging parallel scaling, more chunks just
adds thread-spawn overhead with nothing to actually overlap with, and
thats exactly what shows up:

    World: 120x90
    Ticks per run: 300

    Chunks: 1
    Ticks/sec: 2376

    Chunks: 2
    Ticks/sec: 2745

    Chunks: 4
    Ticks/sec: 2450

    Chunks: 8
    Ticks/sec: 1598

    Chunks: 16
    Ticks/sec: 1024

need to actually run this on my own machine (ryzen 5 3600, 6c/12t) to get
numbers that mean anything. also the world is still hardcoded to 120x90 at
compile time (WIDTH/HEIGHT consts in types.h), so cant do the 512x512 test
without a recompile. not fixing that this round, flagging it.

## still open / not doing this round

- world size still compile-time fixed, cant bench at different grid sizes
  without rebuilding
- resolver's apply pass is still single threaded. the conflict-detection
  pass (building cellOwner) is probably parallelizable per-chunk with a
  merge step similar to how decide works, apply pass is trickier since it
  writes shared state, would need to think about whether cells can be
  bucketed by chunk to make that safe
- ambient/empty-cell temperature isnt modeled at all. HEAT commands can
  land on an EMPTY cell (from fire heating a neighbor that later gets
  vacated) and that temp just sits there with nothing to cool it back
  down. probably fine for now since nothing reads empty-cell temp, but
  worth knowing about before this gets more elaborate
- priority field exists but nothing sets it to anything but 0 yet outside
  of the one test. no opinion baked in about create/delete/move ordering
- still just the standalone spike, havent started looking at porting any
  of this into src/simulation in the real repo

## how to build/run

    # main demo
    g++ -std=c++17 -O1 -pthread -Wall -Wextra world.cpp elements.cpp resolver.cpp sim.cpp scene.cpp main.cpp -o demo
    ./demo

    # determinism check
    g++ -std=c++17 -O1 -pthread -Wall -Wextra world.cpp elements.cpp resolver.cpp sim.cpp scene.cpp detcheck.cpp -o detcheck
    ./detcheck

    # tests (from inside tests/)
    g++ -std=c++17 -O0 -g -pthread -fsanitize=address,undefined -Wall -Wextra ../world.cpp ../elements.cpp ../resolver.cpp ../sim.cpp ../scene.cpp test_runner.cpp -o test_runner
    ./test_runner

    # benchmark, optional tick count arg (default 200)
    g++ -std=c++17 -O2 -pthread -Wall -Wextra world.cpp elements.cpp resolver.cpp sim.cpp bench.cpp -o bench
    ./bench 300
