#pragma once

// per-tick counters + timings. pass a pointer to simTick/resolveCommands if
// you want this filled in, pass nullptr if you dont care (normal demo run
// doesnt bother, statsdemo does). started as just the 4 cmd counters,
// kept bolting fields onto it as I needed more numbers for the writeup,
// probably could use a cleanup pass at some point but it works
struct SimStats {
    int cmdMove = 0;
    int cmdSwap = 0;
    int cmdDelete = 0;
    int cmdCreate = 0;
    int cmdHeat = 0;
    int cmdAge = 0;

    int conflictedCells = 0; // cells where 2+ commands fought over it
    int commandsRejected = 0; // commands that lost a conflict and got dropped
    int invalidCommandsDropped = 0; // OOB coords or garbage newType, dropped defensively

    double computeMs = 0.0;
    double resolveMs = 0.0; // conflict detection pass
    double applyMs = 0.0; // actual write-into-nxt pass
    double portalIndexMs = 0.0; // scanning for PRTOs before compute starts, see portal_index.h

    void reset() { *this = SimStats{}; }
};