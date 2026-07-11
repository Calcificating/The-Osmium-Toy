#pragma once
#include "world.h"

void setupDemoScene(World& w);

// separate from setupDemoScene on purpose - didnt want to disturb the
// scene everything else has been comparing hashes against, this one's
// just for exercising acid specifically
void setupAcidTestScene(World& w);

void setupPortalTestScene(World& w);

// SAND/WATR/FIRE/ACID/PLSM/WALL/PRTI+PRTO all together, this is the
// "representative subset" scene for the final validation pass
void setupCapstoneScene(World& w);