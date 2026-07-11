#include "resolver.h"
#include <unordered_map>
#include <chrono>

static inline int64_t cellKey(int x, int y) {
    return (int64_t)y * WIDTH + x;
}

static inline bool inRange(int x, int y) {
    return x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT;
}

// is this command even worth looking at. anything with an out of bounds
// coordinate or a garbage newType gets dropped right here before it can
// touch anything. found the newType hole by fuzzing (see tests), a random
// CREATE with newType = -374 or whatever would just get written straight
// into the grid before this, no crash, just silently corrupt data
static bool isSaneCmd(const Cmd& c) {
    if (!inRange(c.fx, c.fy)) return false;
    if (!inRange(c.tx, c.ty)) return false;
    if (c.type == CMD_CREATE && (c.newType < 0 || c.newType >= NUM_TYPES)) return false;
    return true;
}

static void touchedCells(const Cmd& c, int64_t out[2], int& count) {
    count = 0;
    switch (c.type) {
        case CMD_MOVE:
            // used to only register the destination. the source cell also
            // gets implicitly cleared when a move applies, that write was
            // never going through arbitration at all. provable today that
            // nothing else can legally target a move's source cell in the
            // same tick (see notes.md), but that proof depends on every
            // element obeying "only target cells that were empty in cur",
            // and id rather the data structure enforce it than trust every
            // future element gets that invariant right
            out[count++] = cellKey(c.tx, c.ty);
            out[count++] = cellKey(c.fx, c.fy);
            break;
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
            break; // HEAT, AGE dont contest occupancy
    }
}

static bool beats(const Cmd& a, const Cmd& b) {
    if (a.priority != b.priority) return a.priority > b.priority;
    return cellKey(a.fx, a.fy) < cellKey(b.fx, b.fy);
}

static void tallyCmd(SimStats* stats, const Cmd& c) {
    if (!stats) return;
    switch (c.type) {
        case CMD_MOVE:   stats->cmdMove++;   break;
        case CMD_SWAP:   stats->cmdSwap++;   break;
        case CMD_DELETE: stats->cmdDelete++; break;
        case CMD_CREATE: stats->cmdCreate++; break;
        case CMD_HEAT:   stats->cmdHeat++;   break;
        case CMD_AGE:    stats->cmdAge++;    break;
    }
}

void resolveCommands(World& w, std::vector<Cmd>& cmds, SimStats* stats) {
    auto t0 = std::chrono::steady_clock::now();

    // pass 0: throw out anything nonsensical before it can touch a cell
    std::vector<char> alive(cmds.size(), 1);
    for (size_t i = 0; i < cmds.size(); i++) {
        if (!isSaneCmd(cmds[i])) {
            alive[i] = 0;
            if (stats) stats->invalidCommandsDropped++;
        } else {
            tallyCmd(stats, cmds[i]);
        }
    }

    // pass 1: figure out who owns each contested cell
    std::unordered_map<int64_t, int> cellOwner;
    std::unordered_map<int64_t, int> touchCount;
    for (size_t i = 0; i < cmds.size(); i++) {
        if (!alive[i]) continue;
        int64_t touched[2];
        int n = 0;
        touchedCells(cmds[i], touched, n);
        for (int k = 0; k < n; k++) {
            int64_t cell = touched[k];
            touchCount[cell]++;
            auto it = cellOwner.find(cell);
            if (it == cellOwner.end()) {
                cellOwner[cell] = (int)i;
            } else if (beats(cmds[i], cmds[it->second])) {
                it->second = (int)i;
            }
        }
    }

    if (stats) {
        for (auto& kv : touchCount) {
            if (kv.second > 1) stats->conflictedCells++;
        }
    }

    std::vector<char> survives(cmds.size(), 0);
    for (size_t i = 0; i < cmds.size(); i++) {
        if (!alive[i]) { survives[i] = 0; continue; }
        int64_t touched[2];
        int n = 0;
        touchedCells(cmds[i], touched, n);
        bool ok = true;
        for (int k = 0; k < n; k++) {
            if (cellOwner[touched[k]] != (int)i) { ok = false; break; }
        }
        survives[i] = ok ? 1 : 0;
        if (!ok && n > 0 && stats) stats->commandsRejected++;
    }

    auto t1 = std::chrono::steady_clock::now();

    // pass 2: apply
    for (size_t i = 0; i < cmds.size(); i++) {
        if (!survives[i]) continue;
        const Cmd& c = cmds[i];

        switch (c.type) {
            case CMD_MOVE: {
                w.atNext(c.tx, c.ty) = w.at(c.fx, c.fy);
                // amt doubles as an optional life delta here, applied
                // atomically with the move itself. this exists because of
                // a real bug: steam and plasma both want to age every
                // tick AND sometimes move every tick, and emitting AGE
                // as a separate command targeting their OWN cell doesnt
                // work if they also move away that same tick - the AGE
                // either lands on the cell after its already been
                // cleared (silently a no-op, guarded below) or gets
                // overwritten by the move's own clear, depending on
                // order. steam's "convert back to water at life>300"
                // counter was stuck near 0 for this exact reason for
                // several rounds before anyone noticed - defaults to 0
                // for anything that doesnt care (sand/water/acid/portal
                // moves dont set this, so this is a no-op for them)
                if (c.amt != 0.0f) {
                    w.atNext(c.tx, c.ty).life += (int)c.amt;
                }
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
                // self transforms (water->steam etc) always have fx==tx,
                // fy==ty. clearNext() already put the OLD particle in nxt
                // there so this is always fine to overwrite for those.
                // cross-cell creates dont happen anywhere in our elements
                // right now but the arbitration above still protects them
                // if that ever changes
                Particle np;
                np.type = c.newType;
                np.temp = c.newTemp;
                np.life = c.newLife;
                w.atNext(c.tx, c.ty) = np;
                break;
            }
            case CMD_HEAT: {
                Particle& t = w.atNext(c.tx, c.ty);
                // dont warm up empty space. used to happen when a fire
                // heated a neighbor that got deleted/moved away the same
                // tick, temp would just sit on an empty cell forever with
                // nothing to cool it back down
                if (t.type != TYPE_EMPTY) t.temp += c.amt;
                break;
            }
            case CMD_AGE: {
                Particle& t = w.atNext(c.tx, c.ty);
                if (t.type != TYPE_EMPTY) t.life += (int)c.amt;
                break;
            }
        }
    }

    auto t2 = std::chrono::steady_clock::now();
    if (stats) {
        stats->resolveMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        stats->applyMs = std::chrono::duration<double, std::milli>(t2 - t1).count();
    }
}