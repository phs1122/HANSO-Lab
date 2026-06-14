#include "UI/AnalysisPanel.h"

namespace hanso
{
AnalysisPanel::AnalysisPanel(LabWorkflow& labWorkflow)
    : workflow(labWorkflow)
{
    styleSectionTitle(title, "Analysis");
    addAndMakeVisible(title);

    runButton.onClick = [this]
    {
        const auto summary = workflow.runBasicAnalysis();
        summaryLabel.setText(LabWorkflow::formatSummary(summary), juce::dontSendNotification);
    };
    addAndMakeVisible(runButton);

    summaryLabel.setText("Ready", juce::dontSendNotification);
    addAndMakeVisible(summaryLabel);
}

void AnalysisPanel::resized()
{
    auto area = getLocalBounds().reduced(contentMargin);
    title.setBounds(area.removeFromTop(36));
    runButton.setBounds(area.removeFromTop(34).removeFromLeft(190));
    area.removeFromTop(12);
    summaryLabel.setBounds(area.removeFromTop(28));
}
}
