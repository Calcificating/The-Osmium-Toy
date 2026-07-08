#include "sim.h"
#include "elements.h"
#include "resolver.h"
#include "command.h"
#include "rng.h"
#include <thread>
#include <vector>
#include <algorithm>

static void decideChunk(const World& w, int startY, int endY, CmdQueue& outQ,
                         uint32_t seed, int chunkIdx, int tick) {
    Rng rng = makeChunkRng(seed, chunkIdx, tick);
    for (int y = startY; y < endY; y++) {
        for (int x = 0; x < WIDTH; x++) {
            CmdQueue single;
            single.append(ComputeIntentForCell(w, x, y, rng));
            outQ.append(single.cmds);
        }
    }
}

void simTick(World& w, uint32_t seed, int tick, int numChunks) {
    // dont let someone pass 0 or something dumb and hang the sim
    numChunks = std::max(1, std::min(numChunks, HEIGHT));

    int chunkSize = HEIGHT / numChunks;

    std::vector<CmdQueue> localQueues(numChunks);
    std::vector<std::thread> workers;
    workers.reserve(numChunks);

    for (int c = 0; c < numChunks; c++) {
        int startY = c * chunkSize;
        int endY = (c == numChunks - 1) ? HEIGHT : startY + chunkSize;
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
