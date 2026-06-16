#include "Capture/CaptureWizardState.h"

namespace hanso
{
namespace
{
std::vector<CabinetMicPositionSlot> createDefaultCabinetSlots()
{
    auto makeSlot = [](const char* id, const char* label)
    {
        CabinetMicPositionSlot slot;
        slot.id = id;
        slot.label = label;
        return slot;
    };

    std::vector<CabinetMicPositionSlot> slots;
    slots.push_back(makeSlot("cab-center", "Center"));
    slots.push_back(makeSlot("cab-edge", "Edge"));
    slots.push_back(makeSlot("cab-cone", "Cone"));
    slots.push_back(makeSlot("cab-off-axis", "Off-Axis"));
    return slots;
}

juce::var qualityToVar(const CaptureQualityReport& report)
{
    auto object = new juce::DynamicObject();
    object->setProperty("overallSeverity", toString(report.overallSeverity));
    object->setProperty("peakDbfs", report.peakDbfs);
    object->setProperty("rmsDbfs", report.rmsDbfs);
    object->setProperty("clipSampleCount", report.clipSampleCount);
    object->setProperty("noiseFloorDbfs", report.noiseFloorDbfs);
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
    : CaptureWizardState(CaptureType::Amp)
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
            return "Export disabled: " + juce::String::fromUTF8("캐비넷을 내보내려면 최소 1개 이상의 마이크 위치를 캡쳐하거나 IR로 가져와야 합니다.");
        }

        return "Export disabled: " + step->title + " is not completed.";
    }

    if (captureType == CaptureType::Cabinet && ! hasCabinetRealSource())
        return "Export disabled: " + juce::String::fromUTF8("캐비넷을 내보내려면 최소 1개 이상의 마이크 위치를 캡쳐하거나 IR로 가져와야 합니다.");

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
        }
        else
        {
            const auto isCabinetPosition = captureType == CaptureType::Cabinet
                                        && step.anchor.parameterKey == "cabinet-position";
            const auto prefix = step.anchor.chunkPathPrefix();
            anchor->setProperty("dryChunkId", isCabinetPosition ? "capture/shared/cabinet-dry-reference.pcm16"
                                                                : "capture/shared/dry-reference.pcm16");
            anchor->setProperty("capturedChunkId", juce::var());
            anchor->setProperty("alignedChunkId", isCabinetPosition ? "cabinet/positions/" + step.stepId + "/ir.pcm16"
                                                                    : prefix + "/aligned-captured.pcm16");
        }

        anchors.add(anchor);
    }
    recipeObject->setProperty("anchors", anchors);

    if (captureType == CaptureType::Cabinet)
    {
        juce::Array<juce::var> positions;
        for (const auto& slot : cabinetSlots)
        {
            auto position = new juce::DynamicObject();
            position->setProperty("id", slot.id);
            position->setProperty("label", slot.label);
            position->setProperty("source", toString(slot.source));
            position->setProperty("impulseResponseChunkId",
                                  slot.impulseResponseChunkId.isNotEmpty()
                                      ? juce::var(slot.impulseResponseChunkId)
                                      : juce::var());
            position->setProperty("sourceFileName",
                                  slot.sourceFileName.isNotEmpty()
                                      ? juce::var(slot.sourceFileName)
                                      : juce::var());
            position->setProperty("estimatedFrom",
                                  slot.estimatedFrom.isNotEmpty()
                                      ? juce::var(slot.estimatedFrom)
                                      : juce::var());
            position->setProperty("error",
                                  slot.errorMessage.isNotEmpty()
                                      ? juce::var(slot.errorMessage)
                                      : juce::var());
            positions.add(position);
        }

        recipeObject->setProperty("positions", positions);
    }

    auto interpolation = new juce::DynamicObject();
    interpolation->setProperty("parameterKey", captureType == CaptureType::Cabinet ? "micPosition" : "gain");
    interpolation->setProperty("method", captureType == CaptureType::Cabinet ? "source-anchor-estimation" : "future-nonlinear-fit");
    interpolation->setProperty("status", "notComputed");
    recipeObject->setProperty("interpolationPlan", interpolation);

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
        slot->estimatedFrom.clear();
        slot->errorMessage.clear();
    }
}

void CaptureWizardState::markCabinetSlotImported(const juce::String& stepId, const juce::String& chunkId, const juce::String& sourceFileName)
{
    if (auto* slot = findCabinetSlot(stepId))
    {
        slot->source = CabinetSlotSource::ImportedIr;
        slot->impulseResponseChunkId = chunkId;
        slot->sourceFileName = sourceFileName;
        slot->estimatedFrom.clear();
        slot->errorMessage.clear();
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

    for (auto& slot : cabinetSlots)
    {
        if (slot.source == CabinetSlotSource::Empty)
        {
            slot.source = CabinetSlotSource::EstimatedCompactCab;
            slot.estimatedFrom = sources.joinIntoString(",");
            if (auto* step = findStep(slot.id))
                step->status = CaptureStepStatus::Warning;
        }
    }
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

    juce::Array<juce::var> positions;
    for (const auto& slot : cabinetSlots)
    {
        auto position = new juce::DynamicObject();
        position->setProperty("id", slot.id);
        position->setProperty("label", slot.label);
        position->setProperty("source", toString(slot.source));
        position->setProperty("impulseResponseChunkId",
                              slot.impulseResponseChunkId.isNotEmpty()
                                  ? juce::var(slot.impulseResponseChunkId)
                                  : juce::var());
        position->setProperty("sourceFileName",
                              slot.sourceFileName.isNotEmpty()
                                  ? juce::var(slot.sourceFileName)
                                  : juce::var());
        position->setProperty("estimatedFrom",
                              slot.estimatedFrom.isNotEmpty()
                                  ? juce::var(slot.estimatedFrom)
                                  : juce::var());
        positions.add(position);
    }

    profile->setProperty("positions", positions);
    return profile;
}
}
