## bugs from last time, fixed

1. resolver was checking w.at() (cur) to see if a MOVE dest was free, should
   have been checking w.atNext(). fixed, now checks atNext so it actually
   sees claims made earlier in the same tick.
2. sand boundary was "y + 1 <= HEIGHT" (should be <). fixed. verified by
   just reading the diff, didnt bother writing a specific repro for this
   one, its pretty obviously right now.
3. chunk boundary was "y <= endY" with endY = startY + chunkSize, so
   boundary rows got processed by two threads. fixed to proper exclusive
   range, last chunk absorbs the remainder if HEIGHT doesnt divide evenly
   by NUM_CHUNKS.
4. rand() called from multiple threads with no synchronization. fixed by
   giving each chunk its own seeded Rng built fresh every tick from
   (seed, chunkIdx, tick). this one mattered more than i expected, see
   determinism section below.
5. bonus bug i introduced AND caught this round: when i moved CREATE to be
   fully data driven, i forgot that self-transform creates (water->steam
   happens at the same cell, not a different one) were getting blocked by
   the same "is nxt empty" check as cross-cell creates. clearNext() already
   put the OLD particle in nxt at that cell so it never looked empty. fixed
   by special-casing fx==tx && fy==ty as always allowed. found this by
   writing a tiny isolated repro (pinned water next to fire with walls so
   it couldnt just fall away) instead of trusting the big demo scene, which
   never got fire close enough to water to even trigger the transform.
   lesson: the demo scene is not a test, need actual small repros for this
   stuff going forward.

## question 1: can elements be stateless?

yeah, basically already were in spirit last round, made it literal this
time. Sand/Water/Steam/Fire are namespaces now, ComputeIntent(world, x, y,
rng) -> vector<Cmd>, no shared state between calls, no member variables,
nothing. same inputs always produce the same list of commands (rng aside,
and even that is deterministic given the same Rng state going in).

one thing i noticed doing this: Steam::ComputeIntent used to do
"p.life++" directly on the particle since it had a mutable ref. cant do
that anymore obviously, so aging becomes a CMD_AGE command like everything
else. felt a little silly writing a whole command just to increment a
counter but it means literally 100% of world mutation goes through the
same pipe now, no exceptions, which is the point.

## question 2: is World read-only during compute?

yes now, compiler enforced not just convention. World::atNext has no const
overload at all, so if you hold a const World& (which is what
ComputeIntent takes), the write side of the buffer just isnt reachable,
wont compile. tried it on purpose (see draft 1 above), confirmed it breaks
loudly instead of silently doing the wrong thing.

only place still holding a non-const World& is resolver.cpp and
sim.cpp's outer simTick (which needs it for clearNext/swapBuffers). decide
side is fully locked out.

## question 3: can the resolver stay dumb?

closer than before. it now does a real two pass thing:

pass 1 groups commands by target cell and only cares "how many things want
this cell" - doesnt look at newType, doesnt know sand exists, just sees
coordinates and a cmd index. ties get broken by comparing source cell
coordinates, not by whichever chunk happened to get merged first.

pass 2 still switches on Cmd.type to know the SHAPE of each command (a
move copies a particle, a heat adds to temp) but that's structural
knowledge about the command system itself, not about elements. CREATE
pulls newType/newLife/newTemp straight off the cmd now instead of the old
"if (newType == TYPE_FIRE) life = 50" hardcode from last round, so thats
one less place the resolver was secretly element-aware.

still not 100% dumb: SWAP doesnt go through the claims map at all right
now, so it's not part of conflict resolution, it just always happens. fine
while only sand/water swap and nothing else contests those cells, but its
a real gap, not something i fixed, just flagging it.

## question 4: determinism

built detcheck.cpp for this. same seed, run 100 ticks, hash the world
(fnv-ish over type/life/temp per particle), do that 7 times total (2 in
one process + 5 more), compare hashes.

ran it for real, pasting actual output, not making this up:

    run1 hash: 3889698476444987310
    run2 hash: 3889698476444987310
    DETERMINISTIC (matched)
    extra run 0 hash: 3889698476444987310 ok
    extra run 1 hash: 3889698476444987310 ok
    extra run 2 hash: 3889698476444987310 ok
    extra run 3 hash: 3889698476444987310 ok
    extra run 4 hash: 3889698476444987310 ok
    all runs matched

also ran the whole binary 3 separate times as different processes (not
just within one run), same hash every time. the fix that actually mattered
here was the per-chunk seeded rng - before that, rand() being called from
multiple threads concurrently meant which thread got which random number
depended on scheduling, so two runs could easily diverge. worth noting
this WOULD have been silently nondeterministic before and i wouldnt have
known just from looking at it, the hash check is what would've caught it.

things that could still break determinism later and arent tested yet:
floats generally (temp accumulation order matters if we ever sum multiple
HEAT commands targeting the same cell instead of applying them in a fixed
order - right now they apply in cmds vector order which IS fixed given
fixed chunk merge order, but if resolver ever gets parallelized this needs
another look), and the SWAP gap mentioned above if two elements ever both
try to swap into the same pair of cells.

## question 5: command ownership, improvising here

didnt build much code for this, mostly just thinking through it:

right now a "command" doesnt have any real identity beyond fx/fy as its
source, and fx/fy also happens to be how we identify a particle (grid
position = identity, no persistent particle ids). that works fine as long
as a particle only ever emits commands about its own current cell, which
is true today, ComputeIntentForCell always calls the element fn with the
particle's own x,y.

the thing i'd actually want before this goes further: an explicit
ownerId on Cmd, separate from fx/fy, tied to a stable particle id instead
of a grid coordinate. reason being, position-as-identity breaks the moment
you want to track "this specific sand grain" across a move for anything
more than the immediate resolve (debugging, damage over time, whatever).
also matters for catching a bad element implementation: if ownership is
just "whatever fx/fy says", theres nothing stopping a buggy compute fn
from emitting a command with someone elses fx/fy in it and thered be no
way to tell. an ownerId the resolver could sanity check against "did this
chunk actually own this particle this tick" would catch that class of bug
for free.

not adding it yet since it means giving every particle a real id (a slot
index + generation counter probably, to handle reuse safely), which is a
bigger change than todays session. flagging it as the next real
architecture piece though, feels more important than making NUM_CHUNKS
configurable at this point.

## still on the todo pile, not touched this round

- SWAP not going through conflict resolution
- NUM_CHUNKS hardcoded to 4
- resolver still fully single threaded (pass 1 could probably parallelize
  per-chunk with a final merge, pass 2 harder since it writes shared state)
- no real particle id / ownership system yet, see question 5
- havent ported any of this into the real Simulation.cpp/Particle.h yet,
  still just the standalone spike

## how to build/run

    g++ -std=c++17 -O1 -pthread -Wall -Wextra world.cpp elements.cpp resolver.cpp sim.cpp scene.cpp main.cpp -o demo
    ./demo

    g++ -std=c++17 -O1 -pthread -Wall -Wextra world.cpp elements.cpp resolver.cpp sim.cpp scene.cpp detcheck.cpp -o detcheck
    ./detcheck
