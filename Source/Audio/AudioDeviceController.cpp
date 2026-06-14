#include "Audio/AudioDeviceController.h"

namespace hanso
{
namespace
{
juce::AudioIODeviceType* findActiveDeviceType(juce::AudioDeviceManager& manager)
{
    const auto& types = manager.getAvailableDeviceTypes();

    for (auto* type : types)
    {
        if (! type->getDeviceNames(true).isEmpty() || ! type->getDeviceNames(false).isEmpty())
            return type;
    }

    return nullptr;
}

juce::AudioDeviceManager::AudioDeviceSetup buildPreferredAudioSetup(juce::AudioDeviceManager& manager)
{
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    setup.useDefaultInputChannels = true;
    setup.useDefaultOutputChannels = true;

    if (auto* deviceType = findActiveDeviceType(manager))
    {
        const auto outputNames = deviceType->getDeviceNames(false);
        const auto inputNames = deviceType->getDeviceNames(true);

        const auto defaultOutputIndex = deviceType->getDefaultDeviceIndex(false);
        if (defaultOutputIndex >= 0 && defaultOutputIndex < outputNames.size())
            setup.outputDeviceName = outputNames[defaultOutputIndex];

        if (setup.outputDeviceName.isNotEmpty() && inputNames.contains(setup.outputDeviceName))
        {
            setup.inputDeviceName = setup.outputDeviceName;
        }
        else if (! inputNames.isEmpty())
        {
            const auto defaultInputIndex = deviceType->getDefaultDeviceIndex(true);
            if (defaultInputIndex >= 0 && defaultInputIndex < inputNames.size())
                setup.inputDeviceName = inputNames[defaultInputIndex];
        }
    }

    return setup;
}
}

AudioDeviceController::AudioDeviceController(juce::AudioDeviceManager& manager)
    : audioDeviceManager(manager)
{
}

juce::String AudioDeviceController::initialise()
{
    const auto preferredSetup = buildPreferredAudioSetup(audioDeviceManager);
    const auto error = audioDeviceManager.initialise(4, 4, nullptr, true, {}, &preferredSetup);

    if (error.isNotEmpty())
        return audioDeviceManager.initialise(4, 4, nullptr, true);

    return {};
}

void AudioDeviceController::showSettingsDialog(juce::Component* parent)
{
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Audio Settings";
    options.content.setOwned(new juce::AudioDeviceSelectorComponent(audioDeviceManager,
                                                                    0, 8, 0, 8,
                                                                    true, true, true, false));
    options.content->setSize(560, 440);
    options.componentToCentreAround = parent;
    options.resizable = true;
    options.useNativeTitleBar = true;
    options.launchAsync();
}

juce::AudioDeviceManager& AudioDeviceController::deviceManager() noexcept
{
    return audioDeviceManager;
}

juce::String AudioDeviceController::describeCurrentDevice() const
{
    if (auto* device = audioDeviceManager.getCurrentAudioDevice())
    {
        const auto setup = audioDeviceManager.getAudioDeviceSetup();

        return "out: " + setup.outputDeviceName
             + " / in: " + setup.inputDeviceName
             + " / " + device->getName()
             + " / " + juce::String(device->getCurrentSampleRate(), 0) + " Hz"
             + " / " + juce::String(device->getCurrentBufferSizeSamples()) + " samples"
             + " / in " + juce::String(device->getActiveInputChannels().countNumberOfSetBits())
             + " / out " + juce::String(device->getActiveOutputChannels().countNumberOfSetBits());
    }

    return "No audio device";
}
}
