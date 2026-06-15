#pragma once

#include <JuceHeader.h>

#include "Capture/CaptureStep.h"

namespace hanso
{
class CaptureStatusIcon final : public juce::Component,
                                public juce::TooltipClient
{
public:
    std::function<void()> onClick;

    void setStatus(CaptureStepStatus newStatus);
    void setTooltipText(juce::String text);
    juce::String getTooltip() override;
    void paint(juce::Graphics& g) override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    CaptureStepStatus status { CaptureStepStatus::NotStarted };
    juce::String tooltipText;
};
}
