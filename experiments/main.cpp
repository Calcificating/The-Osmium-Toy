#include "world.h"
#include "sim.h"
#include "scene.h"
#include <iostream>

void printWorld(World& w) {
    for (int y = 0; y < HEIGHT; y += 2) {
        for (int x = 0; x < WIDTH; x += 2) {
            char c = '.';
            switch (w.cur[y * WIDTH + x].type) {
                case TYPE_SAND:  c = '#'; break;
                case TYPE_WATER: c = '~'; break;
                case TYPE_STEAM: c = '*'; break;
                case TYPE_FIRE:  c = '^'; break;
                default: break;
            }
            std::cout << c;
        }
        std::cout << "\n";
    }
    std::cout << "----\n";
}

int main() {
    World w;
    setupDemoScene(w);

    uint32_t seed = 1337;
    int totalTicks = 200;
    for (int t = 0; t < totalTicks; t++) {
        simTick(w, seed, t);
        if (t % 20 == 0) {
            std::cout << "tick " << t << " hash=" << hashWorld(w) << "\n";
            printWorld(w);
        }
    }
    return 0;
}
