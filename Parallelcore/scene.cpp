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

void setupAcidTestScene(World& w) {
    // solid sand block for the acid to eat through
    for (int y = 30; y < 60; y++)
        for (int x = 40; x < 80; x++)
            w.cur[y * WIDTH + x].type = TYPE_SAND;

    // acid patch dropped on top of it
    for (int y = 10; y < 15; y++)
        for (int x = 50; x < 70; x++) {
            w.cur[y * WIDTH + x].type = TYPE_ACID;
            w.cur[y * WIDTH + x].life = 40;
        }

    // floor so nothing that falls just vanishes off the bottom, easier
    // to eyeball whats actually left over
    for (int x = 0; x < WIDTH; x++)
        w.cur[(HEIGHT - 1) * WIDTH + x].type = TYPE_WALL;
}

void setupPortalTestScene(World& w) {
    // sand column falling down a narrow walled chute into a PRTI at the
    // bottom, teleporting across the map to a PRTO sitting over an open
    // landing area on the other side
    for (int y = 5; y < 40; y++) {
        w.cur[y * WIDTH + 19].type = TYPE_WALL;
        w.cur[y * WIDTH + 22].type = TYPE_WALL;
    }
    for (int y = 5; y < 15; y++)
        for (int x = 20; x < 22; x++)
            w.cur[y * WIDTH + x].type = TYPE_SAND;

    w.cur[39 * WIDTH + 20].type = TYPE_PRTI;
    w.cur[39 * WIDTH + 20].tmp = 0; // channel 0

    w.cur[10 * WIDTH + 90].type = TYPE_PRTO;
    w.cur[10 * WIDTH + 90].tmp = 0; // same channel

    // floor so anything that lands near the exit and keeps falling
    // doesnt just vanish off the bottom
    for (int x = 0; x < WIDTH; x++)
        w.cur[(HEIGHT - 1) * WIDTH + x].type = TYPE_WALL;
}

void setupCapstoneScene(World& w) {
    // representative-subset scene: sand, water, fire, acid, plasma, wall,
    // and a portal pair, all interacting in one layout. not trying to be
    // a real TPT save, just a scene that gives every ported element
    // something to actually do

    // outer floor + side walls so nothing just falls off the map
    for (int x = 0; x < WIDTH; x++) {
        w.cur[(HEIGHT - 1) * WIDTH + x].type = TYPE_WALL;
    }
    for (int y = 0; y < HEIGHT; y++) {
        w.cur[y * WIDTH + 0].type = TYPE_WALL;
        w.cur[y * WIDTH + (WIDTH - 1)].type = TYPE_WALL;
    }

    // left chamber: sand falling into a water pool, fire nearby to make
    // some steam once they meet
    for (int y = 5; y < 20; y++)
        for (int x = 5; x < 25; x++)
            w.cur[y * WIDTH + x].type = TYPE_SAND;

    for (int y = HEIGHT - 20; y < HEIGHT - 2; y++)
        for (int x = 5; x < 45; x++)
            w.cur[y * WIDTH + x].type = TYPE_WATER;

    for (int y = 30; y < 33; y++)
        for (int x = 30; x < 33; x++) {
            w.cur[y * WIDTH + x].type = TYPE_FIRE;
            w.cur[y * WIDTH + x].life = 50;
        }

    // middle chamber: acid eating a separate sand block
    for (int y = 30; y < 55; y++)
        for (int x = 55; x < 80; x++)
            w.cur[y * WIDTH + x].type = TYPE_SAND;
    for (int y = 10; y < 13; y++)
        for (int x = 60; x < 75; x++) {
            w.cur[y * WIDTH + x].type = TYPE_ACID;
            w.cur[y * WIDTH + x].life = 40;
        }

    // a few plasma particles drifting loose in the middle chamber
    for (int x = 62; x < 68; x++) {
        w.cur[20 * WIDTH + x].type = TYPE_PLSM;
        w.cur[20 * WIDTH + x].life = 60;
    }

    // right chamber: portal pair. sand column feeds a PRTI, matching
    // PRTO drops the sand into an otherwise empty holding chamber so we
    // can see it actually made the trip
    for (int y = 5; y < 25; y++)
        for (int x = 90; x < 92; x++)
            w.cur[y * WIDTH + x].type = TYPE_SAND;

    w.cur[26 * WIDTH + 91].type = TYPE_PRTI;
    w.cur[26 * WIDTH + 91].tmp = 0; // channel 0

    w.cur[60 * WIDTH + 105].type = TYPE_PRTO;
    w.cur[60 * WIDTH + 105].tmp = 0; // same channel, this is the exit
}