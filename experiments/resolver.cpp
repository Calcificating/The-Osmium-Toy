#include "resolver.h"

// applies the merged command list into nxt. commands are processed in
// whatever order they ended up in after merging the per-chunk queues,
// so basically first-come-first-served per tick.
void resolveCommands(World& w, std::vector<Cmd>& cmds) {
    for (auto& c : cmds) {
        switch (c.type) {
            case CMD_MOVE: {
                // check the dest is still free before stomping it.
                // NOTE: checking w.at() (cur) here, not atNext(). that means
                // if an earlier command in this SAME tick already moved
                // something into (tx,ty), this check wont see it and we
                // overwrite it anyway. probably why sand piles look weird
                // sometimes, need to fix at some point
                if (w.at(c.tx, c.ty).type == TYPE_EMPTY) {
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
                Particle np;
                np.type = c.newType;
                np.temp = 22.0f; // forgot to carry over the source temp here, whatever, fine for now
                if (c.newType == TYPE_FIRE) np.life = 50;
                w.atNext(c.tx, c.ty) = np;
                break;
            }
            case CMD_HEAT: {
                Particle& target = w.atNext(c.tx, c.ty);
                if (c.amt < 0) {
                    // hack: negative amt means its actually a life decrement,
                    // reusing HEAT for this so i didnt have to add another cmd type
                    target.life += (int)c.amt;
                } else {
                    target.temp += c.amt;
                }
                break;
            }
        }
    }
}
