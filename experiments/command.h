#pragma once
#include <vector>

// particles dont touch the world directly, they just say what they want
// to happen and the resolver figures out who actually gets it
enum CmdType {
    CMD_MOVE,
    CMD_SWAP,
    CMD_DELETE,
    CMD_CREATE,
    CMD_HEAT
};

struct Cmd {
    CmdType type;
    int fx, fy;      // source cell
    int tx, ty;      // target cell (same as source for HEAT/DELETE)
    int newType;      // used by CREATE
    float amt;         // heat amount, or w/e else needs a float
};

struct CmdQueue {
    std::vector<Cmd> cmds;

    void push(Cmd c) {
        cmds.push_back(c);
    }
};
