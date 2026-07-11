#pragma once
#include <cstdint>

// used to be const int, world size is dynamic now (see setWorldSize in
// world.h). storage lives in world.cpp. call setWorldSize() BEFORE
// constructing any World - we dont support resizing a world thats already
// in use, nothing enforces that at runtime, its just a rule
extern int WIDTH;
extern int HEIGHT;

enum ElemType {
    TYPE_EMPTY = 0,
    TYPE_SAND,
    TYPE_WATER,
    TYPE_STEAM,
    TYPE_FIRE,
    TYPE_WALL,
    TYPE_ACID,
    TYPE_PLSM,
    TYPE_PRTI, // portal in - stationary, absorbs a neighbor and routes it to the matching PRTO
    TYPE_PRTO, // portal out - stationary, just a landing marker, doesnt do anything itself
    NUM_TYPES
};

struct Particle {
    int type = TYPE_EMPTY;
    float temp = 22.0f;
    int life = 0;
    // free scratch field. portals use this for channel id (PRTI looks
    // for a PRTO with the same tmp value). fixed an old comment here that
    // said acid used this - it doesnt, acid's potency is in life, tmp was
    // actually unused until portals needed it
    int tmp = 0;
};