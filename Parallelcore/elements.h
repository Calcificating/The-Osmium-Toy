#pragma once
#include "world.h"
#include "command.h"
#include "rng.h"
#include "portal_index.h"
#include <vector>

// stateless on purpose. each of these takes a READ ONLY world (compiler
// enforced, see world.h), the cell its looking at, and an rng it does not
// own. returns whatever it wants to happen, doesnt touch anything itself.
// no element here knows threads or chunks exist

namespace Sand  { std::vector<Cmd> ComputeIntent(const World& w, int x, int y, Rng& rng); }
namespace Water { std::vector<Cmd> ComputeIntent(const World& w, int x, int y, Rng& rng); }
namespace Steam { std::vector<Cmd> ComputeIntent(const World& w, int x, int y, Rng& rng); }
namespace Fire  { std::vector<Cmd> ComputeIntent(const World& w, int x, int y, Rng& rng); }
namespace Acid  { std::vector<Cmd> ComputeIntent(const World& w, int x, int y, Rng& rng); }
namespace Plsm  { std::vector<Cmd> ComputeIntent(const World& w, int x, int y, Rng& rng); }

// portal is the only one that needs the extra read-only lookup table.
// didnt want to thread a PortalIndex through every other elements
// signature just for this one, so ComputeIntentForCell takes it and only
// passes it down to Portal
namespace Portal { std::vector<Cmd> ComputeIntent(const World& w, int x, int y, Rng& rng, const PortalIndex& portals); }

std::vector<Cmd> ComputeIntentForCell(const World& w, int x, int y, Rng& rng, const PortalIndex& portals);