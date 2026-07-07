#include "sim.h"
#include "elements.h"
#include "resolver.h"
#include "command.h"
#include <thread>
#include <vector>

// just hardcoding 4 chunks for now instead of hardware_concurrency(),
// want a fixed number while im debugging so results are at least
// consistent between runs on my machine
const int NUM_CHUNKS = 4;

static void decideChunk(World& w, int startY, int endY, CmdQueue& outQ) {
    // endY is meant to be exclusive but see the caller, pretty sure
    // theres an off by one somewhere around the chunk boundaries
    for (int y = startY; y <= endY; y++) {
        if (y >= HEIGHT) continue;
        for (int x = 0; x < WIDTH; x++) {
            decideCell(w, x, y, outQ);
        }
    }
}

void simTick(World& w) {
    int chunkSize = HEIGHT / NUM_CHUNKS;

    std::vector<CmdQueue> localQueues(NUM_CHUNKS);
    std::vector<std::thread> workers;

    for (int c = 0; c < NUM_CHUNKS; c++) {
        int startY = c * chunkSize;
        int endY = startY + chunkSize; // should be exclusive top, but decideChunk uses <=
        workers.emplace_back(decideChunk, std::ref(w), startY, endY, std::ref(localQueues[c]));
    }
    for (auto& t : workers) t.join();

    // merge everything into one big list, not bothering with anything
    // smarter than just concatenating for now
    std::vector<Cmd> merged;
    for (auto& q : localQueues) {
        for (auto& c : q.cmds) merged.push_back(c);
    }

    w.clearNext();
    resolveCommands(w, merged); // still single threaded, one thing at a time
    w.swapBuffers();
}
