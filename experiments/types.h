#pragma once
#include <cstdint>

// just hardcoding these for now, should probably read from a cfg file
// or argv at some point but whatever, its a spike
const int WIDTH = 120;
const int HEIGHT = 90;

enum ElemType {
    TYPE_EMPTY = 0,
    TYPE_SAND,
    TYPE_WATER,
    TYPE_STEAM,
    TYPE_FIRE,
    TYPE_WALL,
    NUM_TYPES
};

// loosely mirrors the real Particle struct fields (type/life/temp/tmp)
// but flattened down since we dont need the full float x,y stuff for
// a grid based toy sim
struct Particle {
    int type = TYPE_EMPTY;
    float temp = 22.0f;
    int life = 0;
    int tmp = 0; // scratch field, reused for different stuff per element, same hack as upstream tbh
};
