#include "world.h"
#include "sim.h"
#include <iostream>
#include <chrono>
#include <cstdlib>
#include <thread>

// fills with actual work, empty cells barely cost anything since most
// decide fns bail immediately on TYPE_EMPTY
static void fillRandom(World& w, unsigned seed) {
    std::srand(seed);
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int r = std::rand() % 100;
            int t = TYPE_EMPTY;
            if (r < 30) t = TYPE_SAND;
            else if (r < 50) t = TYPE_WATER;
            w.cur[y * WIDTH + x].type = t;
        }
    }
}

// runs `ticks` timed ticks and returns ticks/sec. does its own short
// untimed warmup first so first-touch page faults / cache effects for
// THIS world dont leak into the timed window
static double measureTicksPerSec(World& w, int numChunks, int ticks) {
    for (int t = 0; t < 20; t++) simTick(w, 42, t, numChunks); // untimed warmup

    auto start = std::chrono::steady_clock::now();
    for (int t = 0; t < ticks; t++) {
        simTick(w, 42, 1000 + t, numChunks); // offset tick number so rng state doesnt overlap the warmup
    }
    auto end = std::chrono::steady_clock::now();

    double secs = std::chrono::duration<double>(end - start).count();
    return ticks / secs;
}

int main(int argc, char** argv) {
    int ticks = 300;
    if (argc > 1) ticks = std::atoi(argv[1]);

    unsigned hw = std::thread::hardware_concurrency();
    std::cout << "hardware_concurrency reports: " << hw << " threads\n";
    std::cout << "(numChunks below is WORK ITEMS, not OS threads - the pool\n";
    std::cout << " has a fixed thread count from hardware_concurrency, more\n";
    std::cout << " chunks past that just means more queued work per worker,\n";
    std::cout << " not more parallelism)\n\n";

    // IMPORTANT: force the thread pool to actually construct before ANY
    // timed measurement happens. last version of this benchmark didnt do
    // this, and the pool - which builds itself lazily on first use - got
    // constructed DURING the first timed config's window, making that one
    // config look artificially slow and everything after it look
    // artificially fast by comparison. this one warmup call outside the
    // loop is the actual fix
    {
        World warm;
        simTick(warm, 1, 0, 1);
    }

    int sizes[] = {64, 128, 256, 512};
    int chunkCounts[] = {1, 2, 4, 6, 8, 12, 16};

    for (int size : sizes) {
        setWorldSize(size, size);
        std::cout << "World: " << size << "x" << size << "\n";

        for (int nc : chunkCounts) {
            World w;
            fillRandom(w, 42);
            double tps = measureTicksPerSec(w, nc, ticks);
            std::cout << "  " << nc << " chunks: " << (int)tps << " ticks/sec\n";
        }
        std::cout << "\n";
    }

    return 0;
}