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

    // this used to unconditionally push a standalone CMD_AGE here, then
    // separately decide whether to move. turned out to be a real bug:
    // whenever steam ALSO moved in the same tick (which is most ticks,
    // steam moves a lot), the AGE targeted the cell steam was ABOUT to
    // leave, and by the time apply ran that cell was either already
    // cleared by the move (AGE becomes a no-op, guarded) or got
    // overwritten by the move's own clear depending on order. either
    // way the age never actually landed on the particle that kept
    // existing at its NEW position. life sat near 0 basically forever as
    // long as steam kept moving, so it never hit the >300 threshold to
    // convert back to water. confirmed this with a 400 tick probe before
    // believing it - life was 7, not ~400.
    //
    // fix: if steam is ABOUT to move, fold the age delta into that same
    // MOVE command (see resolver.cpp, CMD_MOVE now optionally carries a
    // life delta via amt). only fall back to a standalone AGE if steam
    // isn't moving at all this tick, where targeting its own still-
    // occupied cell is safe.
    int wouldBeLife = p.life + 1;

    if (wouldBeLife > 300) {
        Cmd c{CMD_CREATE, x, y, x, y};
        c.newType = TYPE_WATER;
        c.newTemp = 22.0f;
        c.newLife = 0;
        out.push_back(c);
        return out;
    }

    if (y - 1 >= 0 && w.at(x, y - 1).type == TYPE_EMPTY) {
        Cmd mv{CMD_MOVE, x, y, x, y - 1};
        mv.amt = 1.0f;
        out.push_back(mv);
        return out;
    }

    int dir = (rng() % 2 == 0) ? -1 : 1;
    int nx = x + dir;
    if (w.inBounds(nx, y) && w.at(nx, y).type == TYPE_EMPTY) {
        Cmd mv{CMD_MOVE, x, y, nx, y};
        mv.amt = 1.0f;
        out.push_back(mv);
        return out;
    }

    // didn't move, safe to age itself directly
    out.push_back({CMD_AGE, x, y, x, y, 0, 0, 22.0f, 1.0f});
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

namespace Acid {
// deliberately simplified vs real TPT acid (which dissolves most things
// with different probabilities, produces gas sometimes, etc). the point
// of porting this one wasnt to nail the exact upstream behavior, it was
// to see if the architecture could express "a local element that reaches
// out and deletes a NEIGHBOR, not just itself" without a rewrite. keeping
// dissolvable set to just sand on purpose, WALL staying immune also means
// the "pin particles with walls" trick the tests use still works even
// with acid in the mix
static bool isDissolvable(int t) {
    return t == TYPE_SAND;
}

std::vector<Cmd> ComputeIntent(const World& w, int x, int y, Rng& rng) {
    std::vector<Cmd> out;
    const Particle& p = w.at(x, y);
    if (p.type != TYPE_ACID) return out;

    if (p.life <= 0) {
        out.push_back({CMD_DELETE, x, y, x, y});
        return out;
    }

    // one dissolve attempt per tick, random direction order so it doesnt
    // always eat rightward first. if it dissolves something this tick it
    // doesnt also move, feels more like "it stopped to eat" than a big
    // deal, wasnt trying to get the pacing exactly right
    static const int dxs[4] = {1, -1, 0, 0};
    static const int dys[4] = {0, 0, 1, -1};
    int startDir = rng() % 4;
    for (int i = 0; i < 4; i++) {
        int dir = (startDir + i) % 4;
        int nx = x + dxs[dir], ny = y + dys[dir];
        if (!w.inBounds(nx, ny)) continue;
        if (isDissolvable(w.at(nx, ny).type)) {
            // note: this targets the NEIGHBOR's own cell, not acid's own
            // cell. didnt have to change Cmd, touchedCells, or the
            // resolver at all for this to work - DELETE was always just
            // "clear whatever cell fx,fy points at", never actually
            // required to be the emitter's own cell, fire's HEAT already
            // targeted neighbors the same way. that this just worked is
            // kind of the whole point of doing this port
            out.push_back({CMD_DELETE, nx, ny, nx, ny});
            Cmd age{CMD_AGE, x, y, x, y};
            age.amt = -1.0f;
            out.push_back(age);
            return out;
        }
    }

    // nothing nearby to eat, just fall/spread like a liquid
    if (y + 1 < HEIGHT && w.at(x, y + 1).type == TYPE_EMPTY) {
        out.push_back({CMD_MOVE, x, y, x, y + 1});
        return out;
    }
    int dir2 = (rng() % 2 == 0) ? -1 : 1;
    int nx2 = x + dir2;
    if (w.inBounds(nx2, y) && w.at(nx2, y).type == TYPE_EMPTY) {
        out.push_back({CMD_MOVE, x, y, nx2, y});
    }
    return out;
}
}

namespace Plsm {
// simplified vs real TPT plasma (which has proper velocity/momentum and
// interacts with a bunch of other elements). main thing I wanted out of
// this one was "does something that DRIFTS randomly instead of falling
// or rising in a fixed direction" fit the same shape as fire/acid. it
// does - only real difference from fire is the movement step
std::vector<Cmd> ComputeIntent(const World& w, int x, int y, Rng& rng) {
    std::vector<Cmd> out;
    const Particle& p = w.at(x, y);
    if (p.type != TYPE_PLSM) return out;

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
            c.amt = 8.0f; // less than fire, plasma's cooler in this sim, not trying to match real numbers
            out.push_back(c);
        }
    }

    // same bug as steam had: cant emit a standalone AGE and ALSO move
    // in the same tick, the age just gets lost since it targets the cell
    // being vacated. fold the decrement into the move's own amt instead,
    // only emit a standalone AGE for the tick(s) it doesn't actually move
    int dir = rng() % 4;
    int nx = x + dxs[dir], ny = y + dys[dir];
    if (w.inBounds(nx, ny) && w.at(nx, ny).type == TYPE_EMPTY) {
        Cmd mv{CMD_MOVE, x, y, nx, ny};
        mv.amt = -1.0f;
        out.push_back(mv);
        return out;
    }

    Cmd age{CMD_AGE, x, y, x, y};
    age.amt = -1.0f;
    out.push_back(age);
    return out;
}
}

namespace Portal {
// PRTI is a stationary fixture, not something that falls or spreads.
// each tick it checks its 4 neighbors for something worth absorbing, and
// if it finds one AND the matching PRTO has room next to it, teleports
// it there.
//
// first version of this emitted a DELETE on the neighbor plus a separate
// CREATE at the landing spot. that was wrong and I caught it by actually
// running the demo, not by reasoning about it: sand count kept growing
// tick over tick, which is impossible if nothing manufactures sand. the
// bug was that DELETE and CREATE aren't atomic as a pair - if the
// absorbed particle's OWN independent decide-phase command (say, its own
// ordinary gravity MOVE) won arbitration over the source cell instead of
// my DELETE, the DELETE just silently didn't apply while the CREATE,
// touching a totally different and uncontested cell, applied anyway.
// net result: the particle stayed where it was AND got created again at
// the exit. duplicated.
//
// the fix is the thing far_reactions.md guessed at back when this was
// still just a classification exercise: teleporting is just a MOVE with
// a far-away destination instead of an adjacent one. MOVE was already
// atomic (source and destination tracked together, both have to be won
// for it to apply, see resolver.cpp), so routing the absorbed particle
// through a single CMD_MOVE instead of DELETE+CREATE inherits that
// atomicity for free instead of needing a whole new mechanism
static bool isPortable(int t) {
    return t != TYPE_EMPTY && t != TYPE_WALL && t != TYPE_PRTI && t != TYPE_PRTO;
}

std::vector<Cmd> ComputeIntent(const World& w, int x, int y, Rng& rng, const PortalIndex& portals) {
    std::vector<Cmd> out;
    const Particle& p = w.at(x, y);
    if (p.type != TYPE_PRTI) return out;

    int channel = p.tmp;
    if (!portals.hasExit(channel)) return out; // no matching PRTO exists right now

    int ex = portals.exitX[channel];
    int ey = portals.exitY[channel];

    static const int dxs[4] = {1, -1, 0, 0};
    static const int dys[4] = {0, 0, 1, -1};
    int startDir = rng() % 4;
    for (int i = 0; i < 4; i++) {
        int dir = (startDir + i) % 4;
        int nx = x + dxs[dir], ny = y + dys[dir];
        if (!w.inBounds(nx, ny)) continue;
        if (!isPortable(w.at(nx, ny).type)) continue;

        // found a candidate, look for somewhere empty near the exit.
        // NOTE: this reads a cell nowhere near (x,y), could be clear
        // across the map. nothing about World or the resolver cares,
        // read access was never restricted to neighbors, only writes
        // ever needed arbitration
        for (int j = 0; j < 4; j++) {
            int lx = ex + dxs[j], ly = ey + dys[j];
            if (!w.inBounds(lx, ly)) continue;
            if (w.at(lx, ly).type != TYPE_EMPTY) continue;

            // single MOVE, fx,fy = the neighbor being absorbed (not
            // PRTI's own position), tx,ty = the landing spot. carries the
            // whole particle (type/temp/life/tmp) across automatically
            // since thats just what MOVE's apply logic already does
            out.push_back({CMD_MOVE, nx, ny, lx, ly});
            return out;
        }
        // found something to send but nowhere for it to land this tick,
        // stop looking at other neighbors, try again next tick
        break;
    }
    return out;
}
}

std::vector<Cmd> ComputeIntentForCell(const World& w, int x, int y, Rng& rng, const PortalIndex& portals) {
    switch (w.at(x, y).type) {
        case TYPE_SAND:  return Sand::ComputeIntent(w, x, y, rng);
        case TYPE_WATER: return Water::ComputeIntent(w, x, y, rng);
        case TYPE_STEAM: return Steam::ComputeIntent(w, x, y, rng);
        case TYPE_FIRE:  return Fire::ComputeIntent(w, x, y, rng);
        case TYPE_ACID:  return Acid::ComputeIntent(w, x, y, rng);
        case TYPE_PLSM:  return Plsm::ComputeIntent(w, x, y, rng);
        case TYPE_PRTI:  return Portal::ComputeIntent(w, x, y, rng, portals);
        default: return {};
    }
}