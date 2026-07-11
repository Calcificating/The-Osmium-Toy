#include "scene.h"

void setupDemoScene(World& w) {
    for (int y = 5; y < 20; y++)
        for (int x = 20; x < 50; x++)
            w.cur[y * WIDTH + x].type = TYPE_SAND;

    for (int y = HEIGHT - 15; y < HEIGHT - 2; y++)
        for (int x = 10; x < 100; x++)
            w.cur[y * WIDTH + x].type = TYPE_WATER;

    for (int y = 40; y < 43; y++)
        for (int x = 60; x < 63; x++) {
            w.cur[y * WIDTH + x].type = TYPE_FIRE;
            w.cur[y * WIDTH + x].life = 50;
        }
}
