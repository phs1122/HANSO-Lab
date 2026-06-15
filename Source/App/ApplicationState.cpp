#include "App/ApplicationState.h"

namespace hanso
{
ApplicationState::ApplicationState()
{
    package.metadata.name = "Untitled HANSO Asset";
    package.metadata.category = HansoCategory::Amp;
    package.metadata.deviceType = "Amp";
}

HansoPackage& ApplicationState::currentPackage() noexcept
{
    return package;
}

const HansoPackage& ApplicationState::currentPackage() const noexcept
{
    return package;
}

CaptureWizardState& ApplicationState::captureWizard() noexcept
{
    return wizard;
}

const CaptureWizardState& ApplicationState::captureWizard() const noexcept
{
    return wizard;
}

void ApplicationState::resetForNewCapture()
{
    package = {};
    package.metadata.name = "Untitled HANSO Asset";
    package.metadata.category = HansoCategory::Amp;
    package.metadata.deviceType = "Amp";
    wizard = {};
    unsavedCaptureData = false;
}

bool ApplicationState::hasUnsavedCaptureData() const noexcept
{
    return unsavedCaptureData;
}

void ApplicationState::markCaptureDataDirty() noexcept
{
    unsavedCaptureData = true;
}

void ApplicationState::markCaptureDataSaved() noexcept
{
    unsavedCaptureData = false;
}

juce::File ApplicationState::lastExportDirectory() const
{
    if (lastExportDir.exists())
        return lastExportDir;

    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
}

void ApplicationState::setLastExportDirectory(juce::File directory)
{
    if (directory.isDirectory())
        lastExportDir = std::move(directory);
}

void ApplicationState::appendLog(juce::String message)
{
    const juce::ScopedLock lock(logLock);
    pendingLogs.add(juce::Time::getCurrentTime().toString(true, true) + "  " + message);
}

juce::StringArray ApplicationState::consumePendingLogs()
{
    const juce::ScopedLock lock(logLock);
    auto logs = pendingLogs;
    pendingLogs.clear();
    return logs;
}
}
