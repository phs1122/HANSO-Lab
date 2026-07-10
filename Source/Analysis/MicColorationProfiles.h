#pragma once

#include <array>

#include "Model/CabinetToneProfile.h"
#include "Model/MicColoration.h"

namespace hanso
{
using CabinetToneBandArray = std::array<float, static_cast<size_t>(cabinetToneBandCount)>;

// Canonical mic-class and mic-position coloration curves, expressed as average
// magnitude (dB) over the shared cabinet tone bands.
//
// The parametric EQ stage definitions mirror the HANSO TONE cabinet stage
// (Modules/hst_fx/include/hst_fx/CabinetProfiles.h — kMicProfiles and
// kMicPositionPresets). HANSO Lab must not link against the FX plugin, so the
// values are duplicated here with this provenance note; the planned
// integration phase moves both consumers onto one shared module. If either
// side changes its curves, update the other in the same change.
class MicColorationProfiles
{
public:
    // Band-averaged response of the mic-class EQ chain (4 peaking stages).
    // Unknown is treated as Dynamic, the standard default cab mic.
    static CabinetToneBandArray micColorationBandsDb(CabinetMicClass micClass);

    // Band-averaged response of a mic-position slot's EQ chain
    // (low shelf, mid peak, high shelf, high cut). The index follows the
    // kCabinetMicPositions order: 0 Center, 1 Edge, 2 Cone, 3 Off-Axis.
    static CabinetToneBandArray positionColorationBandsDb(int positionIndex);

    // Broadband gain trim applied by the position preset (dB).
    static float positionGainTrimDb(int positionIndex) noexcept;

    static int positionCount() noexcept;
};
}
