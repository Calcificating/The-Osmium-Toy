#include "world.h"
#include "sim.h"
#include <iostream>
#include <chrono>
#include <cstdlib>

// fills the world with a decent amount of actual work (empty world barely
// costs anything since most decide fns bail out immediately on TYPE_EMPTY)
static void fillRandom(World& w, unsigned seed) {
    std::srand(seed);
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int r = std::rand() % 100;
            ElemType t = TYPE_EMPTY;
            if (r < 30) t = TYPE_SAND;
            else if (r < 50) t = TYPE_WATER;
            w.cur[y * WIDTH + x].type = t;
        }
    }
}

int main(int argc, char** argv) {
    int ticks = 200;
    if (argc > 1) ticks = std::atoi(argv[1]);

    std::cout << "World: " << WIDTH << "x" << HEIGHT << "\n";
    std::cout << "Ticks per run: " << ticks << "\n\n";

    int chunkCounts[] = {1, 2, 4, 8, 16};
    for (int nc : chunkCounts) {
        World w;
        fillRandom(w, 42);

        auto start = std::chrono::steady_clock::now();
        for (int t = 0; t < ticks; t++) {
            simTick(w, 42, t, nc);
        }
        auto end = std::chrono::steady_clock::now();

        double secs = std::chrono::duration<double>(end - start).count();
        double tps = ticks / secs;

        std::cout << "Chunks: " << nc << "\n";
        std::cout << "Ticks/sec: " << (int)tps << "\n\n";
    }

    return 0;
}
