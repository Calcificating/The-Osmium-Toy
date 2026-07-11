#include "world.h"

World::World() {
    cur.resize(WIDTH * HEIGHT);
    nxt.resize(WIDTH * HEIGHT);
}

bool World::inBounds(int x, int y) const {
    return x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT;
}

const Particle& World::at(int x, int y) const {
    return cur[y * WIDTH + x];
}

Particle& World::at(int x, int y) {
    return cur[y * WIDTH + x];
}

Particle& World::atNext(int x, int y) {
    return nxt[y * WIDTH + x];
}

void World::clearNext() {
    nxt = cur;
}

void World::swapBuffers() {
    cur.swap(nxt);
}

uint64_t hashWorld(const World& w) {
    uint64_t h = 1469598103934665603ULL; // fnv offset basis
    for (const auto& p : w.cur) {
        h ^= (uint64_t)p.type;
        h *= 1099511628211ULL;
        h ^= (uint64_t)p.life;
        h *= 1099511628211ULL;
        // truncating temp to an int so tiny float noise doesnt make the
        // hash useless, 1000x scale gives us 3 decimal places of precision
        h ^= (uint64_t)(int64_t)(p.temp * 1000.0f);
        h *= 1099511628211ULL;
    }
    return h;
}
