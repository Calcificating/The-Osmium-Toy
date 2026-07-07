#include "world.h"
#include "sim.h"
#include <iostream>

void printWorld(World& w) {
    // downsample a bit so it fits in a terminal, just skip every other row/col
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

    // sand block
    for (int y = 5; y < 20; y++)
        for (int x = 20; x < 50; x++)
            w.cur[y * WIDTH + x].type = TYPE_SAND;

    // water pool near the bottom
    for (int y = HEIGHT - 15; y < HEIGHT - 2; y++)
        for (int x = 10; x < 100; x++)
            w.cur[y * WIDTH + x].type = TYPE_WATER;

    // small fire source
    for (int y = 40; y < 43; y++)
        for (int x = 60; x < 63; x++) {
            w.cur[y * WIDTH + x].type = TYPE_FIRE;
            w.cur[y * WIDTH + x].life = 50;
        }

    int totalTicks = 200;
    for (int t = 0; t < totalTicks; t++) {
        simTick(w);
        if (t % 20 == 0) {
            std::cout << "tick " << t << "\n";
            printWorld(w);
        }
    }

    return 0;
}
