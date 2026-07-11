#include "world.h"
#include "sim.h"
#include "scene.h"
#include <iostream>
#include <chrono>

int main() {
    uint32_t seed = 2026;
    int ticks = 400;

    std::cout << "capstone scene: SAND, WATR, FIRE, ACID, PLSM, WALL, PRTI/PRTO\n";
    std::cout << "running " << ticks << " ticks, twice, at three different chunk counts\n\n";

    for (int nc : {1, 4, 8}) {
        World a;
        setupCapstoneScene(a);
        auto start = std::chrono::steady_clock::now();
        for (int t = 0; t < ticks; t++) simTick(a, seed, t, nc);
        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        uint64_t h1 = hashWorld(a);

        World b;
        setupCapstoneScene(b);
        for (int t = 0; t < ticks; t++) simTick(b, seed, t, nc);
        uint64_t h2 = hashWorld(b);

        int nonEmpty = 0;
        for (auto& p : a.cur) if (p.type != TYPE_EMPTY) nonEmpty++;

        std::cout << "chunks=" << nc << "  hash1=" << h1 << "  hash2=" << h2
                   << "  " << (h1 == h2 ? "MATCH" : "MISMATCH")
                   << "  particles=" << nonEmpty
                   << "  time=" << (int)ms << "ms\n";
    }

    return 0;
}