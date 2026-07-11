#include "world.h"
#include "sim.h"
#include "scene.h"
#include "stats.h"
#include <iostream>
#include <iomanip>

int main() {
    World w;
    setupDemoScene(w);

    uint32_t seed = 1337;
    int totalTicks = 200;

    for (int t = 0; t < totalTicks; t++) {
        SimStats stats;
        simTick(w, seed, t, 4, &stats);

        if (t % 20 != 0) continue;

        std::cout << "tick " << t << "\n";
        std::cout << "Commands generated:\n";
        std::cout << "  Move:   " << stats.cmdMove << "\n";
        std::cout << "  Create: " << stats.cmdCreate << "\n";
        std::cout << "  Delete: " << stats.cmdDelete << "\n";
        std::cout << "  Swap:   " << stats.cmdSwap << "\n";
        std::cout << "  Heat:   " << stats.cmdHeat << "\n";
        std::cout << "  Age:    " << stats.cmdAge << "\n";
        std::cout << "Conflicts:\n";
        std::cout << "  Cells contested: " << stats.conflictedCells << "\n";
        std::cout << "  Commands rejected: " << stats.commandsRejected << "\n";
        if (stats.invalidCommandsDropped > 0) {
            // shouldnt normally see this from the real elements, this is
            // more of a canary in case something starts emitting garbage
            std::cout << "  Invalid commands dropped: " << stats.invalidCommandsDropped << "\n";
        }
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Time:\n";
        std::cout << "  Compute: " << stats.computeMs << " ms\n";
        std::cout << "  Resolve: " << stats.resolveMs << " ms\n";
        std::cout << "  Apply:   " << stats.applyMs << " ms\n";
        std::cout << "----\n";
    }

    return 0;
}