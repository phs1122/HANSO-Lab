#pragma once

#include <JuceHeader.h>

#include "Model/HansoPackage.h"

namespace hanso
{
class ApplicationState final
{
public:
    ApplicationState();

    HansoPackage& currentPackage() noexcept;
    const HansoPackage& currentPackage() const noexcept;

    void appendLog(juce::String message);
    juce::StringArray consumePendingLogs();

private:
    HansoPackage package;
    juce::CriticalSection logLock;
    juce::StringArray pendingLogs;
};
}
