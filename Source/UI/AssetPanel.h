#pragma once

#include <JuceHeader.h>

#include "App/ApplicationState.h"
#include "UI/LabPanel.h"

namespace hanso
{
class AssetPanel final : public LabPanel
{
public:
    explicit AssetPanel(ApplicationState& state);
    void resized() override;

private:
    void exportPackage();
    void updatePackageMetadata();

    ApplicationState& appState;
    std::unique_ptr<juce::FileChooser> exportChooser;
    juce::Label title;
    juce::Label nameLabel;
    juce::TextEditor nameEditor;
    juce::Label manufacturerLabel;
    juce::TextEditor manufacturerEditor;
    juce::Label modelLabel;
    juce::TextEditor modelEditor;
    juce::Label yearLabel;
    juce::TextEditor yearEditor;
    juce::Label notesLabel;
    juce::TextEditor notesEditor;
    juce::Label categoryLabel;
    juce::ComboBox categoryBox;
    juce::Label exportPathLabel;
    juce::TextButton exportButton { "Export .hanso" };
};
}
