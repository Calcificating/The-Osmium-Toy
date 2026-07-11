#include "world.h"
#include "sim.h"
#include "scene.h"
#include <iostream>

// same seed -> run 100 ticks -> hash world -> repeat -> same hash?
// per calci's question 4. this is its own little main so it doesnt get
// mixed up with the normal demo run

int main() {
    uint32_t seed = 1337;
    int ticks = 100;

    World a;
    setupDemoScene(a);
    for (int t = 0; t < ticks; t++) simTick(a, seed, t);
    uint64_t h1 = hashWorld(a);

    World b;
    setupDemoScene(b);
    for (int t = 0; t < ticks; t++) simTick(b, seed, t);
    uint64_t h2 = hashWorld(b);

    std::cout << "run1 hash: " << h1 << "\n";
    std::cout << "run2 hash: " << h2 << "\n";
    std::cout << (h1 == h2 ? "DETERMINISTIC (matched)\n" : "NOT DETERMINISTIC (mismatch!!)\n");

    // run it a handful more times too, two matching runs could still be luck
    bool allMatch = true;
    for (int i = 0; i < 5; i++) {
        World c;
        setupDemoScene(c);
        for (int t = 0; t < ticks; t++) simTick(c, seed, t);
        uint64_t h = hashWorld(c);
        std::cout << "extra run " << i << " hash: " << h << (h == h1 ? " ok" : " MISMATCH") << "\n";
        if (h != h1) allMatch = false;
    }
    std::cout << (allMatch ? "all runs matched\n" : "some runs did not match, nondeterminism somewhere\n");

    return 0;
}
