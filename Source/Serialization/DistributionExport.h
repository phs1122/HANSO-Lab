#pragma once

#include <JuceHeader.h>

#include "Model/HansoPackage.h"

namespace hanso
{
// Master/distribution export pipeline (container doc §6.2, factory-library
// README). UX rule: users never see .hansocap — the master is archived
// silently into an app-managed folder, and the only file handed to the user
// is the stripped distribution .hanso, which must be fully usable on its own.
class DistributionExport final
{
public:
    // App-managed, out-of-sight master archive:
    //   ~/Library/Application Support/HANSO Lab/Masters (macOS)
    static juce::File masterArchiveDirectory();

    // Where the session's master lives (Masters/<captureId>.hansocap).
    static juce::File masterFileForCaptureId(const juce::String& captureId);

    // In place: drop analysis-role chunks the runtime never plays, re-encode
    // cabinet IRs to pcm24 (playback tier), and stamp tier = "distribution".
    static void makeDistribution(HansoPackage& package);

    // The full export: ensure captureId on the session package, silently
    // archive the master (.hansocap, tier = "master"), then write the stripped
    // distribution .hanso to `destination`. The session package keeps its
    // audio; only `captureId` may be assigned on it.
    static bool exportDistribution(HansoPackage& sessionPackage,
                                   const juce::File& destination,
                                   juce::String& error);
};
}
