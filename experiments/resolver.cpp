#include "resolver.h"
#include <unordered_map>

static inline int64_t cellKey(int x, int y) {
    return (int64_t)y * WIDTH + x;
}

void resolveCommands(World& w, std::vector<Cmd>& cmds) {
    // pass 1: who wants to claim which destination cell. only MOVE and
    // CREATE actually claim a slot, so those are the only ones that can
    // conflict with each other. this map has zero idea what a Cmd's newType
    // even means, it just groups by target coord
    std::unordered_map<int64_t, std::vector<int>> claims;
    for (size_t i = 0; i < cmds.size(); i++) {
        const Cmd& c = cmds[i];
        if (c.type == CMD_MOVE || c.type == CMD_CREATE) {
            claims[cellKey(c.tx, c.ty)].push_back((int)i);
        }
    }

    std::vector<char> dropped(cmds.size(), 0);
    for (auto& kv : claims) {
        auto& idxs = kv.second;
        if (idxs.size() <= 1) continue;

        // more than one command wants the same cell this tick. pick a
        // winner using the source cell as a deterministic tiebreak (lowest
        // key wins) instead of "whoever got merged into the list first",
        // which used to depend on chunk order and wasnt really a decision,
        // just an accident of how the queues got concatenated
        int winner = idxs[0];
        for (int idx : idxs) {
            if (cellKey(cmds[idx].fx, cmds[idx].fy) < cellKey(cmds[winner].fx, cmds[winner].fy)) {
                winner = idx;
            }
        }
        for (int idx : idxs) {
            if (idx != winner) dropped[idx] = 1;
        }
    }

    // known gap: SWAP isnt going through the claims map at all right now,
    // so two things swapping with overlapping cells in the same tick isnt
    // handled. havent hit it in practice with just sand/water but its not
    // actually safe. see notes.md

    // pass 2: apply whatever survived. this part does still switch on
    // Cmd.type, but only to know "a move copies fields A/B/C", not
    // "sand does X". CREATE pulls its starting values straight off the cmd
    // instead of the old hardcoded if(newType==TYPE_FIRE) branch
    for (size_t i = 0; i < cmds.size(); i++) {
        if (dropped[i]) continue;
        const Cmd& c = cmds[i];

        switch (c.type) {
            case CMD_MOVE: {
                if (w.atNext(c.tx, c.ty).type == TYPE_EMPTY) {
                    w.atNext(c.tx, c.ty) = w.at(c.fx, c.fy);
                    w.atNext(c.fx, c.fy) = Particle{};
                }
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
                // most CREATEs from our elements are actually self-transforms
                // (water -> steam happens at the same cell it started in).
                // clearNext() already copied the OLD particle into nxt at
                // that cell, so a plain "is nxt empty" check fails every
                // single time for these and the transform silently never
                // applies. caught this by actually watching steam count in
                // a test run instead of just eyeballing the ascii art.
                // same buffer-mixup shape as the move bug, annoyingly
                bool selfSlot = (c.fx == c.tx && c.fy == c.ty);
                if (selfSlot || w.atNext(c.tx, c.ty).type == TYPE_EMPTY) {
                    Particle np;
                    np.type = c.newType;
                    np.temp = c.newTemp;
                    np.life = c.newLife;
                    w.atNext(c.tx, c.ty) = np;
                }
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
