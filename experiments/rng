#pragma once
#include <random>
#include <cstdint>

using Rng = std::mt19937;

// deterministic per (seed, chunk, tick) combo. each chunk thread builds its
// own local rng from this instead of touching a shared/global one, so it
// doesnt matter which thread runs first, same inputs = same numbers every
// time. this is the actual fix for the rand() thread safety thing AND
// gets us determinism as a side effect
inline Rng makeChunkRng(uint32_t seed, int chunk, int tick) {
    uint64_t mix = (uint64_t)seed * 2654435761ULL;
    mix ^= (uint64_t)(chunk + 1) * 40503ULL;
    mix ^= (uint64_t)(tick + 1) * 2246822519ULL;
    mix ^= mix >> 17;
    return Rng((uint32_t)(mix & 0xffffffffu));
}
