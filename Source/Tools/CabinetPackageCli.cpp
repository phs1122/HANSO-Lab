// Headless authoring tool: builds a Cabinet .hanso package from external IR
// wav files, mirroring the app's import path (CabinetIrImporter -> slot ->
// tone profile -> estimation -> cabProfile/micMatrix -> serializer). Useful
// for pipeline validation and batch authoring without the GUI.
//
// Usage:
//   HANSO_Lab_CabCli --out <file.hanso> --name "Cab name"
//                    --slot <cab-center|cab-edge|cab-cone|cab-off-axis> <ir.wav> <dynamic|ribbon|condenser|unknown> [mic model name]
//                    [--speaker "..."] [--notes "..."]
//   (--slot may be repeated; at least one is required)

#include <JuceHeader.h>

#include "Analysis/CabinetToneProfiler.h"
#include "Capture/CabinetIrImporter.h"
#include "Capture/CabinetMicPositions.h"
#include "Capture/CaptureWizardState.h"
#include "Model/HansoPackage.h"
#include "Serialization/HansoAudioChunkCodec.h"
#include "Serialization/HansoSerializer.h"

namespace
{
struct SlotRequest
{
    juce::String slotId;
    juce::File irFile;
    hanso::CabinetMicClass micClass { hanso::CabinetMicClass::Unknown };
    juce::String micModelName;
};

int fail(const juce::String& message)
{
    std::cerr << "ERROR: " << message << std::endl;
    return 1;
}
}

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::File outFile;
    juce::String name = "Imported Cabinet";
    juce::String speaker;
    juce::String notes;
    std::vector<SlotRequest> requests;

    for (int i = 1; i < argc; ++i)
    {
        const auto arg = juce::String::fromUTF8(argv[i]);
        if (arg == "--out" && i + 1 < argc)
        {
            outFile = juce::File(juce::String::fromUTF8(argv[++i]));
        }
        else if (arg == "--name" && i + 1 < argc)
        {
            name = juce::String::fromUTF8(argv[++i]);
        }
        else if (arg == "--speaker" && i + 1 < argc)
        {
            speaker = juce::String::fromUTF8(argv[++i]);
        }
        else if (arg == "--notes" && i + 1 < argc)
        {
            notes = juce::String::fromUTF8(argv[++i]);
        }
        else if (arg == "--slot" && i + 3 < argc)
        {
            SlotRequest request;
            request.slotId = juce::String::fromUTF8(argv[++i]);
            request.irFile = juce::File(juce::String::fromUTF8(argv[++i]));
            request.micClass = hanso::cabinetMicClassFromString(juce::String::fromUTF8(argv[++i]).toLowerCase());
            if (i + 1 < argc && juce::String::fromUTF8(argv[i + 1]).startsWith("--") == false)
                request.micModelName = juce::String::fromUTF8(argv[++i]);
            if (request.micClass == hanso::CabinetMicClass::Unknown && request.micModelName.isNotEmpty())
                request.micClass = hanso::suggestMicClassForModelName(request.micModelName);
            requests.push_back(std::move(request));
        }
        else
        {
            return fail("Unknown or incomplete argument: " + arg);
        }
    }

    if (outFile == juce::File())
        return fail("--out <file.hanso> is required.");
    if (requests.empty())
        return fail("At least one --slot <id> <ir.wav> <mic class> is required.");

    hanso::CaptureWizardState wizard(hanso::CaptureType::Cabinet);
    hanso::HansoPackage package;
    package.formatVersion = 2;

    hanso::CabinetIrImporter importer;
    auto packageSampleRate = 0.0;

    for (const auto& request : requests)
    {
        if (wizard.findCabinetSlot(request.slotId) == nullptr)
            return fail("Unknown slot id: " + request.slotId
                        + " (expected cab-center / cab-edge / cab-cone / cab-off-axis)");

        const auto result = importer.importFile(request.irFile);
        if (! result.success)
            return fail("IR import failed for " + request.irFile.getFullPathName() + ": " + result.error);

        const auto chunkId = hanso::impulseResponseChunkIdForCabinetPosition(request.slotId);
        package.setChunk(chunkId,
                         "cabinet-ir",
                         hanso::mediaType::float32Audio,
                         hanso::HansoAudioChunkCodec::encodeFloat32Audio(result.impulseResponse, result.sampleRate),
                         result.sampleRate,
                         result.impulseResponse.getNumChannels(),
                         result.impulseResponse.getNumSamples());
        packageSampleRate = result.sampleRate;

        wizard.markCabinetSlotImported(request.slotId,
                                       chunkId,
                                       request.irFile.getFileName(),
                                       request.micClass,
                                       request.micModelName);
        if (auto* slot = wizard.findCabinetSlot(request.slotId))
            slot->toneProfile = hanso::CabinetToneProfiler::fromImpulseResponse(result.impulseResponse,
                                                                                result.sampleRate);

        wizard.setStepStatus(request.slotId, hanso::CaptureStepStatus::Passed);
        std::cout << "Imported " << request.slotId << " <- " << request.irFile.getFileName()
                  << " (" << hanso::toString(request.micClass) << ", "
                  << result.impulseResponse.getNumSamples() << " samples @ "
                  << result.sampleRate << " Hz)" << std::endl;
    }

    wizard.estimateEmptyCabinetSlots();
    wizard.setStepStatus("final-validation", hanso::CaptureStepStatus::Passed);

    package.metadata.name = name;
    package.metadata.category = hanso::HansoCategory::Cabinet;
    package.metadata.deviceType = "Cabinet";
    package.metadata.model = name;
    package.metadata.notes = notes;
    package.metadata.sampleRate = packageSampleRate;

    const auto micSummary = requests.front().micModelName.isNotEmpty()
                          ? requests.front().micModelName
                          : hanso::cabinetMicClassLabel(requests.front().micClass);
    package.metadata.sourceDevice = micSummary;
    package.cabinetProfile = wizard.toCabinetProfileVar(name, micSummary, speaker, notes);
    package.captureWorkflow = wizard.toMetadataVar();

    juce::String error;
    if (! hanso::HansoSerializer::writeToFile(package, outFile, error))
        return fail("Serializer failed: " + error);

    auto realCount = 0;
    auto estimatedCount = 0;
    for (const auto& slot : wizard.cabinetSlots)
    {
        if (slot.hasRealSource())
            ++realCount;
        else if (slot.source == hanso::CabinetSlotSource::EstimatedCompactCab)
            ++estimatedCount;
    }

    const auto matrixVar = wizard.toCabinetMicMatrixVar();
    const auto* matrixObject = matrixVar.getDynamicObject();
    const auto matrixEntries = matrixObject != nullptr && matrixObject->getProperty("entries").getArray() != nullptr
                             ? matrixObject->getProperty("entries").getArray()->size()
                             : 0;

    std::cout << "Wrote " << outFile.getFullPathName() << std::endl
              << "  positions: " << realCount << " real / " << estimatedCount << " estimated" << std::endl
              << "  micMatrix entries: " << matrixEntries << std::endl
              << "  size: " << outFile.getSize() << " bytes" << std::endl;
    return 0;
}
