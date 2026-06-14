#include "Audio/CaptureAudioSource.h"

namespace hanso
{
void CaptureAudioSource::prepare(double sampleRate, int maximumBlockSize, int)
{
    juce::ignoreUnused(sampleRate, maximumBlockSize);
}

void CaptureAudioSource::release()
{
    stopCapture();
}

void CaptureAudioSource::setMonitoringEnabled(bool shouldMonitor) noexcept
{
    monitoringEnabled = shouldMonitor;
}

bool CaptureAudioSource::isMonitoringEnabled() const noexcept
{
    return monitoringEnabled;
}

void CaptureAudioSource::setCaptureInputChannel(int zeroBasedChannelIndex) noexcept
{
    selectedInputChannel.store(zeroBasedChannelIndex < 0 ? -1 : zeroBasedChannelIndex);
}

int CaptureAudioSource::captureInputChannel() const noexcept
{
    return selectedInputChannel.load();
}

void CaptureAudioSource::setCaptureChannelCount(int channels) noexcept
{
    selectedCaptureChannels.store(juce::jlimit(1, 2, channels));
}

int CaptureAudioSource::captureChannelCount() const noexcept
{
    return selectedCaptureChannels.load();
}

void CaptureAudioSource::loadCaptureSignal(const juce::AudioBuffer<float>& drySignal)
{
    stopCapture();
    playbackSignal.makeCopyOf(drySignal, true);
    capturedSignal.setSize(selectedCaptureChannels.load(), playbackSignal.getNumSamples(), false, true, true);
    capturedSignal.clear();
    playhead.store(0);
    captureFinished.store(false);
}

bool CaptureAudioSource::startCapture() noexcept
{
    if (playbackSignal.getNumSamples() <= 0)
        return false;

    capturedSignal.clear();
    playhead.store(0);
    captureFinished.store(false);
    captureRunning.store(true);
    return true;
}

void CaptureAudioSource::stopCapture() noexcept
{
    captureRunning.store(false);
}

bool CaptureAudioSource::isCaptureRunning() const noexcept
{
    return captureRunning.load();
}

bool CaptureAudioSource::hasCaptureFinished() const noexcept
{
    return captureFinished.load();
}

juce::AudioBuffer<float> CaptureAudioSource::copyCapturedSignal() const
{
    juce::AudioBuffer<float> copy;
    copy.makeCopyOf(capturedSignal, true);
    return copy;
}

float CaptureAudioSource::inputLevel() const noexcept
{
    return latestInputLevel.load();
}

float CaptureAudioSource::outputLevel() const noexcept
{
    return latestOutputLevel.load();
}

void CaptureAudioSource::process(const float* const* inputChannelData,
                                 int numInputChannels,
                                 juce::AudioBuffer<float>& outputBuffer) noexcept
{
    outputBuffer.clear();

    auto inputPeak = 0.0f;
    auto selectedChannel = selectedInputChannel.load();
    const auto desiredChannels = selectedCaptureChannels.load();

    if (selectedChannel < 0 && inputChannelData != nullptr && numInputChannels > 0)
    {
        auto bestPeak = 0.0f;
        auto bestChannel = 0;
        const auto step = desiredChannels == 2 ? 2 : 1;

        for (int channel = 0; channel < numInputChannels; channel += step)
        {
            auto pairPeak = 0.0f;
            const auto channelsToCheck = juce::jmin(desiredChannels, numInputChannels - channel);

            for (int offset = 0; offset < channelsToCheck; ++offset)
            {
                const auto* input = inputChannelData[channel + offset];
                if (input == nullptr)
                    continue;

                auto channelPeak = 0.0f;
                for (int sample = 0; sample < outputBuffer.getNumSamples(); ++sample)
                    channelPeak = juce::jmax(channelPeak, std::abs(input[sample]));

                pairPeak = juce::jmax(pairPeak, channelPeak);
            }

            if (pairPeak > bestPeak)
            {
                bestPeak = pairPeak;
                bestChannel = channel;
            }
        }

        selectedChannel = bestChannel;
        latestResolvedInputChannel.store(bestChannel);
    }
    else
    {
        selectedChannel = juce::jlimit(0, juce::jmax(0, numInputChannels - 1), juce::jmax(0, selectedChannel));
        latestResolvedInputChannel.store(selectedChannel);
    }

    const auto availableInputChannels = juce::jmax(0, numInputChannels - selectedChannel);
    const auto channelsToCapture = availableInputChannels > 0
                                  ? juce::jlimit(1,
                                                 juce::jmax(1, capturedSignal.getNumChannels()),
                                                 juce::jmin(selectedCaptureChannels.load(), availableInputChannels))
                                  : 0;
    const auto* selectedInput = (numInputChannels > 0 && inputChannelData != nullptr) ? inputChannelData[selectedChannel] : nullptr;

    if (inputChannelData != nullptr && selectedChannel < numInputChannels)
    {
        for (int captureChannel = 0; captureChannel < channelsToCapture; ++captureChannel)
        {
            const auto sourceChannel = selectedChannel + captureChannel;
            const auto* input = sourceChannel < numInputChannels ? inputChannelData[sourceChannel] : nullptr;
            if (input == nullptr)
                continue;

            for (int sample = 0; sample < outputBuffer.getNumSamples(); ++sample)
                inputPeak = juce::jmax(inputPeak, std::abs(input[sample]));
        }
    }

    auto outputPeak = 0.0f;

    if (captureRunning.load())
    {
        auto position = playhead.load();
        const auto remaining = playbackSignal.getNumSamples() - position;
        const auto samplesToCopy = juce::jmin(outputBuffer.getNumSamples(), remaining);

        if (samplesToCopy > 0)
        {
            for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
                outputBuffer.copyFrom(channel, 0, playbackSignal, 0, position, samplesToCopy);

            if (inputChannelData != nullptr)
            {
                for (int captureChannel = 0; captureChannel < channelsToCapture; ++captureChannel)
                {
                    const auto sourceChannel = selectedChannel + captureChannel;
                    const auto* input = sourceChannel < numInputChannels ? inputChannelData[sourceChannel] : nullptr;
                    if (input != nullptr)
                        capturedSignal.copyFrom(captureChannel, position, input, samplesToCopy);
                }
            }

            outputPeak = outputBuffer.getMagnitude(0, 0, samplesToCopy);
            playhead.store(position + samplesToCopy);
        }

        if (samplesToCopy < outputBuffer.getNumSamples() || position + samplesToCopy >= playbackSignal.getNumSamples())
        {
            captureRunning.store(false);
            captureFinished.store(true);
        }
    }
    else if (monitoringEnabled && selectedInput != nullptr && channelsToCapture > 0)
    {
        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
        {
            const auto sourceOffset = channel % channelsToCapture;
            const auto sourceChannel = selectedChannel + sourceOffset;
            const auto* input = sourceChannel < numInputChannels ? inputChannelData[sourceChannel] : selectedInput;
            if (input != nullptr)
                outputBuffer.copyFrom(channel, 0, input, outputBuffer.getNumSamples());
        }

        outputPeak = inputPeak;
    }

    latestInputLevel.store(inputPeak);
    latestOutputLevel.store(outputPeak);
}
}
