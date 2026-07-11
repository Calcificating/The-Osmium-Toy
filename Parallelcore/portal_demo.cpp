#include "world.h"
#include "sim.h"
#include "scene.h"
#include <iostream>

void printPortalWorld(World& w) {
    for (int y = 0; y < HEIGHT; y += 2) {
        for (int x = 0; x < WIDTH; x += 2) {
            char c = '.';
            switch (w.cur[y * WIDTH + x].type) {
                case TYPE_SAND:  c = '#'; break;
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

int main() {
    World w;
    setupPortalTestScene(w);

    uint32_t seed = 7;
    int totalTicks = 150;
    for (int t = 0; t < totalTicks; t++) {
        simTick(w, seed, t);
        if (t % 20 == 0) {
            int sandCount = 0;
            for (auto& p : w.cur) if (p.type == TYPE_SAND) sandCount++;
            std::cout << "tick " << t << " sand=" << sandCount << "\n";
            printPortalWorld(w);
        }
    }
    return 0;
}