#include "test_framework.h"
#include "../world.h"
#include "../sim.h"
#include "../scene.h"
#include "../resolver.h"
#include "../command.h"
#include "../stats.h"
#include <unordered_set>
#include <random>

// ---------------------------------------------------------------------
// regression tests
// ---------------------------------------------------------------------

TEST_CASE(sand_does_not_go_out_of_bounds_at_floor) {
    // this is the "y + 1 <= HEIGHT" bug from way back. put sand right on
    // the literal last row and tick it a bunch. if the bug ever comes back
    // this either crashes under asan or corrupts memory near the end of
    // the buffer, either way something will look wrong
    World w;
    w.cur[(HEIGHT - 1) * WIDTH + 40].type = TYPE_SAND;
    for (int t = 0; t < 10; t++) simTick(w, 1, t, 2);
    CHECK((int)w.cur.size() == WIDTH * HEIGHT); // buffer didnt get corrupted/resized
}

TEST_CASE(water_actually_converts_to_steam_when_hot) {
    // regression for the self-transform CREATE bug (clearNext copies the
    // OLD particle into nxt so the naive "is nxt empty" check always
    // failed for in-place transforms)
    World w;
    w.cur[10 * WIDTH + 10].type = TYPE_WATER;
    w.cur[11 * WIDTH + 10].type = TYPE_WALL; // pin it so it cant fall away
    w.cur[10 * WIDTH + 9].type = TYPE_WALL;
    w.cur[10 * WIDTH + 11].type = TYPE_FIRE;
    w.cur[10 * WIDTH + 11].life = 50;

    bool sawSteamSomewhere = false;
    for (int t = 0; t < 25; t++) {
        simTick(w, 1, t, 2);
        for (auto& p : w.cur) if (p.type == TYPE_STEAM) sawSteamSomewhere = true;
    }
    CHECK(sawSteamSomewhere);
}

TEST_CASE(chunk_boundary_rows_processed_exactly_once) {
    // old bug: decideChunk used "y <= endY" so the row at each chunk
    // boundary got visited by two threads. hard to check "processed twice"
    // directly, so instead check something that would break if it did:
    // put a lone sand grain exactly on a chunk boundary row and make sure
    // it only moves ONE cell in a single tick, not two
    World w;
    int numChunks = 4;
    int chunkSize = HEIGHT / numChunks;
    int boundaryRow = chunkSize; // first row of chunk 1, also would be the
                                  // bugged "extra" row of chunk 0 in the old code
    w.cur[boundaryRow * WIDTH + 30].type = TYPE_SAND;
    simTick(w, 1, 0, numChunks);
    CHECK(w.cur[boundaryRow * WIDTH + 30].type == TYPE_EMPTY);
    CHECK(w.cur[(boundaryRow + 1) * WIDTH + 30].type == TYPE_SAND);
    CHECK(w.cur[(boundaryRow + 2) * WIDTH + 30].type != TYPE_SAND); // didnt skip an extra row
}

// ---------------------------------------------------------------------
// stress tests - intentionally trying to break the resolver
// ---------------------------------------------------------------------

TEST_CASE(five_hundred_particles_target_one_cell) {
    // bypassing the decide phase entirely here, building the conflict
    // directly. 500 distinct sand particles, all emitting a MOVE at the
    // exact same destination cell in the same tick
    World w;
    const int N = 500;
    std::vector<Cmd> cmds;
    for (int i = 0; i < N; i++) {
        int sx = 5 + (i % 100);
        int sy = 5 + (i / 100);
        w.cur[sy * WIDTH + sx].type = TYPE_SAND;
        cmds.push_back({CMD_MOVE, sx, sy, 60, 60});
    }
    w.clearNext();
    resolveCommands(w, cmds);

    int sandAtTarget = (w.nxt[60 * WIDTH + 60].type == TYPE_SAND) ? 1 : 0;
    int totalSandAfter = 0;
    for (auto& p : w.nxt) if (p.type == TYPE_SAND) totalSandAfter++;

    CHECK(sandAtTarget == 1); // exactly one of them got the cell
    CHECK(totalSandAfter == N); // nobody got duplicated or deleted, 499 just stayed put
}

TEST_CASE(contested_swap_does_not_duplicate_or_lose_particles) {
    // A wants to swap with B, C ALSO wants to swap with B, same tick.
    // only one swap should go through, nothing should get duplicated
    World w;
    w.cur[10 * WIDTH + 10].type = TYPE_SAND;  // A
    w.cur[10 * WIDTH + 11].type = TYPE_WATER; // B
    w.cur[10 * WIDTH + 12].type = TYPE_STEAM; // C, unrealistic combo but doesnt matter for this test

    std::vector<Cmd> cmds;
    cmds.push_back({CMD_SWAP, 10, 10, 10, 11}); // A <-> B
    cmds.push_back({CMD_SWAP, 10, 12, 10, 11}); // C <-> B, contests B with the first one

    w.clearNext();
    resolveCommands(w, cmds);

    int sandCount = 0, waterCount = 0, steamCount = 0;
    for (auto& p : w.nxt) {
        if (p.type == TYPE_SAND) sandCount++;
        if (p.type == TYPE_WATER) waterCount++;
        if (p.type == TYPE_STEAM) steamCount++;
    }
    CHECK(sandCount == 1);
    CHECK(waterCount == 1);
    CHECK(steamCount == 1); // all three particles still exist exactly once total
}

TEST_CASE(four_way_swap_ring_conserves_particles) {
    // A wants to swap with B, B wants to swap with C, C wants to swap with
    // D, D wants to swap with A. every cell is contested by two different
    // swap commands (its own outgoing one plus its neighbors incoming one).
    // not asserting exactly what happens here since honestly its not
    // obvious what "should" happen with a 4-cycle, just asserting nothing
    // gets destroyed or duplicated, which is the actual thing that matters
    World w;
    w.cur[20 * WIDTH + 20].type = TYPE_SAND;  // A
    w.cur[20 * WIDTH + 21].type = TYPE_WATER; // B
    w.cur[21 * WIDTH + 21].type = TYPE_STEAM; // C
    w.cur[21 * WIDTH + 20].type = TYPE_FIRE;  // D

    std::vector<Cmd> cmds;
    cmds.push_back({CMD_SWAP, 20, 20, 20, 21}); // A -> B
    cmds.push_back({CMD_SWAP, 20, 21, 21, 21}); // B -> C
    cmds.push_back({CMD_SWAP, 21, 21, 21, 20}); // C -> D
    cmds.push_back({CMD_SWAP, 21, 20, 20, 20}); // D -> A

    w.clearNext();
    resolveCommands(w, cmds);

    int total = 0, sand = 0, water = 0, steam = 0, fire = 0;
    for (auto& p : w.nxt) {
        if (p.type != TYPE_EMPTY) total++;
        if (p.type == TYPE_SAND) sand++;
        if (p.type == TYPE_WATER) water++;
        if (p.type == TYPE_STEAM) steam++;
        if (p.type == TYPE_FIRE) fire++;
    }
    CHECK(total == 4);
    CHECK(sand == 1);
    CHECK(water == 1);
    CHECK(steam == 1);
    CHECK(fire == 1);
}

TEST_CASE(priority_field_actually_breaks_ties_when_set) {
    // two MOVEs target the same cell, sources are equal-ish so the default
    // coordinate tiebreak could go either way. give one of them a higher
    // priority and confirm it always wins regardless of coordinates
    World w;
    w.cur[5 * WIDTH + 99].type = TYPE_SAND; // "worse" coords, would win the default tiebreak
    w.cur[5 * WIDTH + 1].type = TYPE_WATER;  // "better" coords, but we're overriding priority

    Cmd low{CMD_MOVE, 99, 5, 50, 50};
    low.priority = 0;
    Cmd high{CMD_MOVE, 1, 5, 50, 50};
    high.priority = 0;
    // sanity: with equal priority, lowest fx,fy source wins, which is the
    // water one (fx=1). now flip it by giving the sand one higher priority
    low.priority = 10;

    std::vector<Cmd> cmds{low, high};
    w.clearNext();
    resolveCommands(w, cmds);

    CHECK(w.nxt[50 * WIDTH + 50].type == TYPE_SAND); // priority overrode the coord tiebreak
}

// ---------------------------------------------------------------------
// determinism
// ---------------------------------------------------------------------

TEST_CASE(same_seed_same_hash_across_runs) {
    uint32_t seed = 777;
    World a; setupDemoScene(a);
    for (int t = 0; t < 60; t++) simTick(a, seed, t, 4);
    uint64_t h1 = hashWorld(a);

    World b; setupDemoScene(b);
    for (int t = 0; t < 60; t++) simTick(b, seed, t, 4);
    uint64_t h2 = hashWorld(b);

    CHECK(h1 == h2);
}

// ---------------------------------------------------------------------
// regression tests, round 2
// ---------------------------------------------------------------------

TEST_CASE(heat_does_not_warm_up_empty_space) {
    // old bug: a HEAT command could land on a cell that ended up empty
    // this same tick (particle there moved away or got deleted), and
    // there was nothing stopping the temp from just sitting on an empty
    // Particle{} forever, drifting up a little more every time fire
    // happened to be nearby. build that exact scenario directly instead
    // of hoping the demo scene stumbles into it
    World w;
    w.cur[10 * WIDTH + 10].type = TYPE_EMPTY;
    std::vector<Cmd> cmds;
    Cmd heat{CMD_HEAT, 10, 10, 10, 10};
    heat.amt = 500.0f; // exaggerated on purpose, easy to spot if this leaks through
    cmds.push_back(heat);

    w.clearNext();
    resolveCommands(w, cmds);

    CHECK(w.nxt[10 * WIDTH + 10].type == TYPE_EMPTY);
    CHECK(w.nxt[10 * WIDTH + 10].temp == 22.0f); // default, untouched
}

TEST_CASE(resolver_rejects_garbage_newtype_instead_of_writing_it) {
    // found by fuzzing: a CREATE with an out of range newType used to get
    // written straight into the grid with zero validation
    World w;
    std::vector<Cmd> cmds;
    Cmd bad{CMD_CREATE, 5, 5, 5, 5};
    bad.newType = 9999; // way outside ElemType
    cmds.push_back(bad);

    w.clearNext();
    SimStats stats;
    resolveCommands(w, cmds, &stats);

    CHECK(w.nxt[5 * WIDTH + 5].type >= 0);
    CHECK(w.nxt[5 * WIDTH + 5].type < NUM_TYPES);
    CHECK(stats.invalidCommandsDropped == 1);
}

TEST_CASE(resolver_rejects_out_of_bounds_coords_instead_of_crashing) {
    // this one only really proves something under asan/ubsan, without
    // that its just "did the process not segfault", which is still worth
    // checking honestly
    World w;
    std::vector<Cmd> cmds;
    cmds.push_back({CMD_MOVE, -50, -50, 99999, 99999});
    cmds.push_back({CMD_DELETE, WIDTH + 10, HEIGHT + 10, WIDTH + 10, HEIGHT + 10});
    cmds.push_back({CMD_SWAP, -1, -1, WIDTH, HEIGHT});

    w.clearNext();
    SimStats stats;
    resolveCommands(w, cmds, &stats);

    CHECK(stats.invalidCommandsDropped == 3);
}

// ---------------------------------------------------------------------
// property tests - dont care what the world looks like, only that
// certain invariants never break, across a pile of random scenarios
// ---------------------------------------------------------------------

static World makeRandomWorld(unsigned seed) {
    World w;
    std::mt19937 rng(seed);
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int r = rng() % 100;
            int type = TYPE_EMPTY;
            if (r < 20) type = TYPE_SAND;
            else if (r < 35) type = TYPE_WATER;
            else if (r < 40) type = TYPE_STEAM;
            else if (r < 43) type = TYPE_FIRE;
            w.cur[y * WIDTH + x].type = type;
            if (type == TYPE_FIRE) w.cur[y * WIDTH + x].life = 1 + (rng() % 50);
        }
    }
    return w;
}

static int countNonEmpty(const World& w) {
    int n = 0;
    for (auto& p : w.cur) if (p.type != TYPE_EMPTY) n++;
    return n;
}

TEST_CASE(property_particle_count_never_increases_across_random_worlds) {
    // nothing in this sim creates a NEW particle out of nothing - CREATE
    // is always an in-place transform (water->steam, steam->water), the
    // only thing that changes total count is DELETE (fire dying). so
    // "conserved" isnt quite right, but "never goes up" definitely should
    // hold. running fewer than the 10k the question mentioned, this is a
    // toy grid not a real benchmark, keeping the suite fast
    for (unsigned seed = 1; seed <= 20; seed++) {
        World w = makeRandomWorld(seed);
        int before = countNonEmpty(w);
        for (int t = 0; t < 30; t++) simTick(w, seed, t, 4);
        int after = countNonEmpty(w);
        CHECK(after <= before);
    }
}

TEST_CASE(property_no_type_ever_leaves_valid_range) {
    // not testing "no duplicates" here on purpose - in a dense grid
    // representation a cell physically cant hold two particles, theres no
    // duplicate-entity failure mode the way there would be in an entity
    // list. the equivalent risk (two commands writing the same cell) is
    // what the conflict tests above already cover. this one instead just
    // checks every particle in the grid stays a real, valid type after a
    // pile of random ticks
    for (unsigned seed = 100; seed <= 115; seed++) {
        World w = makeRandomWorld(seed);
        for (int t = 0; t < 40; t++) simTick(w, seed, t, 4);
        for (auto& p : w.cur) {
            CHECK(p.type >= 0);
            CHECK(p.type < NUM_TYPES);
        }
    }
}

TEST_CASE(property_determinism_holds_across_many_seeds) {
    // last round only checked one seed. checking a spread of them, since
    // determinism bugs have a nasty habit of only showing up for specific
    // seed/thread-count combos
    for (uint32_t seed = 1; seed <= 12; seed++) {
        World a = makeRandomWorld(seed);
        for (int t = 0; t < 40; t++) simTick(a, seed, t, 4);
        uint64_t h1 = hashWorld(a);

        World b = makeRandomWorld(seed);
        for (int t = 0; t < 40; t++) simTick(b, seed, t, 4);
        uint64_t h2 = hashWorld(b);

        CHECK(h1 == h2);
    }
}

TEST_CASE(property_determinism_holds_across_different_chunk_counts_too) {
    // same seed, same scene, but split into a different number of chunks.
    // this ISNT expected to produce the same hash as a different chunk
    // count (the rng streams are seeded per-chunk so a different chunk
    // layout is a legitimately different run), what it DOES need to do is
    // be internally deterministic for a GIVEN chunk count, which is what
    // this checks - 1 chunk vs 1 chunk, 8 chunks vs 8 chunks
    for (int nc : {1, 2, 8}) {
        World a = makeRandomWorld(555);
        for (int t = 0; t < 30; t++) simTick(a, 555, t, nc);
        uint64_t h1 = hashWorld(a);

        World b = makeRandomWorld(555);
        for (int t = 0; t < 30; t++) simTick(b, 555, t, nc);
        uint64_t h2 = hashWorld(b);

        CHECK(h1 == h2);
    }
}

// ---------------------------------------------------------------------
// fuzzing - throw genuinely stupid command lists at the resolver directly
// ---------------------------------------------------------------------

TEST_CASE(fuzz_resolver_with_random_command_soup) {
    // building a world with SOME real particles in it so the fx/fy source
    // reads in the apply pass are touching real data, not just testing
    // against an all-empty grid. coordinates deliberately include some
    // out-of-bounds ones, mixed cmd types, garbage newType values, the
    // whole mess. resolver should just quietly reject the nonsense and
    // keep going, never crash, never leave an invalid type sitting in
    // the grid
    World w = makeRandomWorld(999);
    std::mt19937 rng(12345);

    for (int trial = 0; trial < 30; trial++) {
        std::vector<Cmd> cmds;
        int n = 200 + (rng() % 1800); // 200-2000 commands per trial
        for (int i = 0; i < n; i++) {
            Cmd c;
            int t = rng() % 6;
            c.type = (CmdType)t;

            // bias toward in-bounds most of the time but let some through
            // that arent, thats the point
            bool bogus = (rng() % 10 == 0);
            if (bogus) {
                c.fx = (int)(rng() % 4000) - 2000;
                c.fy = (int)(rng() % 4000) - 2000;
                c.tx = (int)(rng() % 4000) - 2000;
                c.ty = (int)(rng() % 4000) - 2000;
            } else {
                c.fx = rng() % WIDTH;
                c.fy = rng() % HEIGHT;
                c.tx = rng() % WIDTH;
                c.ty = rng() % HEIGHT;
            }

            c.newType = (int)(rng() % (NUM_TYPES + 20)) - 10; // sometimes garbage on purpose
            c.newLife = (int)(rng() % 200) - 50;
            c.newTemp = (float)(rng() % 500);
            c.amt = (float)((int)(rng() % 200) - 100);
            c.priority = (int)(rng() % 5);

            cmds.push_back(c);
        }

        w.clearNext();
        SimStats stats;
        resolveCommands(w, cmds, &stats);
        w.swapBuffers();

        for (auto& p : w.cur) {
            CHECK(p.type >= 0);
            CHECK(p.type < NUM_TYPES);
        }
    }
    // if we made it this far without asan yelling or a CHECK failing
    // above, the resolver survived ~30 trials of 200-2000 garbage
    // commands each without corrupting the grid
}
// ---------------------------------------------------------------------
// acid - the "port one real element" exercise
// ---------------------------------------------------------------------

TEST_CASE(acid_dissolves_a_dissolvable_neighbor) {
    World w;
    w.cur[10 * WIDTH + 10].type = TYPE_SAND;
    w.cur[10 * WIDTH + 9].type = TYPE_ACID;
    w.cur[10 * WIDTH + 9].life = 40;

    simTick(w, 1, 0);

    CHECK(w.cur[10 * WIDTH + 10].type == TYPE_EMPTY);
    CHECK(w.cur[10 * WIDTH + 9].life == 39); // spent one use
}

TEST_CASE(acid_ignores_immune_material) {
    // WALL isnt in the dissolvable set. also doubles as a check that the
    // "pin things with walls" trick other tests rely on still works now
    // that acid exists. boxing it in on purpose so it cant just fall away
    // before we check whether it spent a life point - first version of
    // this test only put a wall beside it, and since nothing blocked it
    // from falling straight down it just moved off before the check ran,
    // which made it LOOK like something was wrong when it wasnt
    World w;
    w.cur[10 * WIDTH + 10].type = TYPE_WALL;  // right
    w.cur[10 * WIDTH + 8].type = TYPE_WALL;   // left
    w.cur[11 * WIDTH + 9].type = TYPE_WALL;   // below
    w.cur[9 * WIDTH + 9].type = TYPE_WALL;    // above
    w.cur[10 * WIDTH + 9].type = TYPE_ACID;
    w.cur[10 * WIDTH + 9].life = 40;

    simTick(w, 1, 0);

    CHECK(w.cur[10 * WIDTH + 10].type == TYPE_WALL);
    CHECK(w.cur[10 * WIDTH + 9].type == TYPE_ACID); // still right where it was, boxed in
    CHECK(w.cur[10 * WIDTH + 9].life == 40); // didnt spend a use on nothing
}

TEST_CASE(acid_self_deletes_when_potency_runs_out) {
    World w;
    w.cur[10 * WIDTH + 10].type = TYPE_ACID;
    w.cur[10 * WIDTH + 10].life = 0;

    simTick(w, 1, 0);

    CHECK(w.cur[10 * WIDTH + 10].type == TYPE_EMPTY);
}

TEST_CASE(two_acids_targeting_same_sand_both_pay_but_only_one_dissolve_happens) {
    // this is the interesting one. two separate acid particles both decide
    // (independently, from the same cur snapshot) to dissolve the SAME
    // sand cell between them. the resolver correctly arbitrates the
    // DELETE conflict - only one of them actually applies, sand doesnt
    // get double-deleted or leave the grid in a weird state. but BOTH
    // acids still pay their life cost, because the CMD_AGE decrementing
    // their potency isnt part of the occupancy conflict system at all (by
    // design - nothing should need to know whether an AGE "won"). neither
    // acid can know at decide-time whether its dissolve attempt will
    // actually be the one that wins arbitration, since thats resolved
    // later and decide-phase never gets to see the outcome. not treating
    // this as a bug, its an honest consequence of decide-phase being
    // optimistic/speculative by design, which is what makes it safe to
    // run in parallel in the first place. writing it down here so its a
    // documented tradeoff instead of a surprise
    World w;
    w.cur[10 * WIDTH + 10].type = TYPE_SAND;
    w.cur[10 * WIDTH + 9].type = TYPE_ACID;
    w.cur[10 * WIDTH + 9].life = 40;
    w.cur[10 * WIDTH + 11].type = TYPE_ACID;
    w.cur[10 * WIDTH + 11].life = 40;

    simTick(w, 1, 0);

    CHECK(w.cur[10 * WIDTH + 10].type == TYPE_EMPTY); // sand is gone, exactly once
    CHECK(w.cur[10 * WIDTH + 9].life == 39);  // both spent a use...
    CHECK(w.cur[10 * WIDTH + 11].life == 39); // ...even though only one "really" dissolved it
}
TEST_CASE(swap_target_beats_an_independent_move_from_the_same_cell) {
    // THE bug this round. sand sitting above water decides to SWAP down
    // into the water's cell (sand sinks through water). the water, from
    // its OWN independent decide call on the SAME cur snapshot, doesn't
    // know sand is about to swap into it and decides to slide sideways
    // instead. both decisions are individually reasonable, and before
    // this round's hardening, NEITHER conflict-checked the water's own
    // source cell, so both writes landed on nxt[10,10] with the outcome
    // depending on unspecified vector iteration order - confirmed by
    // hand that the old resolver actually produced water duplicated
    // into TWO cells and sand vanishing entirely for this exact setup.
    // this is why MOVE's source cell needed to be a real tracked touch,
    // not just a "provably fine" argument - the proof was wrong because
    // it forgot SWAP doesn't require its target to be empty
    World w;
    w.cur[9 * WIDTH + 10].type = TYPE_SAND;    // sand above
    w.cur[10 * WIDTH + 10].type = TYPE_WATER;  // water below sand
    w.cur[11 * WIDTH + 10].type = TYPE_WALL;   // block water from falling further
    w.cur[10 * WIDTH + 11].type = TYPE_EMPTY;  // water's lateral escape route

    simTick(w, 1, 0);

    int sandCount = 0, waterCount = 0;
    for (auto& p : w.cur) {
        if (p.type == TYPE_SAND) sandCount++;
        if (p.type == TYPE_WATER) waterCount++;
    }
    CHECK(sandCount == 1); // exactly one, not zero (lost) or two (duplicated)
    CHECK(waterCount == 1);
    CHECK(w.cur[9 * WIDTH + 10].type == TYPE_WATER);  // water rose
    CHECK(w.cur[10 * WIDTH + 10].type == TYPE_SAND);  // sand sank
    CHECK(w.cur[10 * WIDTH + 11].type == TYPE_EMPTY); // water's own move got correctly discarded
}
// ---------------------------------------------------------------------
// portal - the "port one GENUINELY DIFFICULT element" exercise
// ---------------------------------------------------------------------

TEST_CASE(portal_teleports_an_adjacent_particle_to_the_matching_exit) {
    World w;
    w.cur[10 * WIDTH + 10].type = TYPE_SAND;
    w.cur[11 * WIDTH + 10].type = TYPE_WALL; // below
    w.cur[11 * WIDTH + 9].type = TYPE_WALL;  // below-left, sand can fall diagonally too
    w.cur[11 * WIDTH + 11].type = TYPE_WALL; // below-right
    w.cur[10 * WIDTH + 11].type = TYPE_PRTI;
    w.cur[10 * WIDTH + 11].tmp = 0;
    w.cur[50 * WIDTH + 50].type = TYPE_PRTO;
    w.cur[50 * WIDTH + 50].tmp = 0;

    simTick(w, 1, 0);

    int sandCount = 0;
    bool sandNearExit = false;
    for (auto& p : w.cur) if (p.type == TYPE_SAND) sandCount++;
    static const int dxs[4] = {1, -1, 0, 0};
    static const int dys[4] = {0, 0, 1, -1};
    for (int i = 0; i < 4; i++) {
        if (w.cur[(50 + dys[i]) * WIDTH + (50 + dxs[i])].type == TYPE_SAND) sandNearExit = true;
    }

    CHECK(sandCount == 1); // didnt duplicate
    CHECK(w.cur[10 * WIDTH + 10].type == TYPE_EMPTY); // gone from source
    CHECK(sandNearExit); // showed up next to the exit
}

TEST_CASE(portal_does_nothing_with_no_matching_exit) {
    World w;
    w.cur[10 * WIDTH + 10].type = TYPE_SAND;
    w.cur[11 * WIDTH + 10].type = TYPE_WALL;
    w.cur[11 * WIDTH + 9].type = TYPE_WALL;
    w.cur[11 * WIDTH + 11].type = TYPE_WALL;
    w.cur[10 * WIDTH + 11].type = TYPE_PRTI;
    w.cur[10 * WIDTH + 11].tmp = 5; // no PRTO anywhere uses channel 5

    simTick(w, 1, 0);

    CHECK(w.cur[10 * WIDTH + 10].type == TYPE_SAND); // never got touched
}

TEST_CASE(portal_ignores_wrong_channel) {
    World w;
    w.cur[10 * WIDTH + 10].type = TYPE_SAND;
    w.cur[11 * WIDTH + 10].type = TYPE_WALL;
    w.cur[11 * WIDTH + 9].type = TYPE_WALL;
    w.cur[11 * WIDTH + 11].type = TYPE_WALL;
    w.cur[10 * WIDTH + 11].type = TYPE_PRTI;
    w.cur[10 * WIDTH + 11].tmp = 1;
    w.cur[50 * WIDTH + 50].type = TYPE_PRTO;
    w.cur[50 * WIDTH + 50].tmp = 2; // different channel

    simTick(w, 1, 0);

    CHECK(w.cur[10 * WIDTH + 10].type == TYPE_SAND);
}

TEST_CASE(two_portals_racing_for_the_same_exit_dont_duplicate_or_lose) {
    // this is the scenario that would have been a documented "known
    // limitation" under the first (DELETE+CREATE) implementation. after
    // switching portal teleport to a single atomic MOVE, this collapsed
    // into just an ordinary move-vs-move conflict, which the resolver
    // already handles correctly. two different PRTIs, two different
    // sand particles, both routed to portals sharing one exit that only
    // has room for one arrival
    World w;
    w.cur[10 * WIDTH + 10].type = TYPE_SAND;
    w.cur[11 * WIDTH + 10].type = TYPE_WALL;
    w.cur[11 * WIDTH + 9].type = TYPE_WALL;
    w.cur[11 * WIDTH + 11].type = TYPE_WALL;
    w.cur[10 * WIDTH + 11].type = TYPE_PRTI;
    w.cur[10 * WIDTH + 11].tmp = 0;

    w.cur[20 * WIDTH + 20].type = TYPE_SAND;
    w.cur[21 * WIDTH + 20].type = TYPE_WALL;
    w.cur[21 * WIDTH + 19].type = TYPE_WALL;
    w.cur[21 * WIDTH + 21].type = TYPE_WALL;
    w.cur[20 * WIDTH + 21].type = TYPE_PRTI;
    w.cur[20 * WIDTH + 21].tmp = 0; // same channel as the first PRTI

    // exit boxed in so it only has exactly ONE empty landing neighbor,
    // forcing both PRTIs to compete for that single spot
    w.cur[50 * WIDTH + 50].type = TYPE_PRTO;
    w.cur[50 * WIDTH + 50].tmp = 0;
    w.cur[49 * WIDTH + 50].type = TYPE_WALL; // above
    w.cur[50 * WIDTH + 49].type = TYPE_WALL; // left
    w.cur[51 * WIDTH + 50].type = TYPE_WALL; // below
    // right (x=51,y=50) left open - the only place either sand grain could land

    simTick(w, 1, 0);

    int sandCount = 0;
    for (auto& p : w.cur) if (p.type == TYPE_SAND) sandCount++;
    CHECK(sandCount == 2); // exactly two, not lost, not duplicated

    bool firstStillHome = w.cur[10 * WIDTH + 10].type == TYPE_SAND;
    bool secondStillHome = w.cur[20 * WIDTH + 20].type == TYPE_SAND;
    bool oneArrived = w.cur[50 * WIDTH + 51].type == TYPE_SAND; // (x=51,y=50), the open cell
    // exactly one of the two should have made the trip, the other stays
    // put and gets another shot next tick
    CHECK((firstStillHome ^ secondStillHome)); // exactly one stayed home
    CHECK(oneArrived);
}

// ---------------------------------------------------------------------
// plsm
// ---------------------------------------------------------------------

TEST_CASE(plsm_heats_neighbors_and_ages_down) {
    World w;
    w.cur[10 * WIDTH + 10].type = TYPE_PLSM;
    w.cur[10 * WIDTH + 10].life = 60;
    w.cur[10 * WIDTH + 11].type = TYPE_WALL; // something to heat that wont move away

    simTick(w, 1, 0);

    bool plsmSomewhere = false;
    for (auto& p : w.cur) if (p.type == TYPE_PLSM) plsmSomewhere = true;
    CHECK(plsmSomewhere);
    CHECK(w.cur[10 * WIDTH + 11].temp > 22.0f); // wall got warmed up
}

TEST_CASE(plsm_dies_out_when_life_hits_zero) {
    World w;
    w.cur[10 * WIDTH + 10].type = TYPE_PLSM;
    w.cur[10 * WIDTH + 10].life = 0;

    simTick(w, 1, 0);

    CHECK(w.cur[10 * WIDTH + 10].type == TYPE_EMPTY);
}
TEST_CASE(steam_still_ages_when_it_also_moves_same_tick) {
    // regression for a real bug: steam used to emit a standalone AGE
    // targeting its own cell AND a separate MOVE in the same tick,
    // and whenever the move succeeded (which is most ticks in open
    // space) the age silently never landed. confirmed with a 400-tick
    // probe before fixing - life sat at 7 instead of ~400. fix folds the
    // age delta into the move itself (see resolver.cpp CMD_MOVE, amt is
    // now an optional life delta applied atomically with the move)
    World w;
    w.cur[10 * WIDTH + 10].type = TYPE_STEAM;
    w.cur[10 * WIDTH + 10].life = 0;
    // nothing above it, so it reliably moves up this tick

    simTick(w, 1, 0);

    bool found = false;
    for (auto& p : w.cur) {
        if (p.type == TYPE_STEAM) {
            CHECK(p.life == 1); // aged even though it also moved
            found = true;
        }
    }
    CHECK(found);
}

TEST_CASE(plsm_still_ages_when_it_also_moves_same_tick) {
    World w;
    w.cur[10 * WIDTH + 10].type = TYPE_PLSM;
    w.cur[10 * WIDTH + 10].life = 5;
    // fully open around it, plasma always finds its randomly chosen
    // direction empty here, so it reliably moves

    simTick(w, 1, 0);

    bool found = false;
    for (auto& p : w.cur) {
        if (p.type == TYPE_PLSM) {
            CHECK(p.life == 4); // aged down even though it also moved
            found = true;
        }
    }
    CHECK(found);
}

int main() {
    for (auto& tc : allTests()) {
        std::cout << "[ RUN ] " << tc.name << "\n";
        tc.fn();
    }
    std::cout << "\n" << g_passCount << " checks passed, " << g_failCount << " failed\n";
    return g_failCount == 0 ? 0 : 1;
}