#include "world.h"

World::World() {
    cur.resize(WIDTH * HEIGHT);
    nxt.resize(WIDTH * HEIGHT);
}

bool World::inBounds(int x, int y) const {
    return x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT;
}

Particle& World::at(int x, int y) {
    // no bounds check here on purpose (perf), callers need to check inBounds
    // themselves. yes i know this bit me once already, see notes.md
    return cur[y * WIDTH + x];
}

Particle& World::atNext(int x, int y) {
    return nxt[y * WIDTH + x];
}

void World::clearNext() {
    nxt = cur; // baseline copy so particles that dont get a command just stay put
}

void World::swapBuffers() {
    cur.swap(nxt);
}
