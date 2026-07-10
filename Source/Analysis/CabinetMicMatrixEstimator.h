#pragma once

#include <JuceHeader.h>

#include <vector>

#include "Capture/CabinetCaptureState.h"

namespace hanso
{
// One (mic class, mic position) cell of the estimated cabinet matrix.
struct CabinetMicMatrixEntry
{
    CabinetMicClass micClass { CabinetMicClass::Dynamic };
    juce::String positionId;
    bool measured { false };
    juce::String estimatedFrom;
    CabinetToneProfile toneProfile;
};

struct CabinetMicMatrix
{
    bool valid { false };
    std::vector<CabinetMicMatrixEntry> entries;

    juce::var toVar() const;
};

// Estimates tone profiles for every (mic class x mic position) combination
// from the real-source slots of a cabinet capture session.
//
// Each real slot is treated as: neutral cabinet response + mic coloration +
// position coloration (additive in the log-magnitude band domain). The known
// colorations are subtracted to recover the neutral cabinet estimate, which is
// then re-embedded with every other combination's colorations. Slots with an
// unknown mic class are assumed to be dynamic, the standard cab mic.
class CabinetMicMatrixEstimator
{
public:
    static CabinetMicMatrix estimate(const std::vector<CabinetMicPositionSlot>& slots);
};
}
