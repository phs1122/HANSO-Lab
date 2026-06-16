#include "UI/AssetPanel.h"

#include "App/Utf8.h"
#include "Model/HansoCategory.h"
#include "Serialization/HansoSerializer.h"

namespace hanso
{
AssetPanel::AssetPanel(ApplicationState& state)
    : appState(state)
{
    styleSectionTitle(title, "HANSO Package");
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

    micTypeLabel.setText("Mic Type", juce::dontSendNotification);
    addAndMakeVisible(micTypeLabel);
    micTypeEditor.setText(appState.currentPackage().metadata.sourceDevice);
    micTypeEditor.onTextChange = [this] { updatePackageMetadata(); };
    addAndMakeVisible(micTypeEditor);

    speakerDescriptionLabel.setText("Speaker / Cab", juce::dontSendNotification);
    addAndMakeVisible(speakerDescriptionLabel);
    speakerDescriptionEditor.setMultiLine(true);
    speakerDescriptionEditor.onTextChange = [this] { updatePackageMetadata(); };
    addAndMakeVisible(speakerDescriptionEditor);

    categoryLabel.setText("Category", juce::dontSendNotification);
    addAndMakeVisible(categoryLabel);

    const auto categories = allHansoCategoryNames();
    for (int i = 0; i < categories.size(); ++i)
        categoryBox.addItem(categories[i], i + 1);
    categoryBox.setSelectedId(categories.indexOf(toString(appState.currentPackage().metadata.category)) + 1,
                              juce::dontSendNotification);
    categoryBox.onChange = [this] { updatePackageMetadata(); };
    addAndMakeVisible(categoryBox);

    packageSummaryLabel.setText("Package: no capture chunks yet", juce::dontSendNotification);
    addAndMakeVisible(packageSummaryLabel);

    exportPathLabel.setText("Export path: -", juce::dontSendNotification);
    addAndMakeVisible(exportPathLabel);

    exportButton.setButtonText("Let's HANSO!");
    exportButton.onClick = [this] { exportPackage(); };
    addAndMakeVisible(exportButton);
    updateFieldVisibility();
}

void AssetPanel::refreshSummary()
{
    updateFieldVisibility();
    updatePackageSummary();
}

void AssetPanel::resized()
{
    auto area = getLocalBounds().reduced(contentMargin);
    title.setBounds(area.removeFromTop(36));
    const auto cabinet = isCabinetExport();

    auto row = area.removeFromTop(32);
    nameLabel.setBounds(row.removeFromLeft(120));
    nameEditor.setBounds(row.removeFromLeft(340));

    area.removeFromTop(12);
    row = area.removeFromTop(32);
    categoryLabel.setBounds(row.removeFromLeft(120));
    categoryBox.setBounds(row.removeFromLeft(180));

    area.removeFromTop(12);
    if (cabinet)
    {
        row = area.removeFromTop(32);
        micTypeLabel.setBounds(row.removeFromLeft(120));
        micTypeEditor.setBounds(row.removeFromLeft(340));

        area.removeFromTop(12);
        row = area.removeFromTop(76);
        speakerDescriptionLabel.setBounds(row.removeFromLeft(120));
        speakerDescriptionEditor.setBounds(row.removeFromLeft(460));

        area.removeFromTop(16);
        row = area.removeFromTop(84);
        notesLabel.setBounds(row.removeFromLeft(120));
        notesEditor.setBounds(row.removeFromLeft(460));
    }
    else
    {
        row = area.removeFromTop(32);
        manufacturerLabel.setBounds(row.removeFromLeft(120));
        manufacturerEditor.setBounds(row.removeFromLeft(340));

        area.removeFromTop(12);
        row = area.removeFromTop(32);
        modelLabel.setBounds(row.removeFromLeft(120));
        modelEditor.setBounds(row.removeFromLeft(340));

        area.removeFromTop(12);
        row = area.removeFromTop(32);
        yearLabel.setBounds(row.removeFromLeft(120));
        yearEditor.setBounds(row.removeFromLeft(120));

        area.removeFromTop(24);
        row = area.removeFromTop(96);
        notesLabel.setBounds(row.removeFromLeft(120));
        notesEditor.setBounds(row.removeFromLeft(460));
    }

    area.removeFromTop(24);
    packageSummaryLabel.setBounds(area.removeFromTop(28));
    area.removeFromTop(12);
    exportButton.setBounds(area.removeFromTop(34).removeFromLeft(150));
    area.removeFromTop(12);
    exportPathLabel.setBounds(area.removeFromTop(28));
}

void AssetPanel::updatePackageMetadata()
{
    auto& metadata = appState.currentPackage().metadata;
    metadata.name = nameEditor.getText();
    metadata.notes = notesEditor.getText();

    if (isCabinetExport())
    {
        metadata.category = HansoCategory::Cabinet;
        metadata.deviceType = "Cabinet";
        metadata.model = nameEditor.getText();
        metadata.sourceDevice = micTypeEditor.getText();
        metadata.manufacturer = {};
        metadata.year = {};
        appState.currentPackage().cabinetProfile =
            appState.captureWizard().toCabinetProfileVar(nameEditor.getText(),
                                                         micTypeEditor.getText(),
                                                         speakerDescriptionEditor.getText(),
                                                         notesEditor.getText());
        categoryBox.setSelectedId(allHansoCategoryNames().indexOf("Cabinet") + 1, juce::dontSendNotification);
    }
    else
    {
        metadata.category = hansoCategoryFromString(categoryBox.getText());
        metadata.manufacturer = manufacturerEditor.getText();
        metadata.model = modelEditor.getText();
        metadata.year = yearEditor.getText();
        metadata.deviceType = categoryBox.getText();
        metadata.sourceDevice = {};
        appState.currentPackage().cabinetProfile = juce::var();
    }
}

void AssetPanel::updatePackageSummary()
{
    const auto& package = appState.currentPackage();
    auto summary = "Package v" + juce::String(package.formatVersion)
                 + " / chunks " + juce::String(package.chunks.size());

    if (const auto* captured = package.findChunk("capture/captured.f32"))
        summary += " / captured " + juce::String(captured->numChannels)
                + " ch x " + juce::String(captured->numSamples) + " samples";
    else if (const auto* gainCaptured = package.findChunk("capture/gain-010/aligned-captured.pcm16"))
        summary += " / gain anchors / aligned " + juce::String(gainCaptured->numChannels)
                + " ch x " + juce::String(gainCaptured->numSamples) + " samples";

    if (package.findChunk("capture/aligned-captured.f32") != nullptr
        || package.findChunk("capture/gain-010/aligned-captured.pcm16") != nullptr)
        summary += " / aligned";

    if (package.findChunk("model/compact-v1.hmodel") != nullptr)
        summary += " / compact realtime model";

    if (appState.captureWizard().captureType == CaptureType::Cabinet)
    {
        int realSources = 0;
        int estimated = 0;
        for (const auto& slot : appState.captureWizard().cabinetSlots)
        {
            if (slot.hasRealSource())
                ++realSources;
            else if (slot.source == CabinetSlotSource::EstimatedCompactCab)
                ++estimated;
        }

        summary += " / cabinet sources " + juce::String(realSources)
                + " real, " + juce::String(estimated) + " estimated";
    }

    if (! appState.captureWizard().isExportReady())
        summary += " / " + appState.captureWizard().exportDisabledReason();
    else if (appState.captureWizard().hasWarnings())
        summary += " / warnings";

    packageSummaryLabel.setText(summary, juce::dontSendNotification);
}

bool AssetPanel::isCabinetExport() const noexcept
{
    return appState.captureWizard().captureType == CaptureType::Cabinet;
}

void AssetPanel::updateFieldVisibility()
{
    const auto cabinet = isCabinetExport();
    categoryBox.setEnabled(! cabinet);

    manufacturerLabel.setVisible(! cabinet);
    manufacturerEditor.setVisible(! cabinet);
    modelLabel.setVisible(! cabinet);
    modelEditor.setVisible(! cabinet);
    yearLabel.setVisible(! cabinet);
    yearEditor.setVisible(! cabinet);

    micTypeLabel.setVisible(cabinet);
    micTypeEditor.setVisible(cabinet);
    speakerDescriptionLabel.setVisible(cabinet);
    speakerDescriptionEditor.setVisible(cabinet);

    nameLabel.setText(cabinet ? "Cabinet Name" : "Name", juce::dontSendNotification);
    if (cabinet)
        categoryBox.setSelectedId(allHansoCategoryNames().indexOf("Cabinet") + 1, juce::dontSendNotification);
}

void AssetPanel::exportPackage()
{
    updatePackageMetadata();
    updatePackageSummary();

    auto& wizard = appState.captureWizard();
    appState.currentPackage().captureWorkflow = wizard.toMetadataVar();

    if (! wizard.isExportReady())
    {
        appState.appendLog(wizard.exportDisabledReason());
        exportPathLabel.setText(wizard.exportDisabledReason(), juce::dontSendNotification);
        return;
    }

    if (wizard.hasWarnings() && ! wizard.warningExportAccepted)
    {
        juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon,
                                           "Export with warnings?",
                                           utf8("Some captures have warnings. The model may be less accurate.\n그래도 export 할까요?"),
                                           "Export",
                                           "Cancel",
                                           this,
                                           juce::ModalCallbackFunction::create([this](int result)
                                           {
                                               if (result != 0)
                                               {
                                                   appState.captureWizard().warningExportAccepted = true;
                                                   exportPackage();
                                               }
                                           }));
        return;
    }

    auto fileName = appState.currentPackage().metadata.name.trim();
    if (fileName.isEmpty())
        fileName = "Untitled HANSO Asset";

    exportChooser = std::make_unique<juce::FileChooser>("Choose export folder",
                                                        appState.lastExportDirectory(),
                                                        juce::String());

    exportChooser->launchAsync(juce::FileBrowserComponent::openMode
                                   | juce::FileBrowserComponent::canSelectDirectories,
        [this, fileName](const juce::FileChooser& fc)
        {
            auto directory = fc.getResult();
            if (directory == juce::File())
            {
                exportChooser = nullptr;
                return;
            }

            if (! directory.isDirectory())
                directory = directory.getParentDirectory();

            auto file = directory.getChildFile(fileName);
            if (! file.hasFileExtension(".hanso"))
                file = file.withFileExtension(".hanso");

            handleExportDestination(file);
            exportChooser = nullptr;
        });
}

void AssetPanel::handleExportDestination(juce::File file)
{
    if (! file.existsAsFile())
    {
        writePackageToFile(std::move(file));
        return;
    }

    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::WarningIcon)
        .withTitle("File already exists")
        .withMessage("A file with this name already exists.")
        .withButton("Replace")
        .withButton("Create Unique Filename")
        .withButton("Cancel")
        .withAssociatedComponent(this);

    juce::AlertWindow::showAsync(options, [this, file](int result)
    {
        if (result == 1)
            writePackageToFile(file);
        else if (result == 2)
            writePackageToFile(createUniqueFile(file));
    });
}

void AssetPanel::writePackageToFile(juce::File file)
{
    juce::String error;
    if (HansoSerializer::writeToFile(appState.currentPackage(), file, error))
    {
        appState.setLastExportDirectory(file.getParentDirectory());
        appState.markCaptureDataSaved();
        exportPathLabel.setText("Export path: " + file.getFullPathName(), juce::dontSendNotification);
        appState.appendLog("Exported package: " + file.getFullPathName());
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                               "Let's HANSO!",
                                               utf8("저장되었습니다.\n") + file.getFullPathName());
    }
    else
    {
        appState.appendLog("Export failed: " + error);
    }
}

juce::File AssetPanel::createUniqueFile(const juce::File& requestedFile)
{
    const auto directory = requestedFile.getParentDirectory();
    const auto baseName = requestedFile.getFileNameWithoutExtension();
    const auto extension = requestedFile.getFileExtension();

    for (int i = 1; i < 10000; ++i)
    {
        auto candidate = directory.getChildFile(baseName + "_" + juce::String(i).paddedLeft('0', 2) + extension);
        if (! candidate.exists())
            return candidate;
    }

    return requestedFile.getNonexistentSibling();
}
}
