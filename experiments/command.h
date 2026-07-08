#pragma once
#include <vector>

// split HEAT and AGE into separate cmd types instead of overloading amt's
// sign to mean two different things. was a dumb hack before, this is
// clearer and means the resolver doesnt need any special-casing at all
enum CmdType {
    CMD_MOVE,
    CMD_SWAP,
    CMD_DELETE,
    CMD_CREATE,
    CMD_HEAT,
    CMD_AGE
};

struct Cmd {
    CmdType type;
    int fx = 0, fy = 0;      // source cell
    int tx = 0, ty = 0;      // target cell

    // used by CREATE only. resolver just copies these onto the new
    // particle, it doesnt know or care what TYPE_FIRE means, the element
    // that emitted the CREATE decides its own starting values
    int newType = 0;
    int newLife = 0;
    float newTemp = 22.0f;

    float amt = 0.0f;         // used by HEAT (temp delta) and AGE (life delta)
};

struct CmdQueue {
    std::vector<Cmd> cmds;
    void push(const Cmd& c) { cmds.push_back(c); }
    void append(const std::vector<Cmd>& more) {
        cmds.insert(cmds.end(), more.begin(), more.end());
    }
};
