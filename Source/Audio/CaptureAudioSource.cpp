#include "Audio/CaptureAudioSource.h"

namespace hanso
{
void CaptureAudioSource::prepare(double sampleRate, int maximumBlockSize, int outputChannels)
{
    previewProcessor.prepare(sampleRate, maximumBlockSize, outputChannels);
    cabinetProcessor.prepare(sampleRate, maximumBlockSize, outputChannels);
    cabinetIrProcessor.prepare(sampleRate, maximumBlockSize, outputChannels);
    previewScratchBuffer.setSize(juce::jmax(1, outputChannels),
                                 juce::jmax(1, maximumBlockSize),
                                 false,
                                 false,
                                 true);
}

void CaptureAudioSource::release()
{
    stopCapture();
    stopCalibrationSignal();
    stopPreviewSample();
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

void CaptureAudioSource::setOutputRoutingMode(OutputRoutingMode mode) noexcept
{
    selectedOutputRouting.store(static_cast<int>(mode));
}

OutputRoutingMode CaptureAudioSource::outputRoutingMode() const noexcept
{
    return static_cast<OutputRoutingMode>(selectedOutputRouting.load());
}

void CaptureAudioSource::setInputChannelLayout(const InputChannelLayout& layout)
{
    inputLayout = layout;
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

void CaptureAudioSource::loadCalibrationSignal(const juce::AudioBuffer<float>& signal)
{
    stopCalibrationSignal();
    calibrationSignal.makeCopyOf(signal, true);
    calibrationPlayhead.store(0);
}

void CaptureAudioSource::replaceCalibrationSignal(const juce::AudioBuffer<float>& signal)
{
    calibrationSignal.makeCopyOf(signal, true);
    calibrationPlayhead.store(0);
}

bool CaptureAudioSource::startCapture() noexcept
{
    if (playbackSignal.getNumSamples() <= 0)
        return false;

    capturedSignal.clear();
    playhead.store(0);
    captureFinished.store(false);
    calibrationRunning.store(false);
    captureRunning.store(true);
    return true;
}

void CaptureAudioSource::stopCapture() noexcept
{
    captureRunning.store(false);
}

bool CaptureAudioSource::startCalibrationSignal() noexcept
{
    if (calibrationSignal.getNumSamples() <= 0)
        return false;

    captureRunning.store(false);
    captureFinished.store(false);
    calibrationPlayhead.store(0);
    calibrationRunning.store(true);
    return true;
}

void CaptureAudioSource::stopCalibrationSignal() noexcept
{
    calibrationRunning.store(false);
}

bool CaptureAudioSource::isCalibrationSignalRunning() const noexcept
{
    return calibrationRunning.load();
}

bool CaptureAudioSource::isCaptureRunning() const noexcept
{
    return captureRunning.load();
}

double CaptureAudioSource::captureProgress() const noexcept
{
    const auto total = playbackSignal.getNumSamples();
    if (total <= 0)
        return 0.0;

    return juce::jlimit(0.0, 1.0, static_cast<double>(playhead.load()) / static_cast<double>(total));
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

bool CaptureAudioSource::loadPreviewModel(const CompactHansoModel& model)
{
    clearPreviewCabinetPackage();
    return previewProcessor.loadModel(model);
}

void CaptureAudioSource::setPreviewGainPercent(float percent) noexcept
{
    previewProcessor.setPreviewGainPercent(percent);
}

void CaptureAudioSource::clearPreviewModel() noexcept
{
    stopPreviewSample();
    previewProcessor.clear();
    cabinetProcessor.reset();
}

bool CaptureAudioSource::loadPreviewCabinetPackage(const HansoPackage& package, juce::String& error)
{
    clearPreviewModel();
    if (! cabinetIrProcessor.loadFromPackage(package, error))
        return false;

    return true;
}

void CaptureAudioSource::setPreviewMicPositionNormalized(float normalizedPosition) noexcept
{
    cabinetIrProcessor.setMicPositionNormalized(normalizedPosition);
}

void CaptureAudioSource::clearPreviewCabinetPackage() noexcept
{
    stopPreviewSample();
    cabinetIrProcessor.clear();
}

bool CaptureAudioSource::hasPreviewCabinetPackage() const noexcept
{
    return cabinetIrProcessor.hasModel();
}

juce::String CaptureAudioSource::previewCabinetSummary() const
{
    return cabinetIrProcessor.summary();
}

void CaptureAudioSource::loadPreviewSample(const juce::AudioBuffer<float>& sample)
{
    stopPreviewSample();
    previewSampleSignal.makeCopyOf(sample, true);
    previewSamplePlayhead.store(0);
}

bool CaptureAudioSource::startPreviewSample() noexcept
{
    if (previewSampleSignal.getNumSamples() <= 0)
        return false;

    stopCapture();
    stopCalibrationSignal();
    setMonitoringEnabled(false);
    previewProcessor.reset();
    cabinetProcessor.reset();
    cabinetIrProcessor.reset();
    previewSamplePlayhead.store(0);
    previewSampleRunning.store(true);
    return true;
}

void CaptureAudioSource::stopPreviewSample() noexcept
{
    previewSampleRunning.store(false);
    previewSamplePlayhead.store(0);
    previewProcessor.reset();
    cabinetProcessor.reset();
    cabinetIrProcessor.reset();
}

bool CaptureAudioSource::isPreviewSamplePlaying() const noexcept
{
    return previewSampleRunning.load();
}

double CaptureAudioSource::previewSampleProgress() const noexcept
{
    const auto total = previewSampleSignal.getNumSamples();
    if (total <= 0)
        return 0.0;

    return juce::jlimit(0.0, 1.0, static_cast<double>(previewSamplePlayhead.load()) / static_cast<double>(total));
}

void CaptureAudioSource::seekPreviewSample(double normalizedProgress) noexcept
{
    const auto total = previewSampleSignal.getNumSamples();
    if (total <= 0)
        return;

    const auto clamped = juce::jlimit(0.0, 1.0, normalizedProgress);
    previewSamplePlayhead.store(static_cast<int>(std::round(clamped * static_cast<double>(total - 1))));
}

void CaptureAudioSource::setPreviewLoopEnabled(bool enabled) noexcept
{
    previewLoopEnabled.store(enabled);
}

bool CaptureAudioSource::isPreviewLoopEnabled() const noexcept
{
    return previewLoopEnabled.load();
}

void CaptureAudioSource::setPreviewOutputGainDb(float gainDb) noexcept
{
    previewOutputGainDbValue.store(juce::jlimit(-24.0f, 12.0f, gainDb));
}

float CaptureAudioSource::previewOutputGainDb() const noexcept
{
    return previewOutputGainDbValue.load();
}

void CaptureAudioSource::setPreviewNormalizationEnabled(bool enabled) noexcept
{
    previewNormalizationEnabled.store(enabled);
}

bool CaptureAudioSource::isPreviewNormalizationEnabled() const noexcept
{
    return previewNormalizationEnabled.load();
}

void CaptureAudioSource::setPreviewCabinetEnabled(bool enabled) noexcept
{
    previewCabinetEnabled.store(enabled);
    if (! enabled)
        cabinetProcessor.reset();
}

bool CaptureAudioSource::isPreviewCabinetEnabled() const noexcept
{
    return previewCabinetEnabled.load();
}

bool CaptureAudioSource::hasPreviewModel() const noexcept
{
    return previewProcessor.hasModel();
}

juce::String CaptureAudioSource::previewModelSummary() const
{
    return previewProcessor.summary();
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

        const auto searchChannels = inputLayout.numDeviceChannels() > 0
                                  ? inputLayout.numDeviceChannels()
                                  : numInputChannels;

        for (int channel = 0; channel < searchChannels; channel += step)
        {
            auto pairPeak = 0.0f;
            const auto channelsToCheck = juce::jmin(desiredChannels, searchChannels - channel);

            for (int offset = 0; offset < channelsToCheck; ++offset)
            {
                const auto* input = inputLayout.numDeviceChannels() > 0
                                  ? inputLayout.channelDataForDeviceChannel(inputChannelData,
                                                                            numInputChannels,
                                                                            channel + offset)
                                  : inputLayout.channelDataAtCallbackIndex(inputChannelData,
                                                                           numInputChannels,
                                                                           channel + offset);
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
        const auto maxSelectableChannel = inputLayout.numDeviceChannels() > 0
                                        ? inputLayout.numDeviceChannels() - 1
                                        : numInputChannels - 1;
        selectedChannel = juce::jlimit(0, juce::jmax(0, maxSelectableChannel), juce::jmax(0, selectedChannel));
        latestResolvedInputChannel.store(selectedChannel);
    }

    const auto selectableChannels = inputLayout.numDeviceChannels() > 0
                                  ? inputLayout.numDeviceChannels()
                                  : numInputChannels;
    const auto availableInputChannels = juce::jmax(0, selectableChannels - selectedChannel);
    const auto channelsToCapture = availableInputChannels > 0
                                  ? juce::jlimit(1,
                                                 juce::jmax(1, capturedSignal.getNumChannels()),
                                                 juce::jmin(selectedCaptureChannels.load(), availableInputChannels))
                                  : 0;
    const auto* selectedInput = inputLayout.numDeviceChannels() > 0
                              ? inputLayout.channelDataForDeviceChannel(inputChannelData, numInputChannels, selectedChannel)
                              : inputLayout.channelDataAtCallbackIndex(inputChannelData, numInputChannels, selectedChannel);

    if (inputChannelData != nullptr && selectedChannel < selectableChannels)
    {
        for (int captureChannel = 0; captureChannel < channelsToCapture; ++captureChannel)
        {
            const auto sourceChannel = selectedChannel + captureChannel;
            const auto* input = inputLayout.numDeviceChannels() > 0
                              ? inputLayout.channelDataForDeviceChannel(inputChannelData,
                                                                        numInputChannels,
                                                                        sourceChannel)
                              : inputLayout.channelDataAtCallbackIndex(inputChannelData,
                                                                       numInputChannels,
                                                                       sourceChannel);
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
            const auto routing = outputRoutingMode();
            if (routing == OutputRoutingMode::MonoLeftOnly)
            {
                if (outputBuffer.getNumChannels() > 0)
                    outputBuffer.copyFrom(0, 0, playbackSignal, 0, position, samplesToCopy);
            }
            else
            {
                for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
                    outputBuffer.copyFrom(channel, 0, playbackSignal, 0, position, samplesToCopy);
            }

            if (inputChannelData != nullptr)
            {
                for (int captureChannel = 0; captureChannel < channelsToCapture; ++captureChannel)
                {
                    const auto sourceChannel = selectedChannel + captureChannel;
                    const auto* input = inputLayout.numDeviceChannels() > 0
                                      ? inputLayout.channelDataForDeviceChannel(inputChannelData,
                                                                                numInputChannels,
                                                                                sourceChannel)
                                      : inputLayout.channelDataAtCallbackIndex(inputChannelData,
                                                                               numInputChannels,
                                                                               sourceChannel);
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
    else if (calibrationRunning.load() && calibrationSignal.getNumSamples() > 0)
    {
        auto position = calibrationPlayhead.load();
        const auto numSamples = outputBuffer.getNumSamples();
        auto samplesWritten = 0;
        const auto routing = outputRoutingMode();

        while (samplesWritten < numSamples)
        {
            const auto remainingInSignal = calibrationSignal.getNumSamples() - position;
            const auto samplesToCopy = juce::jmin(numSamples - samplesWritten, remainingInSignal);

            if (routing == OutputRoutingMode::MonoLeftOnly)
            {
                if (outputBuffer.getNumChannels() > 0)
                    outputBuffer.copyFrom(0, samplesWritten, calibrationSignal, 0, position, samplesToCopy);
            }
            else
            {
                for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
                    outputBuffer.copyFrom(channel, samplesWritten, calibrationSignal, 0, position, samplesToCopy);
            }

            samplesWritten += samplesToCopy;
            position += samplesToCopy;
            if (position >= calibrationSignal.getNumSamples())
                position = 0;
        }

        calibrationPlayhead.store(position);
        outputPeak = outputBuffer.getMagnitude(0, 0, outputBuffer.getNumSamples());
    }
    else if (previewSampleRunning.load() && previewSampleSignal.getNumSamples() > 0)
    {
        auto position = previewSamplePlayhead.load();
        const auto totalSamples = previewSampleSignal.getNumSamples();
        const auto numSamples = outputBuffer.getNumSamples();
        auto samplesWritten = 0;

        previewScratchBuffer.clear();
        while (samplesWritten < numSamples && previewSampleRunning.load())
        {
            const auto remaining = totalSamples - position;
            const auto samplesToCopy = juce::jmin(numSamples - samplesWritten, remaining);
            const auto channelsToRead = juce::jmin(previewScratchBuffer.getNumChannels(),
                                                   previewSampleSignal.getNumChannels());
            for (int channel = 0; channel < channelsToRead; ++channel)
                previewScratchBuffer.copyFrom(channel, samplesWritten, previewSampleSignal, channel, position, samplesToCopy);

            if (previewSampleSignal.getNumChannels() == 1 && previewScratchBuffer.getNumChannels() > 1)
            {
                for (int channel = 1; channel < previewScratchBuffer.getNumChannels(); ++channel)
                    previewScratchBuffer.copyFrom(channel, samplesWritten, previewScratchBuffer, 0, samplesWritten, samplesToCopy);
            }

            samplesWritten += samplesToCopy;
            position += samplesToCopy;

            if (position >= totalSamples)
            {
                if (previewLoopEnabled.load())
                {
                    position = 0;
                }
                else
                {
                    previewSampleRunning.store(false);
                    break;
                }
            }
        }

        const auto inputChannelsForPreview = juce::jmin(previewScratchBuffer.getNumChannels(), 2);
        const auto* const* previewInputs = previewScratchBuffer.getArrayOfReadPointers();

        if (cabinetIrProcessor.hasModel())
        {
            cabinetIrProcessor.process(previewInputs, inputChannelsForPreview, outputBuffer, samplesWritten);
        }
        else if (previewProcessor.hasModel())
        {
            previewProcessor.process(previewInputs, inputChannelsForPreview, outputBuffer);
            if (previewCabinetEnabled.load())
                cabinetProcessor.process(outputBuffer, samplesWritten);
        }
        else
        {
            const auto channelsToWrite = juce::jmin(outputBuffer.getNumChannels(), inputChannelsForPreview);
            for (int channel = 0; channel < channelsToWrite; ++channel)
                outputBuffer.copyFrom(channel, 0, previewScratchBuffer, channel, 0, samplesWritten);

            if (channelsToWrite == 1 && outputBuffer.getNumChannels() > 1)
                for (int channel = 1; channel < outputBuffer.getNumChannels(); ++channel)
                    outputBuffer.copyFrom(channel, 0, previewScratchBuffer, 0, 0, samplesWritten);
        }

        applyPreviewMonitoring(outputBuffer, samplesWritten);

        if (samplesWritten > 0)
        {
            outputPeak = outputBuffer.getMagnitude(0, 0, samplesWritten);
            for (int channel = 1; channel < outputBuffer.getNumChannels(); ++channel)
                outputPeak = juce::jmax(outputPeak, outputBuffer.getMagnitude(channel, 0, samplesWritten));
        }

        previewSamplePlayhead.store(position);
    }
    else if (monitoringEnabled && selectedInput != nullptr && channelsToCapture > 0)
    {
        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
        {
            const auto sourceOffset = channel % channelsToCapture;
            const auto sourceChannel = selectedChannel + sourceOffset;
            const auto* input = inputLayout.numDeviceChannels() > 0
                              ? inputLayout.channelDataForDeviceChannel(inputChannelData,
                                                                        numInputChannels,
                                                                        sourceChannel)
                              : inputLayout.channelDataAtCallbackIndex(inputChannelData,
                                                                       numInputChannels,
                                                                       sourceChannel);
            if (input == nullptr)
                input = selectedInput;
            if (input != nullptr)
                outputBuffer.copyFrom(channel, 0, input, outputBuffer.getNumSamples());
        }

        outputPeak = inputPeak;
    }

    latestInputLevel.store(inputPeak);
    latestOutputLevel.store(outputPeak);
}

void CaptureAudioSource::applyPreviewMonitoring(juce::AudioBuffer<float>& outputBuffer, int numSamples) noexcept
{
    if (numSamples <= 0)
        return;

    const auto channelsToProcess = juce::jmin(outputBuffer.getNumChannels(), 2);

    auto rmsSum = 0.0f;
    for (int channel = 0; channel < channelsToProcess; ++channel)
        rmsSum += outputBuffer.getRMSLevel(channel, 0, numSamples);
    const auto blockRms = rmsSum / static_cast<float>(juce::jmax(1, channelsToProcess));
    const auto targetRms = juce::Decibels::decibelsToGain(-20.0f);
    const auto rawNorm = targetRms / juce::jmax(blockRms, 1.0e-6f);
    const auto targetNorm = previewNormalizationEnabled.load()
                          ? juce::jlimit(juce::Decibels::decibelsToGain(-12.0f),
                                         juce::Decibels::decibelsToGain(12.0f),
                                         rawNorm)
                          : 1.0f;
    const auto targetOutputGain = juce::Decibels::decibelsToGain(previewOutputGainDbValue.load());

    for (int sample = 0; sample < numSamples; ++sample)
    {
        currentPreviewNormalizationGain += (targetNorm - currentPreviewNormalizationGain) * 0.006f;
        currentPreviewOutputGain += (targetOutputGain - currentPreviewOutputGain) * 0.004f;
        const auto gain = currentPreviewNormalizationGain * currentPreviewOutputGain;

        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
        {
            auto* samples = outputBuffer.getWritePointer(channel);
            samples[sample] = juce::jlimit(-0.99f, 0.99f, samples[sample] * gain);
        }
    }
}
}
