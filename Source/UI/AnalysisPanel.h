#pragma once

#include <JuceHeader.h>

#include "App/LabWorkflow.h"
#include "UI/LabPanel.h"

namespace hanso
{
class AnalysisPanel final : public LabPanel
{
public:
    explicit AnalysisPanel(LabWorkflow& workflow);
    void resized() override;

private:
    LabWorkflow& workflow;
    juce::Label title;
    juce::TextButton runButton { "Run Basic Analysis" };
    juce::Label summaryLabel;
};
}
