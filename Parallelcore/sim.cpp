#include "sim.h"
#include "elements.h"
#include "resolver.h"
#include "command.h"
#include "rng.h"
#include "threadpool.h"
#include "portal_index.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <chrono>

// pool gets built once on first use and reused for every tick after that,
// used to spawn/join std::thread fresh every single tick and that was
// eating a surprising amount of time, especially on windows apparently.
// sized off hardware_concurrency, falls back to 4 if that returns 0 (some
// vm/container setups do that)
static ThreadPool& getPool() {
    static ThreadPool pool(
        std::thread::hardware_concurrency() > 0
            ? (int)std::thread::hardware_concurrency()
            : 4
    );
    return pool;
}

static void decideChunk(const World& w, int startY, int endY, CmdQueue& outQ,
                         uint32_t seed, int chunkIdx, int tick, const PortalIndex& portals) {
    Rng rng = makeChunkRng(seed, chunkIdx, tick);
    // rough guess so the vector doesnt have to keep reallocating as it
    // grows, most cells are empty and emit nothing so this overshoots a
    // bit on purpose, better than growing one push_back at a time
    outQ.cmds.reserve((size_t)(endY - startY) * WIDTH / 4);
    for (int y = startY; y < endY; y++) {
        for (int x = 0; x < WIDTH; x++) {
            for (auto& cmd : ComputeIntentForCell(w, x, y, rng, portals)) {
                outQ.push(cmd);
            }
        }
    }
}

void simTick(World& w, uint32_t seed, int tick, int numChunks, SimStats* stats) {
    numChunks = std::max(1, std::min(numChunks, HEIGHT));

    // has to happen before the parallel chunks start, single threaded,
    // see portal_index.h for why this cant just be done lazily per-cell
    auto portalStart = std::chrono::steady_clock::now();
    PortalIndex portals = buildPortalIndex(w);
    auto portalEnd = std::chrono::steady_clock::now();
    if (stats) {
        stats->portalIndexMs = std::chrono::duration<double, std::milli>(portalEnd - portalStart).count();
    }

    auto computeStart = std::chrono::steady_clock::now();

    int chunkSize = HEIGHT / numChunks;
    std::vector<CmdQueue> localQueues(numChunks);
    ThreadPool& pool = getPool();

    for (int c = 0; c < numChunks; c++) {
        int startY = c * chunkSize;
        int endY = (c == numChunks - 1) ? HEIGHT : startY + chunkSize;
        pool.submit([&w, startY, endY, &localQueues, seed, c, tick, &portals]() {
            decideChunk(w, startY, endY, localQueues[c], seed, c, tick, portals);
        });
    }
    pool.waitAll();

    std::vector<Cmd> merged;
    for (auto& q : localQueues) {
        merged.insert(merged.end(), q.cmds.begin(), q.cmds.end());
    }

    auto computeEnd = std::chrono::steady_clock::now();
    if (stats) {
        stats->computeMs = std::chrono::duration<double, std::milli>(computeEnd - computeStart).count();
    }

    w.clearNext();
    resolveCommands(w, merged, stats);
    w.swapBuffers();
}