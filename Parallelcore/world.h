#pragma once
#include "types.h"
#include <vector>
#include <cstdint>

// call this before constructing any World if you want something other
// than the 120x90 default. doesnt touch an already-constructed World,
// this only sets what NEW World objects will size themselves to
void setWorldSize(int w, int h);

class World {
public:
    World();

    // const overload for anyone holding a const World& (ie compute/decide code).
    // non-const overload still exists for the resolver, which is the only
    // thing that should ever get a mutable World&
    const Particle& at(int x, int y) const;
    Particle& at(int x, int y);

    Particle& atNext(int x, int y);
    // deliberately NOT providing "const Particle& atNext(...) const" here.
    // if compute code is holding a const World&, atNext just isnt callable,
    // wont compile. thats the actual enforcement mechanism, not a comment
    // telling people to behave

    bool inBounds(int x, int y) const;

    void clearNext();
    void swapBuffers();

    std::vector<Particle> cur;
    std::vector<Particle> nxt;
};

// cheap fnv-ish hash over the whole particle grid, good enough to sanity
// check determinism between two runs, not trying to be cryptographically
// anything
uint64_t hashWorld(const World& w);