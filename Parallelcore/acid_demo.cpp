#include "world.h"
#include "sim.h"
#include "scene.h"
#include <iostream>

void printAcidWorld(World& w) {
    for (int y = 0; y < HEIGHT; y += 2) {
        for (int x = 0; x < WIDTH; x += 2) {
            char c = '.';
            switch (w.cur[y * WIDTH + x].type) {
                case TYPE_SAND:  c = '#'; break;
                case TYPE_WATER: c = '~'; break;
                case TYPE_STEAM: c = '*'; break;
                case TYPE_FIRE:  c = '^'; break;
                case TYPE_ACID:  c = '@'; break;
                case TYPE_WALL:  c = '='; break;
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
    setupAcidTestScene(w);

    uint32_t seed = 42;
    int totalTicks = 300;
    for (int t = 0; t < totalTicks; t++) {
        simTick(w, seed, t);
        if (t % 40 == 0) {
            int sandLeft = 0, acidLeft = 0;
            for (auto& p : w.cur) {
                if (p.type == TYPE_SAND) sandLeft++;
                if (p.type == TYPE_ACID) acidLeft++;
            }
            std::cout << "tick " << t << " sand=" << sandLeft << " acid=" << acidLeft << "\n";
            printAcidWorld(w);
        }
    }
    return 0;
}