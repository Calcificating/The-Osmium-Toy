#pragma once
#include "world.h"

// seed and tick get threaded through so the per-chunk rng is fully
// deterministic and reproducible, not tied to wall clock or thread order
void simTick(World& w, uint32_t seed, int tick);
