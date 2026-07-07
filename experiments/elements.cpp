#include "elements.h"
#include <cstdlib>

// NOTE: using rand() here, called from multiple decide threads in sim.cpp.
// rand() has global state so this is probably not actually thread safe,
// works fine on my machine but should swap to a thread_local rng eventually

void decideSand(World& w, int x, int y, CmdQueue& q) {
    Particle& p = w.at(x, y);
    if (p.type != TYPE_SAND) return;

    // should be y+1 < HEIGHT, this lets y+1 == HEIGHT sneak through when
    // sand is sitting on the very last row
    if (y + 1 <= HEIGHT) {
        Particle& below = w.at(x, y + 1);
        if (below.type == TYPE_EMPTY) {
            q.push({CMD_MOVE, x, y, x, y + 1, 0, 0});
            return;
        }
        if (below.type == TYPE_WATER) {
            q.push({CMD_SWAP, x, y, x, y + 1, 0, 0});
            return;
        }
    }

    // try diagonal if straight down is blocked
    int dir = (rand() % 2 == 0) ? -1 : 1;
    for (int i = 0; i < 2; i++) {
        int dx = (i == 0) ? dir : -dir;
        int nx = x + dx, ny = y + 1;
        if (!w.inBounds(nx, ny)) continue;
        Particle& diag = w.at(nx, ny);
        if (diag.type == TYPE_EMPTY) {
            q.push({CMD_MOVE, x, y, nx, ny, 0, 0});
            return;
        }
    }
}

void decideWater(World& w, int x, int y, CmdQueue& q) {
    Particle& p = w.at(x, y);
    if (p.type != TYPE_WATER) return;

    // water near fire turns to steam, cheap heat check
    if (p.temp > 100.0f) {
        q.push({CMD_CREATE, x, y, x, y, TYPE_STEAM, 0});
        return;
    }

    if (y + 1 < HEIGHT) {
        Particle& below = w.at(x, y + 1);
        if (below.type == TYPE_EMPTY) {
            q.push({CMD_MOVE, x, y, x, y + 1, 0, 0});
            return;
        }
    }

    // spread sideways, pick a random direction so it doesnt always go left
    int dir = (rand() % 2 == 0) ? -1 : 1;
    int nx = x + dir;
    if (w.inBounds(nx, y) && w.at(nx, y).type == TYPE_EMPTY) {
        q.push({CMD_MOVE, x, y, nx, y, 0, 0});
    }
}

void decideSteam(World& w, int x, int y, CmdQueue& q) {
    Particle& p = w.at(x, y);
    if (p.type != TYPE_STEAM) return;

    p.life++; // just reusing life as an age counter here, kinda hacky

    // cools down and turns back into water after a while
    if (p.life > 300) {
        q.push({CMD_CREATE, x, y, x, y, TYPE_WATER, 0});
        return;
    }

    if (y - 1 >= 0 && w.at(x, y - 1).type == TYPE_EMPTY) {
        q.push({CMD_MOVE, x, y, x, y - 1, 0, 0});
        return;
    }

    int dir = (rand() % 2 == 0) ? -1 : 1;
    int nx = x + dir;
    if (w.inBounds(nx, y) && w.at(nx, y).type == TYPE_EMPTY) {
        q.push({CMD_MOVE, x, y, nx, y, 0, 0});
    }
}

void decideFire(World& w, int x, int y, CmdQueue& q) {
    Particle& p = w.at(x, y);
    if (p.type != TYPE_FIRE) return;

    if (p.life <= 0) {
        q.push({CMD_DELETE, x, y, x, y, 0, 0});
        return;
    }

    // heat up neighbors a bit, dumb version, doesnt bother with diagonals
    static const int dxs[4] = {1, -1, 0, 0};
    static const int dys[4] = {0, 0, 1, -1};
    for (int i = 0; i < 4; i++) {
        int nx = x + dxs[i], ny = y + dys[i];
        if (!w.inBounds(nx, ny)) continue;
        if (w.at(nx, ny).type != TYPE_EMPTY) {
            q.push({CMD_HEAT, nx, ny, nx, ny, 0, 15.0f});
        }
    }

    q.push({CMD_HEAT, x, y, x, y, 0, -1.0f}); // life--, reusing heat cmd for this, bit gross
}

void decideCell(World& w, int x, int y, CmdQueue& q) {
    Particle& p = w.at(x, y);
    switch (p.type) {
        case TYPE_SAND:  decideSand(w, x, y, q);  break;
        case TYPE_WATER: decideWater(w, x, y, q); break;
        case TYPE_STEAM: decideSteam(w, x, y, q); break;
        case TYPE_FIRE:  decideFire(w, x, y, q);  break;
        default: break; // empty / wall, nothing to do
    }
}
