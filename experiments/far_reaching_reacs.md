# far reaching reactions

not makin any of these, jus classifying. goal is to know
which bucket each one falls into before trying to touch any of them.

**caveat up front:** this is from general/public knowledge of how TPT
works, not from reading the actual code. treat the "why difficult" and 
"possible fit" columns as informed hypotheses to go verify against the 
real code, not settled fact.

| System | Local (single tick)? | Why difficult | Possible fit |
|---|---|---|---|
| Pressure/velocity (air) | mostly yes - each cell reads its immediate neighbors and averages | not actually hard mechanically, its the same shape as our HEAT diffusion. the catch is it usually runs on a COARSER grid than the particle grid (one pressure cell per NxN block of pixels in real TPT), needs its own double buffer, and needs a defined order relative to particle movement for determinism | own pass, own buffer, same diffusion-style local update we already do for temperature, just a second grid at a different resolution |
| Gravity (Newtonian mode) | no, if done as true pairwise n-body (everything pulls on everything) | O(n²) pairwise force is exactly the kind of thing that doesnt decompose into "read your neighbors." my understanding (unverified against the real source) is TPT actually approximates this as a field on a coarse grid rather than true pairwise, which would make it structurally close to Pressure above, not actually all-to-all every tick | if it IS a field approximation: own pass like pressure. if its genuinely pairwise: doesnt fit this architecture at all without a totally different approach (barnes-hut / spatial partitioning), would need to verify which one the real engine does before assuming |
| Electricity / SPRK (normal conductors) | yes - a charged cell only actually touches its immediate conductive neighbors per tick | this one is basically fine, its diffusion-shaped again, just along a path instead of in all directions | own pass, or possibly even fits our command model directly: a charged cell emits a "charge neighbor" command to adjacent conductive cells, same shape as fire emitting HEAT to neighbors already |
| Electricity / INST (instant wire) | no - by definition it propagates the ENTIRE connected run in one tick | this is the actually hard one in the electricity family. needs a real graph traversal (BFS/DFS along connected conductive cells) within a single tick, not a diffusion step | needs an adjacency structure over conductive cells (built or incrementally maintained as wires get created/destroyed) plus an event-queue/BFS traversal pass, separate from the local particle pass |
| Photons / light | no - a single photon can travel many cells in one tick, doesnt stop at neighbors | doesnt fit "read a few neighbors, decide, emit local commands" at all. a photons own per-tick update needs to read a whole line of cells along its path (refraction, absorption, what it hits), which is closer to raycasting than cellular automaton | treat photons as their own subsystem entirely, a raymarching pass separate from the grid-cell decide/resolve/apply loop, probably the one that looks LEAST like the rest of this architecture |
| Portals (PRTI/PRTO) | no in effect (arbitrary jump across the map) but MAYBE yes in mechanism | worth flagging as a pleasant surprise rather than a hard problem: our CMD_MOVE already allows fx,fy -> tx,ty to be any two cells, nothing in the resolver assumes theyre adjacent. teleporting might just be "a MOVE with a far away destination" plus a read-only portal-pairing lookup table passed alongside World during compute. havent actually built this to confirm, its a hypothesis, but its a cheap one to test later | if the hypothesis holds: no architecture change needed, just a lookup table. this is the one id actually want to prototype first out of this whole list, specifically because I suspect it might already just work |
| SOAP (connected bubble networks) | no | soap particles need to stay roughly the right distance from OTHER particles in their connected group, which can be several cells away through the chain. one particles "correct" move depends on particles its not adjacent to. this is a spring/constraint network, not a neighbor-read problem | needs to stop being "just a grid cell" and become a real object: a soap blob as a first-class thing referencing multiple cells, with its own constraint-solving step, decoupled from the per-cell local pass entirely |
| STKM/STKM2/FIGH (stickman characters) | no | same shape of problem as SOAP - joints/limbs need coordinated movement as one body, not independently decided per cell. this is closer to a tiny embedded physics engine than a cellular automaton | own object-based subsystem, same bucket as SOAP |
| WIFI (broadcast signal) | no - broadcasts to every WIFI element on the map instantly, no distance falloff at all | flagging this because its counterintuitive: despite being the MOST non-local thing on this list (its not even distance-based, it just ignores distance entirely), its probably one of the SIMPLER ones to implement. no graph, no topology, no traversal needed | doesnt need graph traversal like SPRK does. just needs a global collect-then-broadcast step: local decide phase can flag "this cell fired," a tiny global pass gathers all of those and stamps every WIFI receiver, done between resolve and the next tick |

## what this suggests about the two-tier split

the original simple idea:

    Local systems (sand, water, powders, liquids, fire, acid...)
        -> our existing compute/resolve/apply pipeline, unchanged

    Global systems (pressure, gravity-if-its-a-field, SPRK-INST,
    photons, SOAP, STKM, WIFI)
        -> each gets its OWN pass, with its own data structure suited to
           what it actually needs (a second diffusion grid for
           pressure/gravity, a graph+event-queue for INST, a raymarcher
           for photons, an object list for SOAP/STKM, a global
           collect-then-broadcast for WIFI)

the one thing id add: these global passes probably dont all need to run
at the SAME point relative to the local pass. pressure/gravity feel like
they want to run once per tick alongside (before or after) the local
particle pass. photons might want to run as their own mini-loop since a
single photon could need several "hops" resolved before the tick is
really done. INST specifically wants to run AFTER local electricity
diffusion has placed charges, not instead of it. so "own pass" isnt one
uniform thing, its more a small pipeline of passes with their own
ordering rules, which is a bigger design question than this doc is
trying to answer. just wanted to write down that "own pass" undersells
how much these differ from each other even within the "global" bucket.
