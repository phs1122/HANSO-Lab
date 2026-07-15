#pragma once

#include <JuceHeader.h>

#include "Capture/CaptureStep.h"
#include "Model/HansoCategory.h"

namespace hanso
{
struct FixedControlInstruction
{
    juce::String controlName;
    juce::String positionLabel;
    float normalizedValue { 0.5f };
};

struct CaptureRecipe
{
    juce::String recipeId;
    juce::String displayName;
    HansoCategory category { HansoCategory::Amp };
    std::vector<FixedControlInstruction> fixedControls;
    std::vector<CaptureStep> steps;

    static CaptureRecipe createBasicAmpLiquidGain();
    static CaptureRecipe createStaticPedalCapture();
    static CaptureRecipe createCabinetMicPositions();
};
}
