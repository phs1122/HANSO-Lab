#pragma once

#include <JuceHeader.h>

#include "Capture/CaptureWizardState.h"
#include "Model/HansoPackage.h"

namespace hanso
{
class ApplicationState final
{
public:
    ApplicationState();

    HansoPackage& currentPackage() noexcept;
    const HansoPackage& currentPackage() const noexcept;
    CaptureWizardState& captureWizard() noexcept;
    const CaptureWizardState& captureWizard() const noexcept;
    void resetForNewCapture();
    bool hasUnsavedCaptureData() const noexcept;
    void markCaptureDataDirty() noexcept;
    void markCaptureDataSaved() noexcept;
    juce::File lastExportDirectory() const;
    void setLastExportDirectory(juce::File directory);

    void appendLog(juce::String message);
    juce::StringArray consumePendingLogs();

private:
    HansoPackage package;
    CaptureWizardState wizard;
    juce::File lastExportDir;
    bool unsavedCaptureData { false };
    juce::CriticalSection logLock;
    juce::StringArray pendingLogs;
};
}
