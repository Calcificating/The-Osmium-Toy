#include "resolver.h"
#include <unordered_map>

static inline int64_t cellKey(int x, int y) {
    return (int64_t)y * WIDTH + x;
}

// which cells does this command actually write into in nxt. HEAT/AGE dont
// count, they modify a field on whatever already ended up in that cell,
// they dont change WHO occupies it, so they arent part of the occupancy
// fight. everything else (move, swap, delete, create) changes what particle
// lives in a cell so all of those need to go through arbitration
static void touchedCells(const Cmd& c, int64_t out[2], int& count) {
    count = 0;
    switch (c.type) {
        case CMD_MOVE:
        case CMD_CREATE:
            out[count++] = cellKey(c.tx, c.ty);
            break;
        case CMD_SWAP:
            out[count++] = cellKey(c.fx, c.fy);
            out[count++] = cellKey(c.tx, c.ty);
            break;
        case CMD_DELETE:
            out[count++] = cellKey(c.fx, c.fy);
            break;
        default:
            break; // HEAT, AGE
    }
}

// true if a is a "better" claim than b. priority first, then lower source
// coord wins. this is the ENTIRE opinion the resolver has about who should
// win a conflict, it doesnt know anything about what a or b actually do
static bool beats(const Cmd& a, const Cmd& b) {
    if (a.priority != b.priority) return a.priority > b.priority;
    return cellKey(a.fx, a.fy) < cellKey(b.fx, b.fy);
}

void resolveCommands(World& w, std::vector<Cmd>& cmds) {
    // pass 1: for every cell, figure out which single command "owns" it
    // this tick. a command has to win at EVERY cell it touches to survive
    // (matters for swap, which touches two cells at once - if it only wins
    // one of them we cant do half a swap, so the whole thing gets dropped)
    std::unordered_map<int64_t, int> cellOwner; // cell -> winning cmd index

    for (size_t i = 0; i < cmds.size(); i++) {
        int64_t touched[2];
        int n = 0;
        touchedCells(cmds[i], touched, n);
        for (int k = 0; k < n; k++) {
            int64_t cell = touched[k];
            auto it = cellOwner.find(cell);
            if (it == cellOwner.end()) {
                cellOwner[cell] = (int)i;
            } else if (beats(cmds[i], cmds[it->second])) {
                it->second = (int)i;
            }
        }
    }

    std::vector<char> survives(cmds.size(), 1);
    for (size_t i = 0; i < cmds.size(); i++) {
        int64_t touched[2];
        int n = 0;
        touchedCells(cmds[i], touched, n);
        for (int k = 0; k < n; k++) {
            if (cellOwner[touched[k]] != (int)i) {
                survives[i] = 0;
                break;
            }
        }
    }

    // pass 2: apply whatever survived. still switches on Cmd.type here but
    // only to know the SHAPE of the command (move copies fields, heat adds
    // a float), not what element emitted it
    for (size_t i = 0; i < cmds.size(); i++) {
        if (!survives[i]) continue;
        const Cmd& c = cmds[i];

        switch (c.type) {
            case CMD_MOVE: {
                w.atNext(c.tx, c.ty) = w.at(c.fx, c.fy);
                w.atNext(c.fx, c.fy) = Particle{};
                break;
            }
            case CMD_SWAP: {
                Particle a = w.at(c.fx, c.fy);
                Particle b = w.at(c.tx, c.ty);
                w.atNext(c.fx, c.fy) = b;
                w.atNext(c.tx, c.ty) = a;
                break;
            }
            case CMD_DELETE: {
                w.atNext(c.fx, c.fy) = Particle{};
                break;
            }
            case CMD_CREATE: {
                Particle np;
                np.type = c.newType;
                np.temp = c.newTemp;
                np.life = c.newLife;
                w.atNext(c.tx, c.ty) = np;
                break;
            }
            case CMD_HEAT: {
                w.atNext(c.tx, c.ty).temp += c.amt;
                break;
            }
            case CMD_AGE: {
                w.atNext(c.tx, c.ty).life += (int)c.amt;
                break;
            }
        }
    }
}
