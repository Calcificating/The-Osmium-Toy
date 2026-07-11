#pragma once
#include "world.h"
#include "stats.h"

// seed/tick threaded through for deterministic per-chunk rng. numChunks is
// how many work items the tick gets split into, NOT the thread count
// anymore (see threadpool.h) - the pool has its own fixed thread count and
// just chews through however many chunks you ask for. stats is optional,
// pass nullptr if you dont want the overhead of tracking it (its cheap but
// why bother for the normal demo run)
void simTick(World& w, uint32_t seed, int tick, int numChunks = 4, SimStats* stats = nullptr);