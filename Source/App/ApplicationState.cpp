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
