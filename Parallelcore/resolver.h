#pragma once
#include "world.h"
#include "command.h"
#include "stats.h"
#include <vector>

void resolveCommands(World& w, std::vector<Cmd>& cmds, SimStats* stats = nullptr);