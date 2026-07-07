#pragma once
#include "types.h"
#include <vector>

// current/next double buffer. everything READS from cur during decide phase
// and only the resolver WRITES into nxt. once resolve is done we swap.
class World {
public:
    World();

    Particle& at(int x, int y);       // cur, read-only in spirit but not enforced lol
    Particle& atNext(int x, int y);   // nxt

    bool inBounds(int x, int y) const;

    void clearNext(); // copies cur -> nxt so unmoved stuff just persists
    void swapBuffers();

    std::vector<Particle> cur;
    std::vector<Particle> nxt;
};
