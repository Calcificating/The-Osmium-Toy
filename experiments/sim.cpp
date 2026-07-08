#include "sim.h"
#include "elements.h"
#include "resolver.h"
#include "command.h"
#include "rng.h"
#include <thread>
#include <vector>

const int NUM_CHUNKS = 4; // still hardcoded, see notes.md, not fixing this one yet

static void decideChunk(const World& w, int startY, int endY, CmdQueue& outQ,
                         uint32_t seed, int chunkIdx, int tick) {
    Rng rng = makeChunkRng(seed, chunkIdx, tick);
    // fixed: was "y <= endY" before, which meant the row at endY got
    // processed twice, once by this chunk and once by the next chunk
    // starting there too. now endY is a proper exclusive bound and the
    // last chunk soaks up whatever remainder rows dont divide evenly
    for (int y = startY; y < endY; y++) {
        for (int x = 0; x < WIDTH; x++) {
            CmdQueue single;
            single.append(ComputeIntentForCell(w, x, y, rng));
            outQ.append(single.cmds);
        }
    }
}

void simTick(World& w, uint32_t seed, int tick) {
    int chunkSize = HEIGHT / NUM_CHUNKS;

    std::vector<CmdQueue> localQueues(NUM_CHUNKS);
    std::vector<std::thread> workers;

    for (int c = 0; c < NUM_CHUNKS; c++) {
        int startY = c * chunkSize;
        int endY = (c == NUM_CHUNKS - 1) ? HEIGHT : startY + chunkSize;
        // world passed by const ref now, compute side literally cannot
        // write into it even if it wanted to
        workers.emplace_back(decideChunk, std::cref(w), startY, endY,
                              std::ref(localQueues[c]), seed, c, tick);
    }
    for (auto& t : workers) t.join();

    std::vector<Cmd> merged;
    for (auto& q : localQueues) {
        merged.insert(merged.end(), q.cmds.begin(), q.cmds.end());
    }

    w.clearNext();
    resolveCommands(w, merged);
    w.swapBuffers();
}
