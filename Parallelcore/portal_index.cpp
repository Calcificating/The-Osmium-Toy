#include "portal_index.h"

PortalIndex buildPortalIndex(const World& w) {
    PortalIndex idx;
    for (int c = 0; c < MAX_PORTAL_CHANNELS; c++) {
        idx.exitX[c] = -1;
        idx.exitY[c] = -1;
    }

    // if two PRTOs somehow share a channel, last one found in scan order
    // wins, didnt think this was worth a warning system for a prototype
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            const Particle& p = w.at(x, y);
            if (p.type == TYPE_PRTO && p.tmp >= 0 && p.tmp < MAX_PORTAL_CHANNELS) {
                idx.exitX[p.tmp] = x;
                idx.exitY[p.tmp] = y;
            }
        }
    }
    return idx;
}