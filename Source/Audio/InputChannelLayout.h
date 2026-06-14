#pragma once

#include <JuceHeader.h>

namespace hanso
{
struct InputChannelLayout
{
    juce::StringArray channelNames;
    juce::BigInteger activeChannels;

    int numDeviceChannels() const noexcept
    {
        return channelNames.size();
    }

    int numCallbackChannels() const noexcept
    {
        return activeChannels.countNumberOfSetBits();
    }

    bool isSparseCallbackLayout(int numInputChannels) const noexcept
    {
        return numDeviceChannels() > 0 && numInputChannels >= numDeviceChannels();
    }

    bool isDeviceChannelActive(int deviceChannel) const noexcept
    {
        return deviceChannel >= 0 && deviceChannel < numDeviceChannels() && activeChannels[deviceChannel];
    }

    int deviceChannelToCallbackIndex(int deviceChannel) const noexcept
    {
        if (! isDeviceChannelActive(deviceChannel))
            return -1;

        auto callbackIndex = 0;
        for (int channel = 0; channel < deviceChannel; ++channel)
            if (activeChannels[channel])
                ++callbackIndex;

        return callbackIndex;
    }

    int callbackIndexForDeviceChannel(int deviceChannel, int numInputChannels) const noexcept
    {
        if (deviceChannel < 0)
            return -1;

        if (isSparseCallbackLayout(numInputChannels))
            return (deviceChannel < numInputChannels && isDeviceChannelActive(deviceChannel))
                 ? deviceChannel
                 : -1;

        return deviceChannelToCallbackIndex(deviceChannel);
    }

    const float* channelDataForDeviceChannel(const float* const* inputChannelData,
                                             int numInputChannels,
                                             int deviceChannel) const noexcept
    {
        if (inputChannelData == nullptr || numInputChannels <= 0 || deviceChannel < 0)
            return nullptr;

        const auto callbackIndex = callbackIndexForDeviceChannel(deviceChannel, numInputChannels);
        if (callbackIndex < 0 || callbackIndex >= numInputChannels)
            return nullptr;

        return inputChannelData[callbackIndex];
    }

    const float* channelDataAtCallbackIndex(const float* const* inputChannelData,
                                            int numInputChannels,
                                            int callbackIndex) const noexcept
    {
        if (inputChannelData == nullptr || callbackIndex < 0 || callbackIndex >= numInputChannels)
            return nullptr;

        return inputChannelData[callbackIndex];
    }

    juce::String describeActiveChannels() const
    {
        if (numDeviceChannels() == 0)
            return "none";

        juce::StringArray parts;

        for (int channel = 0; channel < numDeviceChannels(); ++channel)
        {
            if (! activeChannels[channel])
                continue;

            const auto name = channelNames[channel];
            parts.add("[" + juce::String(channel) + "]=" + name
                      + " -> cb " + juce::String(deviceChannelToCallbackIndex(channel)));
        }

        return parts.isEmpty() ? "none enabled" : parts.joinIntoString(", ");
    }

    static InputChannelLayout fromDevice(juce::AudioIODevice& device)
    {
        InputChannelLayout layout;
        layout.channelNames = device.getInputChannelNames();
        layout.activeChannels = device.getActiveInputChannels();

        if (layout.numDeviceChannels() > 0)
            layout.activeChannels.setRange(layout.numDeviceChannels(),
                                           layout.activeChannels.getHighestBit() + 1 - layout.numDeviceChannels(),
                                           false);

        return layout;
    }
};
}
