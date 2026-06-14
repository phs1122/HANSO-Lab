#include "UI/AssetPanel.h"

#include "Model/HansoCategory.h"
#include "Serialization/HansoSerializer.h"

namespace hanso
{
AssetPanel::AssetPanel(ApplicationState& state)
    : appState(state)
{
    styleSectionTitle(title, "Asset Package");
    addAndMakeVisible(title);

    nameLabel.setText("Name", juce::dontSendNotification);
    addAndMakeVisible(nameLabel);

    nameEditor.setText(appState.currentPackage().metadata.name);
    nameEditor.onTextChange = [this] { updatePackageMetadata(); };
    addAndMakeVisible(nameEditor);

    manufacturerLabel.setText("Manufacturer", juce::dontSendNotification);
    addAndMakeVisible(manufacturerLabel);
    manufacturerEditor.onTextChange = [this] { updatePackageMetadata(); };
    addAndMakeVisible(manufacturerEditor);

    modelLabel.setText("Model", juce::dontSendNotification);
    addAndMakeVisible(modelLabel);
    modelEditor.onTextChange = [this] { updatePackageMetadata(); };
    addAndMakeVisible(modelEditor);

    yearLabel.setText("Year", juce::dontSendNotification);
    addAndMakeVisible(yearLabel);
    yearEditor.onTextChange = [this] { updatePackageMetadata(); };
    addAndMakeVisible(yearEditor);

    notesLabel.setText("Notes", juce::dontSendNotification);
    addAndMakeVisible(notesLabel);
    notesEditor.setMultiLine(true);
    notesEditor.onTextChange = [this] { updatePackageMetadata(); };
    addAndMakeVisible(notesEditor);

    categoryLabel.setText("Category", juce::dontSendNotification);
    addAndMakeVisible(categoryLabel);

    const auto categories = allHansoCategoryNames();
    for (int i = 0; i < categories.size(); ++i)
        categoryBox.addItem(categories[i], i + 1);
    categoryBox.setSelectedId(1);
    categoryBox.onChange = [this] { updatePackageMetadata(); };
    addAndMakeVisible(categoryBox);

    exportPathLabel.setText("Export path: -", juce::dontSendNotification);
    addAndMakeVisible(exportPathLabel);

    exportButton.onClick = [this] { exportPackage(); };
    addAndMakeVisible(exportButton);
}

void AssetPanel::resized()
{
    auto area = getLocalBounds().reduced(contentMargin);
    title.setBounds(area.removeFromTop(36));

    auto row = area.removeFromTop(32);
    nameLabel.setBounds(row.removeFromLeft(100));
    nameEditor.setBounds(row.removeFromLeft(320));

    area.removeFromTop(12);
    row = area.removeFromTop(32);
    categoryLabel.setBounds(row.removeFromLeft(100));
    categoryBox.setBounds(row.removeFromLeft(180));

    area.removeFromTop(12);
    row = area.removeFromTop(32);
    manufacturerLabel.setBounds(row.removeFromLeft(100));
    manufacturerEditor.setBounds(row.removeFromLeft(320));

    area.removeFromTop(12);
    row = area.removeFromTop(32);
    modelLabel.setBounds(row.removeFromLeft(100));
    modelEditor.setBounds(row.removeFromLeft(320));

    area.removeFromTop(12);
    row = area.removeFromTop(32);
    yearLabel.setBounds(row.removeFromLeft(100));
    yearEditor.setBounds(row.removeFromLeft(120));

    area.removeFromTop(24);
    row = area.removeFromTop(96);
    notesLabel.setBounds(row.removeFromLeft(100));
    notesEditor.setBounds(row.removeFromLeft(460));

    area.removeFromTop(24);
    exportButton.setBounds(area.removeFromTop(34).removeFromLeft(150));
    area.removeFromTop(12);
    exportPathLabel.setBounds(area.removeFromTop(28));
}

void AssetPanel::updatePackageMetadata()
{
    auto& metadata = appState.currentPackage().metadata;
    metadata.name = nameEditor.getText();
    metadata.category = hansoCategoryFromString(categoryBox.getText());
    metadata.manufacturer = manufacturerEditor.getText();
    metadata.model = modelEditor.getText();
    metadata.year = yearEditor.getText();
    metadata.deviceType = categoryBox.getText();
    metadata.notes = notesEditor.getText();
}

void AssetPanel::exportPackage()
{
    updatePackageMetadata();

    auto fileName = appState.currentPackage().metadata.name.trim();
    if (fileName.isEmpty())
        fileName = "Untitled HANSO Asset";

    exportChooser = std::make_unique<juce::FileChooser>(
        "Export HANSO package",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile(fileName + ".hanso"),
        "*.hanso");

    exportChooser->launchAsync(juce::FileBrowserComponent::saveMode
                                   | juce::FileBrowserComponent::canSelectFiles
                                   | juce::FileBrowserComponent::warnAboutOverwriting,
        [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File())
            {
                exportChooser = nullptr;
                return;
            }

            if (! file.hasFileExtension(".hanso"))
                file = file.withFileExtension(".hanso");

            juce::String error;
            if (HansoSerializer::writeToFile(appState.currentPackage(), file, error))
            {
                exportPathLabel.setText("Export path: " + file.getFullPathName(), juce::dontSendNotification);
                appState.appendLog("Exported package: " + file.getFullPathName());
            }
            else
            {
                appState.appendLog("Export failed: " + error);
            }

            exportChooser = nullptr;
        });
}
}
