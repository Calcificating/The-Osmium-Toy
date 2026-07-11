#pragma once
#include "world.h"
#include "command.h"
#include "rng.h"
#include <vector>

// stateless on purpose. each of these takes a READ ONLY world (compiler
// enforced, see world.h), the cell its looking at, and an rng it does not
// own. returns whatever it wants to happen, doesnt touch anything itself.
// no element here knows threads or chunks exist

namespace Sand  { std::vector<Cmd> ComputeIntent(const World& w, int x, int y, Rng& rng); }
namespace Water { std::vector<Cmd> ComputeIntent(const World& w, int x, int y, Rng& rng); }
namespace Steam { std::vector<Cmd> ComputeIntent(const World& w, int x, int y, Rng& rng); }
namespace Fire  { std::vector<Cmd> ComputeIntent(const World& w, int x, int y, Rng& rng); }

std::vector<Cmd> ComputeIntentForCell(const World& w, int x, int y, Rng& rng);
