#pragma once
#include <cstdint>

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

struct Particle {
    int type = TYPE_EMPTY;
    float temp = 22.0f;
    int life = 0;
    int tmp = 0;
};
