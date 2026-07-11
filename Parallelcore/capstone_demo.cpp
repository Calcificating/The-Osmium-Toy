#include "world.h"
#include "sim.h"
#include "scene.h"
#include <iostream>
#include <map>

void printCapstoneWorld(World& w) {
    for (int y = 0; y < HEIGHT; y += 2) {
        for (int x = 0; x < WIDTH; x += 2) {
            char c = '.';
            switch (w.cur[y * WIDTH + x].type) {
                case TYPE_SAND:  c = '#'; break;
                case TYPE_WATER: c = '~'; break;
                case TYPE_STEAM: c = '*'; break;
                case TYPE_FIRE:  c = '^'; break;
                case TYPE_ACID:  c = '@'; break;
                case TYPE_PLSM:  c = '%'; break;
                case TYPE_WALL:  c = '='; break;
                case TYPE_PRTI:  c = 'I'; break;
                case TYPE_PRTO:  c = 'O'; break;
                default: break;
            }
            std::cout << c;
        }
        std::cout << "\n";
    }
    std::cout << "----\n";
}

void printCounts(World& w) {
    std::map<int, int> counts;
    for (auto& p : w.cur) counts[p.type]++;
    std::cout << "sand=" << counts[TYPE_SAND] << " water=" << counts[TYPE_WATER]
               << " steam=" << counts[TYPE_STEAM] << " fire=" << counts[TYPE_FIRE]
               << " acid=" << counts[TYPE_ACID] << " plsm=" << counts[TYPE_PLSM]
               << " wall=" << counts[TYPE_WALL] << "\n";
}

int main() {
    World w;
    setupCapstoneScene(w);

    uint32_t seed = 2026;
    int totalTicks = 400;
    for (int t = 0; t < totalTicks; t++) {
        simTick(w, seed, t);
        if (t % 50 == 0) {
            std::cout << "tick " << t << " hash=" << hashWorld(w) << "\n";
            printCounts(w);
            printCapstoneWorld(w);
        }
    }
    return 0;
}