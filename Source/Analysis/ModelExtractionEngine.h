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
    ModelExtractionResult extractIntoPackage(HansoPackage& package) const;
};
}
