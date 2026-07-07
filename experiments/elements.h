#pragma once
#include "world.h"
#include "command.h"

// phase 1 stuff - particle looks at cur (read only) and pushes commands
// it does NOT touch the world directly, thats the whole point of this experiment
void decideSand(World& w, int x, int y, CmdQueue& q);
void decideWater(World& w, int x, int y, CmdQueue& q);
void decideSteam(World& w, int x, int y, CmdQueue& q);
void decideFire(World& w, int x, int y, CmdQueue& q);

void decideCell(World& w, int x, int y, CmdQueue& q);
