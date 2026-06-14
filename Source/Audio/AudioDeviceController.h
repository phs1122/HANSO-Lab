#pragma once

#include <JuceHeader.h>

namespace hanso
{
class AudioDeviceController final
{
public:
    explicit AudioDeviceController(juce::AudioDeviceManager& manager);

    juce::String initialise();
    void showSettingsDialog(juce::Component* parent);
    juce::AudioDeviceManager& deviceManager() noexcept;
    juce::String describeCurrentDevice() const;

private:
    juce::AudioDeviceManager& audioDeviceManager;
};
}
