#include "test_framework.h"
#include "../world.h"
#include "../sim.h"
#include "../scene.h"
#include "../resolver.h"
#include "../command.h"
#include <unordered_set>

// ---------------------------------------------------------------------
// regression tests - one per bug we actually hit and fixed
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

int main() {
    for (auto& tc : allTests()) {
        std::cout << "[ RUN ] " << tc.name << "\n";
        tc.fn();
    }
    std::cout << "\n" << g_passCount << " checks passed, " << g_failCount << " failed\n";
    return g_failCount == 0 ? 0 : 1;
}
