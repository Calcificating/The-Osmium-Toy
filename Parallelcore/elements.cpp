#include "elements.h"

namespace Sand {
std::vector<Cmd> ComputeIntent(const World& w, int x, int y, Rng& rng) {
    std::vector<Cmd> out;
    const Particle& p = w.at(x, y);
    if (p.type != TYPE_SAND) return out;

    // fixed: was "y + 1 <= HEIGHT" before which let y+1==HEIGHT through
    // and read/wrote one row past the buffer when sand sat on the last row
    if (y + 1 < HEIGHT) {
        const Particle& below = w.at(x, y + 1);
        if (below.type == TYPE_EMPTY) {
            out.push_back({CMD_MOVE, x, y, x, y + 1});
            return out;
        }
        if (below.type == TYPE_WATER) {
            out.push_back({CMD_SWAP, x, y, x, y + 1});
            return out;
        }
    }

    int dir = (rng() % 2 == 0) ? -1 : 1;
    for (int i = 0; i < 2; i++) {
        int dx = (i == 0) ? dir : -dir;
        int nx = x + dx, ny = y + 1;
        if (!w.inBounds(nx, ny)) continue;
        if (w.at(nx, ny).type == TYPE_EMPTY) {
            out.push_back({CMD_MOVE, x, y, nx, ny});
            return out;
        }
    }
    return out;
}
}

namespace Water {
std::vector<Cmd> ComputeIntent(const World& w, int x, int y, Rng& rng) {
    std::vector<Cmd> out;
    const Particle& p = w.at(x, y);
    if (p.type != TYPE_WATER) return out;

    if (p.temp > 100.0f) {
        Cmd c{CMD_CREATE, x, y, x, y};
        c.newType = TYPE_STEAM;
        c.newTemp = p.temp; // carry the heat over, used to just reset to room temp
        c.newLife = 0;
        out.push_back(c);
        return out;
    }

    if (y + 1 < HEIGHT && w.at(x, y + 1).type == TYPE_EMPTY) {
        out.push_back({CMD_MOVE, x, y, x, y + 1});
        return out;
    }

    int dir = (rng() % 2 == 0) ? -1 : 1;
    int nx = x + dir;
    if (w.inBounds(nx, y) && w.at(nx, y).type == TYPE_EMPTY) {
        out.push_back({CMD_MOVE, x, y, nx, y});
    }
    return out;
}
}

namespace Steam {
std::vector<Cmd> ComputeIntent(const World& w, int x, int y, Rng& rng) {
    std::vector<Cmd> out;
    const Particle& p = w.at(x, y);
    if (p.type != TYPE_STEAM) return out;

    // used to do p.life++ directly on the particle right here, cant do that
    // anymore since w is const now. age is just another command, resolver
    // applies it same as everything else
    out.push_back({CMD_AGE, x, y, x, y, 0, 0, 22.0f, 1.0f});

    if (p.life > 300) {
        Cmd c{CMD_CREATE, x, y, x, y};
        c.newType = TYPE_WATER;
        c.newTemp = 22.0f;
        c.newLife = 0;
        out.push_back(c);
        return out;
    }

    if (y - 1 >= 0 && w.at(x, y - 1).type == TYPE_EMPTY) {
        out.push_back({CMD_MOVE, x, y, x, y - 1});
        return out;
    }

    int dir = (rng() % 2 == 0) ? -1 : 1;
    int nx = x + dir;
    if (w.inBounds(nx, y) && w.at(nx, y).type == TYPE_EMPTY) {
        out.push_back({CMD_MOVE, x, y, nx, y});
    }
    return out;
}
}

namespace Fire {
std::vector<Cmd> ComputeIntent(const World& w, int x, int y, Rng&) {
    std::vector<Cmd> out;
    const Particle& p = w.at(x, y);
    if (p.type != TYPE_FIRE) return out;

    if (p.life <= 0) {
        out.push_back({CMD_DELETE, x, y, x, y});
        return out;
    }

    static const int dxs[4] = {1, -1, 0, 0};
    static const int dys[4] = {0, 0, 1, -1};
    for (int i = 0; i < 4; i++) {
        int nx = x + dxs[i], ny = y + dys[i];
        if (!w.inBounds(nx, ny)) continue;
        if (w.at(nx, ny).type != TYPE_EMPTY) {
            Cmd c{CMD_HEAT, nx, ny, nx, ny};
            c.amt = 15.0f;
            out.push_back(c);
        }
    }

    Cmd age{CMD_AGE, x, y, x, y};
    age.amt = -1.0f;
    out.push_back(age);
    return out;
}
}

std::vector<Cmd> ComputeIntentForCell(const World& w, int x, int y, Rng& rng) {
    switch (w.at(x, y).type) {
        case TYPE_SAND:  return Sand::ComputeIntent(w, x, y, rng);
        case TYPE_WATER: return Water::ComputeIntent(w, x, y, rng);
        case TYPE_STEAM: return Steam::ComputeIntent(w, x, y, rng);
        case TYPE_FIRE:  return Fire::ComputeIntent(w, x, y, rng);
        default: return {};
    }
}
