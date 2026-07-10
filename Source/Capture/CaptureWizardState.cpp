#include "Capture/CaptureWizardState.h"

#include "Analysis/CabinetMicMatrixEstimator.h"
#include "Capture/CabinetMessages.h"
#include "Capture/CabinetMicPositions.h"
#include "Capture/CaptureStepUtils.h"

namespace hanso
{
namespace
{
juce::var qualityToVar(const CaptureQualityReport& report)
{
    auto object = new juce::DynamicObject();
    object->setProperty("overallSeverity", toString(report.overallSeverity));
    object->setProperty("peakDbfs", report.peakDbfs);
    object->setProperty("rmsDbfs", report.rmsDbfs);
    object->setProperty("clipSampleCount", report.clipSampleCount);
    object->setProperty("noiseFloorDbfs", report.noiseFloorDbfs);
    object->setProperty("signalToNoiseDb", report.signalToNoiseDb);
    object->setProperty("latencySamples", report.latencySamples);
    object->setProperty("latencyMs", report.latencyMs);
    object->setProperty("alignmentConfidence", report.alignmentConfidence);

    juce::Array<juce::var> issues;
    for (const auto& issue : report.issues)
    {
        auto issueObject = new juce::DynamicObject();
        issueObject->setProperty("severity", toString(issue.severity));
        issueObject->setProperty("code", issue.code);
        issueObject->setProperty("messageEnglish", issue.messageEnglish);
        issueObject->setProperty("messageKorean", issue.messageKorean);
        issueObject->setProperty("suggestedActionEnglish", issue.suggestedActionEnglish);
        issueObject->setProperty("suggestedActionKorean", issue.suggestedActionKorean);
        issues.add(issueObject);
    }
    object->setProperty("issues", issues);
    return object;
}
}

CaptureWizardState::CaptureWizardState()
    : CaptureWizardState(CaptureType::FullRig)   // Full Rig is the most common amp form
{
}

CaptureWizardState::CaptureWizardState(CaptureType type)
{
    setCaptureType(type);
}

void CaptureWizardState::setCaptureType(CaptureType type)
{
    captureType = type;
    recipe = captureType == CaptureType::Cabinet
           ? CaptureRecipe::createCabinetMicPositions()
           : CaptureRecipe::createBasicAmpLiquidGain();

    results.clear();
    cabinetSlots = captureType == CaptureType::Cabinet ? createDefaultCabinetSlots()
                                                       : std::vector<CabinetMicPositionSlot>();
    currentStepId = recipe.steps.empty() ? juce::String() : recipe.steps.front().stepId;
    calibrationPassed = false;
    warningExportAccepted = false;
    cabinetCaptureMicClass = CabinetMicClass::Unknown;
    cabinetCaptureMicModelName.clear();
}

CaptureStep* CaptureWizardState::findStep(const juce::String& stepId) noexcept
{
    for (auto& step : recipe.steps)
        if (step.stepId == stepId)
            return &step;

    return nullptr;
}

const CaptureStep* CaptureWizardState::findStep(const juce::String& stepId) const noexcept
{
    for (const auto& step : recipe.steps)
        if (step.stepId == stepId)
            return &step;

    return nullptr;
}

CaptureStep* CaptureWizardState::firstIncompleteRequiredStep() noexcept
{
    for (auto& step : recipe.steps)
        if (step.required && step.status != CaptureStepStatus::Passed && step.status != CaptureStepStatus::Warning)
            return &step;

    return nullptr;
}

const CaptureStep* CaptureWizardState::firstIncompleteRequiredStep() const noexcept
{
    for (const auto& step : recipe.steps)
        if (step.required && step.status != CaptureStepStatus::Passed && step.status != CaptureStepStatus::Warning)
            return &step;

    return nullptr;
}

void CaptureWizardState::setStepStatus(const juce::String& stepId, CaptureStepStatus status)
{
    if (auto* step = findStep(stepId))
        step->status = status;
}

void CaptureWizardState::storeResult(const CaptureStepResult& result)
{
    setStepStatus(result.stepId, result.status);
    for (auto& existing : results)
    {
        if (existing.stepId == result.stepId)
        {
            existing = result;
            return;
        }
    }

    results.push_back(result);
}

void CaptureWizardState::removeResult(const juce::String& stepId)
{
    results.erase(std::remove_if(results.begin(),
                                 results.end(),
                                 [&stepId](const CaptureStepResult& result)
                                 {
                                     return result.stepId == stepId;
                                 }),
                  results.end());
}

const CaptureStepResult* CaptureWizardState::findResult(const juce::String& stepId) const noexcept
{
    for (const auto& result : results)
        if (result.stepId == stepId)
            return &result;

    return nullptr;
}

bool CaptureWizardState::hasWarnings() const noexcept
{
    for (const auto& result : results)
        if (result.status == CaptureStepStatus::Warning)
            return true;

    return false;
}

bool CaptureWizardState::isExportReady() const noexcept
{
    if (captureType == CaptureType::Cabinet && ! hasCabinetRealSource())
        return false;

    return firstIncompleteRequiredStep() == nullptr;
}

juce::String CaptureWizardState::exportDisabledReason() const
{
    if (const auto* step = firstIncompleteRequiredStep())
    {
        if (captureType == CaptureType::Cabinet
            && step->stepId == "final-validation"
            && ! hasCabinetRealSource())
        {
            return "Export disabled: " + cabinetExportRequiresRealSourceMessage();
        }

        return "Export disabled: " + step->title + " is not completed.";
    }

    if (captureType == CaptureType::Cabinet && ! hasCabinetRealSource())
        return "Export disabled: " + cabinetExportRequiresRealSourceMessage();

    return {};
}

juce::var CaptureWizardState::toMetadataVar() const
{
    auto object = new juce::DynamicObject();
    object->setProperty("captureType", toString(captureType));
    object->setProperty("captureMode", toString(mode));

    auto routing = new juce::DynamicObject();
    routing->setProperty("mode", mode == CaptureMode::Easy ? "MonoLeftOnly" : "Default");
    routing->setProperty("leftChannel", "TestSignal");
    routing->setProperty("rightChannel", mode == CaptureMode::Easy ? "Silence" : "Default");
    object->setProperty("outputRouting", routing);

    auto cable = new juce::DynamicObject();
    cable->setProperty("selected", toString(cableGuide));
    cable->setProperty("alternateSupported", "TrsToDualTsYCable");
    object->setProperty("cableGuide", cable);

    auto recipeObject = new juce::DynamicObject();
    recipeObject->setProperty("recipeId", recipe.recipeId);
    recipeObject->setProperty("displayName", recipe.displayName);
    recipeObject->setProperty("category", toString(recipe.category));

    juce::Array<juce::var> fixedControls;
    for (const auto& control : recipe.fixedControls)
    {
        auto controlObject = new juce::DynamicObject();
        controlObject->setProperty("controlName", control.controlName);
        controlObject->setProperty("position", control.positionLabel);
        controlObject->setProperty("normalizedValue", control.normalizedValue);
        fixedControls.add(controlObject);
    }
    recipeObject->setProperty("fixedControls", fixedControls);

    juce::Array<juce::var> anchors;
    for (const auto& step : recipe.steps)
    {
        if (! step.isAnchorCapture())
            continue;

        auto anchor = new juce::DynamicObject();
        anchor->setProperty("parameterKey", step.anchor.parameterKey);
        anchor->setProperty("normalizedValue", step.anchor.normalizedValue);
        anchor->setProperty("displayLabel", step.anchor.displayLabel);
        anchor->setProperty("status", toString(step.status));

        if (const auto* result = findResult(step.stepId))
        {
            anchor->setProperty("dryChunkId", result->dryChunkId);
            anchor->setProperty("capturedChunkId", result->capturedChunkId);
            anchor->setProperty("alignedChunkId", result->alignedChunkId);
            anchor->setProperty("quality", qualityToVar(result->quality));

            auto levelMetadata = new juce::DynamicObject();
            levelMetadata->setProperty("dryPeakDbfs", result->dryPeakDbfs);
            levelMetadata->setProperty("dryRmsDbfs", result->dryRmsDbfs);
            levelMetadata->setProperty("captureOutputDbfs", result->captureOutputDbfs);
            levelMetadata->setProperty("capturedPeakDbfs", result->quality.peakDbfs);
            levelMetadata->setProperty("capturedRmsDbfs", result->quality.rmsDbfs);
            levelMetadata->setProperty("normalization", "per-anchor dry reference");
            anchor->setProperty("levelMetadata", levelMetadata);
        }
        else
        {
            anchor->setProperty("dryChunkId", dryChunkIdForStep(step));
            anchor->setProperty("capturedChunkId", juce::var());
            anchor->setProperty("alignedChunkId", alignedChunkIdForStep(step));
        }

        anchors.add(anchor);
    }
    recipeObject->setProperty("anchors", anchors);

    if (captureType == CaptureType::Cabinet)
        recipeObject->setProperty("positions", cabinetSlotsToVar(cabinetSlots, true));

    auto interpolation = new juce::DynamicObject();
    interpolation->setProperty("parameterKey", captureType == CaptureType::Cabinet ? "micPosition" : "gain");
    interpolation->setProperty("method", captureType == CaptureType::Cabinet ? "tone-profile-interpolation" : "measured-transfer-fit");
    interpolation->setProperty("status", cabinetInterpolationComputed ? "computed" : "notComputed");
    recipeObject->setProperty("interpolationPlan", interpolation);

    if (const auto* calibration = findResult("calibration"))
    {
        auto calibrationObject = new juce::DynamicObject();
        calibrationObject->setProperty("status", toString(calibration->status));
        calibrationObject->setProperty("quality", qualityToVar(calibration->quality));
        calibrationObject->setProperty("noiseFloorDbfs", calibration->quality.noiseFloorDbfs);
        calibrationObject->setProperty("signalToNoiseDb", calibration->quality.signalToNoiseDb);
        calibrationObject->setProperty("validator", "noise-floor-plus-goertzel-multisine");
        object->setProperty("calibration", calibrationObject);
    }

    object->setProperty("captureRecipe", recipeObject);
    return object;
}

CabinetMicPositionSlot* CaptureWizardState::findCabinetSlot(const juce::String& stepId) noexcept
{
    for (auto& slot : cabinetSlots)
        if (slot.id == stepId)
            return &slot;

    return nullptr;
}

const CabinetMicPositionSlot* CaptureWizardState::findCabinetSlot(const juce::String& stepId) const noexcept
{
    for (const auto& slot : cabinetSlots)
        if (slot.id == stepId)
            return &slot;

    return nullptr;
}

void CaptureWizardState::markCabinetSlotCaptured(const juce::String& stepId, const juce::String& chunkId)
{
    if (auto* slot = findCabinetSlot(stepId))
    {
        slot->source = CabinetSlotSource::CapturedIr;
        slot->impulseResponseChunkId = chunkId;
        slot->sourceFileName.clear();
        slot->micClass = cabinetCaptureMicClass;
        slot->micModelName = cabinetCaptureMicModelName;
        slot->estimatedFrom.clear();
        slot->errorMessage.clear();
    }
}

void CaptureWizardState::markCabinetSlotImported(const juce::String& stepId,
                                                 const juce::String& chunkId,
                                                 const juce::String& sourceFileName,
                                                 CabinetMicClass micClass,
                                                 const juce::String& micModelName)
{
    if (auto* slot = findCabinetSlot(stepId))
    {
        slot->source = CabinetSlotSource::ImportedIr;
        slot->impulseResponseChunkId = chunkId;
        slot->sourceFileName = sourceFileName;
        slot->micClass = micClass;
        slot->micModelName = micModelName;
        slot->estimatedFrom.clear();
        slot->errorMessage.clear();
    }
}

void CaptureWizardState::applyCabinetCaptureMic(CabinetMicClass micClass, const juce::String& micModelName)
{
    cabinetCaptureMicClass = micClass;
    cabinetCaptureMicModelName = micModelName;

    for (auto& slot : cabinetSlots)
    {
        if (slot.source != CabinetSlotSource::CapturedIr)
            continue;

        slot.micClass = micClass;
        slot.micModelName = micModelName;
    }
}

void CaptureWizardState::markCabinetSlotCapturing(const juce::String& stepId)
{
    if (auto* slot = findCabinetSlot(stepId))
    {
        slot->source = CabinetSlotSource::Capturing;
        slot->errorMessage.clear();
    }
}

void CaptureWizardState::markCabinetSlotError(const juce::String& stepId, const juce::String& error)
{
    if (auto* slot = findCabinetSlot(stepId))
    {
        slot->source = CabinetSlotSource::Error;
        slot->errorMessage = error;
    }
}

void CaptureWizardState::resetCabinetSlot(const juce::String& stepId)
{
    if (auto* slot = findCabinetSlot(stepId))
    {
        const auto id = slot->id;
        const auto label = slot->label;
        *slot = {};
        slot->id = id;
        slot->label = label;
    }
}

bool CaptureWizardState::hasCabinetRealSource() const noexcept
{
    for (const auto& slot : cabinetSlots)
        if (slot.hasRealSource())
            return true;

    return false;
}

juce::StringArray CaptureWizardState::cabinetRealSourceIds() const
{
    juce::StringArray ids;
    for (const auto& slot : cabinetSlots)
        if (slot.hasRealSource())
            ids.add(slot.id);

    return ids;
}

void CaptureWizardState::estimateEmptyCabinetSlots()
{
    const auto sources = cabinetRealSourceIds();
    if (sources.isEmpty())
        return;

    // Real-source slots with a valid tone profile act as anchors on the mic
    // position axis; estimated slots receive an interpolated profile so the
    // package carries actual renderable data, not just a source label.
    struct Anchor
    {
        float position;
        const CabinetMicPositionSlot* slot;
    };

    std::vector<Anchor> anchors;
    for (const auto& slot : cabinetSlots)
        if (slot.hasRealSource() && slot.toneProfile.valid)
            anchors.push_back({ normalizedPositionForCabinetId(slot.id), &slot });

    std::sort(anchors.begin(), anchors.end(),
              [](const Anchor& a, const Anchor& b) { return a.position < b.position; });

    auto interpolatedCount = 0;
    for (auto& slot : cabinetSlots)
    {
        if (slot.source != CabinetSlotSource::Empty)
            continue;

        slot.source = CabinetSlotSource::EstimatedCompactCab;
        slot.estimatedFrom = sources.joinIntoString(",");

        if (! anchors.empty())
        {
            const auto position = normalizedPositionForCabinetId(slot.id);
            const Anchor* below = nullptr;
            const Anchor* above = nullptr;
            for (const auto& anchor : anchors)
            {
                if (anchor.position <= position)
                    below = &anchor;
                if (anchor.position >= position && above == nullptr)
                    above = &anchor;
            }

            if (below != nullptr && above != nullptr && below != above)
            {
                const auto range = juce::jmax(0.001f, above->position - below->position);
                slot.toneProfile = CabinetToneProfile::interpolate(below->slot->toneProfile,
                                                                   above->slot->toneProfile,
                                                                   (position - below->position) / range);
            }
            else
            {
                const auto* nearest = below != nullptr ? below : above;
                slot.toneProfile = nearest->slot->toneProfile;
                slot.toneProfile.estimated = true;
            }

            ++interpolatedCount;
        }

        if (auto* step = findStep(slot.id))
            step->status = CaptureStepStatus::Warning;
    }

    cabinetInterpolationComputed = interpolatedCount > 0 || ! anchors.empty();
}

juce::var CaptureWizardState::toCabinetMicMatrixVar() const
{
    return CabinetMicMatrixEstimator::estimate(cabinetSlots).toVar();
}

juce::var CaptureWizardState::toCabinetProfileVar(const juce::String& cabinetName,
                                                  const juce::String& micType,
                                                  const juce::String& speakerDescription,
                                                  const juce::String& notes) const
{
    auto profile = new juce::DynamicObject();
    profile->setProperty("cabinetName", cabinetName);
    profile->setProperty("micType", micType);
    profile->setProperty("speaker", speakerDescription);
    profile->setProperty("description", speakerDescription);
    profile->setProperty("notes", notes);

    profile->setProperty("positions", cabinetSlotsToVar(cabinetSlots, false));
    profile->setProperty("micMatrix", toCabinetMicMatrixVar());
    return profile;
}
}
