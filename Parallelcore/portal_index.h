#pragma once
#include "world.h"

// PRTI needs to find the matching PRTO to route a particle to, and
// scanning the whole grid for that on every single cell would be insane
// (every PRTI doing an O(WIDTH*HEIGHT) search every tick). so instead we
// scan ONCE per tick, single threaded, before the parallel compute phase
// even starts, and build a small read-only table. this is exactly what
// far_reactions.md guessed portals would need - "a read-only lookup
// table passed alongside World during compute" - turned out to be right,
// this is that table.
//
// this scan is a real serial cost thats NOT part of the resolve/apply
// bottleneck discussed in the benchmark writeup, its a THIRD serial
// phase, just a much cheaper one (single pass over the grid, no hashing,
// no conflict logic). worth knowing about if this ever needs to scale to
// worlds with lots of portals.

const int MAX_PORTAL_CHANNELS = 16;

struct PortalIndex {
    // -1,-1 means "no PRTO found for this channel this tick"
    int exitX[MAX_PORTAL_CHANNELS];
    int exitY[MAX_PORTAL_CHANNELS];

    bool hasExit(int channel) const {
        if (channel < 0 || channel >= MAX_PORTAL_CHANNELS) return false;
        return exitX[channel] >= 0;
    }
};

// builds the table by scanning w for PRTO cells. single threaded, call
// this before spawning the chunk jobs, not from inside one
PortalIndex buildPortalIndex(const World& w);