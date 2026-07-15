#include "Serialization/DistributionExport.h"

#include "Serialization/HansoAudioChunkCodec.h"
#include "Serialization/HansoSerializer.h"

namespace hanso
{
namespace
{
// Mirrors hanso-dsp stripAnalysisChunks(): roles the runtime never plays.
bool isAnalysisRole(const juce::String& role)
{
    return role == "dryReference" || role == "drySample" || role == "alignedCaptured"
        || role == "captured" || role == "probeValidationDry"
        || role == "probeValidationReal" || role == "ampProcessedSample";
}

// Cabinet IRs stay in the distribution build but move to the playback
// encoding: float32/pcm16 master audio -> pcm24, with the id suffix renamed to
// match (consumers resolve legacy suffixes via findAudioChunk fallback).
void reencodeIrChunkToPcm24(HansoBinaryChunk& chunk)
{
    juce::AudioBuffer<float> audio;
    double sampleRate = 0.0;
    juce::String error;
    if (! HansoAudioChunkCodec::decodeAudio(chunk.mediaType, chunk.data, audio, sampleRate, error))
        return; // leave the chunk untouched rather than ship a broken one

    chunk.data = HansoAudioChunkCodec::encodePcm24Audio(audio, sampleRate);
    chunk.mediaType = mediaType::pcm24Audio;
    const auto lastDot = chunk.id.lastIndexOfChar('.');
    if (lastDot > 0)
        chunk.id = chunk.id.substring(0, lastDot) + ".pcm24";
}
} // namespace

juce::File DistributionExport::masterArchiveDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("HANSO Lab")
        .getChildFile("Masters");
}

juce::File DistributionExport::masterFileForCaptureId(const juce::String& captureId)
{
    return masterArchiveDirectory().getChildFile(captureId + ".hansocap");
}

void DistributionExport::makeDistribution(HansoPackage& package)
{
    for (int i = package.chunks.size(); --i >= 0;)
    {
        auto* chunk = package.chunks.getUnchecked(i);
        if (isAnalysisRole(chunk->role))
            package.chunks.remove(i);
        else if (chunk->role == "cabinet-ir")
            reencodeIrChunkToPcm24(*chunk);
    }
    package.assetTier = "distribution";
}

bool DistributionExport::exportDistribution(HansoPackage& sessionPackage,
                                            const juce::File& destination,
                                            juce::String& error)
{
    if (sessionPackage.captureId.isEmpty())
        sessionPackage.captureId = juce::Uuid().toString();

    // A session opened from a distribution .hanso carries no master audio —
    // archiving it would overwrite the real master. Only master-tier sessions
    // (fresh captures, legacy full files) refresh the archive.
    const bool sessionIsMaster = sessionPackage.assetTier != "distribution";

    juce::File sourceFile;
    juce::File temporaryFile;
    if (sessionIsMaster)
    {
        // 1) Silent master archive — app-managed, never shown to the user.
        const auto masterFile = masterFileForCaptureId(sessionPackage.captureId);
        if (! masterFile.getParentDirectory().createDirectory())
        {
            error = "Failed to create master archive directory: "
                  + masterFile.getParentDirectory().getFullPathName();
            return false;
        }
        sessionPackage.assetTier = "master";
        if (! HansoSerializer::writeToFile(sessionPackage, masterFile, error))
            return false;
        sourceFile = masterFile;
    }
    else
    {
        // Distribution session: leave the archive untouched; clone via a
        // temporary file instead (HansoPackage is not copyable).
        temporaryFile = destination.getSiblingFile("." + destination.getFileNameWithoutExtension()
                                                   + ".export-tmp.hanso");
        if (! HansoSerializer::writeToFile(sessionPackage, temporaryFile, error))
            return false;
        sourceFile = temporaryFile;
    }

    // 2) Distribution copy — reload, strip + re-encode, hand the user their .hanso.
    HansoPackage distribution;
    const auto readOk = HansoSerializer::readFromFile(sourceFile, distribution, error);
    if (temporaryFile != juce::File())
        temporaryFile.deleteFile();
    if (! readOk)
        return false;

    makeDistribution(distribution);
    return HansoSerializer::writeToFile(distribution, destination, error);
}
}
