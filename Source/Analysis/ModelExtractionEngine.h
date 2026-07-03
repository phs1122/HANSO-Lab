#pragma once

#include <JuceHeader.h>

#include "Model/CompactHansoModel.h"
#include "Model/HansoPackage.h"

namespace hanso
{
struct ModelExtractionResult
{
    bool success { false };
    CompactHansoModel model;
    juce::String message;
};

class ModelExtractionEngine final
{
public:
    juce::var buildDspCoreFromAnalysis(const HansoPackage& package) const;
    ModelExtractionResult extractIntoPackage(HansoPackage& package) const;
};
}
