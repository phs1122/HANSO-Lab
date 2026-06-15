#include "Capture/CaptureWizardState.h"

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
    : recipe(CaptureRecipe::createBasicAmpLiquidGain())
{
    currentStepId = recipe.steps.empty() ? juce::String() : recipe.steps.front().stepId;
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
    return firstIncompleteRequiredStep() == nullptr;
}

juce::String CaptureWizardState::exportDisabledReason() const
{
    if (const auto* step = firstIncompleteRequiredStep())
        return "Export disabled: " + step->title + " is not completed.";

    return {};
}

juce::var CaptureWizardState::toMetadataVar() const
{
    auto object = new juce::DynamicObject();
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
            const auto prefix = step.anchor.chunkPathPrefix();
            anchor->setProperty("dryChunkId", "capture/shared/dry-reference.pcm16");
            anchor->setProperty("capturedChunkId", juce::var());
            anchor->setProperty("alignedChunkId", prefix + "/aligned-captured.pcm16");
        }

        anchors.add(anchor);
    }
    recipeObject->setProperty("anchors", anchors);

    auto interpolation = new juce::DynamicObject();
    interpolation->setProperty("parameterKey", "gain");
    interpolation->setProperty("method", "future-nonlinear-fit");
    interpolation->setProperty("status", "notComputed");
    recipeObject->setProperty("interpolationPlan", interpolation);

    object->setProperty("captureRecipe", recipeObject);
    return object;
}
}
