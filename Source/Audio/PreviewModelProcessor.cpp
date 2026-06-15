#include "Audio/PreviewModelProcessor.h"

namespace hanso
{
namespace
{
float safeGain(float db) noexcept
{
    return juce::Decibels::decibelsToGain(juce::jlimit(-36.0f, 24.0f, db));
}
}

void PreviewModelProcessor::prepare(double newSampleRate, int maximumBlockSize, int outputChannels) noexcept
{
    juce::ignoreUnused(maximumBlockSize);
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    preparedChannels = juce::jlimit(1, maxChannels, outputChannels);
    reset();
}

void PreviewModelProcessor::clear() noexcept
{
    modelLoaded = false;
    modelSummary.clear();
    reset();
}

bool PreviewModelProcessor::loadModel(const CompactHansoModel& model)
{
    if (! model.isValid())
        return false;

    modelLoaded = false;
    anchorCount = juce::jmin(maxAnchors, static_cast<int>(model.anchors.size()));
    if (anchorCount <= 0)
        return false;

    sampleRate = model.sampleRate > 0.0 ? model.sampleRate : sampleRate;
    for (int i = 0; i < anchorCount; ++i)
    {
        const auto& source = model.anchors[static_cast<size_t>(i)];
        anchors[static_cast<size_t>(i)] = { juce::jlimit(0.0f, 1.0f, source.parameterValue),
                                            source.inputGainDb,
                                            source.drive,
                                            source.outputGainDb,
                                            source.lowShelfDb,
                                            source.midPeakDb,
                                            source.highShelfDb };
    }

    std::sort(anchors.begin(),
              anchors.begin() + anchorCount,
              [](const RuntimeAnchor& a, const RuntimeAnchor& b)
              {
                  return a.parameterValue < b.parameterValue;
              });

    const auto parameterValue = targetParameter.load();
    const auto anchor = interpolateAnchor(parameterValue);
    updateToneCoefficients(anchor);
    currentInputGain = safeGain(anchor.inputGainDb);
    currentDrive = juce::jlimit(1.0f, 16.0f, anchor.drive);
    currentDriveNormalizer = 1.0f / juce::jmax(0.001f, std::tanh(currentDrive));
    currentOutputGain = safeGain(anchor.outputGainDb);

    modelSummary = model.summary() + " / preview gain "
                 + juce::String(parameterValue * 100.0f, 0) + "%";
    reset();
    modelLoaded = true;
    return true;
}

void PreviewModelProcessor::setPreviewGainPercent(float percent) noexcept
{
    targetParameter.store(juce::jlimit(0.0f, 1.0f, percent / 100.0f));
}

bool PreviewModelProcessor::hasModel() const noexcept
{
    return modelLoaded;
}

juce::String PreviewModelProcessor::summary() const
{
    return modelSummary;
}

void PreviewModelProcessor::reset() noexcept
{
    for (auto& channelStates : states)
        for (auto& state : channelStates)
            state.reset();
}

void PreviewModelProcessor::process(const float* const* inputChannels,
                                    int numInputChannels,
                                    juce::AudioBuffer<float>& outputBuffer) noexcept
{
    outputBuffer.clear();

    if (! modelLoaded || inputChannels == nullptr || numInputChannels <= 0)
        return;

    const auto numOutputChannels = juce::jmin(outputBuffer.getNumChannels(), preparedChannels);
    const auto numSamples = outputBuffer.getNumSamples();
    const auto anchor = interpolateAnchor(targetParameter.load());
    updateToneCoefficients(anchor);
    const auto targetInputGain = safeGain(anchor.inputGainDb);
    const auto targetDrive = juce::jlimit(1.0f, 16.0f, anchor.drive);
    const auto targetOutputGain = safeGain(anchor.outputGainDb);

    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        const auto sourceChannel = juce::jmin(channel, numInputChannels - 1);
        const auto* input = inputChannels[sourceChannel];
        auto* output = outputBuffer.getWritePointer(channel);
        if (input == nullptr || output == nullptr)
            continue;

        auto& lowState = states[static_cast<size_t>(channel)][0];
        auto& midState = states[static_cast<size_t>(channel)][1];
        auto& highState = states[static_cast<size_t>(channel)][2];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            currentInputGain += (targetInputGain - currentInputGain) * 0.0015f;
            currentDrive += (targetDrive - currentDrive) * 0.0015f;
            currentDriveNormalizer = 1.0f / juce::jmax(0.001f, std::tanh(currentDrive));
            currentOutputGain += (targetOutputGain - currentOutputGain) * 0.0015f;

            auto value = input[sample] * currentInputGain;
            value = std::tanh(value * currentDrive) * currentDriveNormalizer;
            value = lowState.process(value, coefficients[0]);
            value = midState.process(value, coefficients[1]);
            value = highState.process(value, coefficients[2]);
            output[sample] = juce::jlimit(-0.98f, 0.98f, value * currentOutputGain);
        }
    }

    for (int channel = numOutputChannels; channel < outputBuffer.getNumChannels(); ++channel)
        outputBuffer.clear(channel, 0, numSamples);
}

PreviewModelProcessor::RuntimeAnchor PreviewModelProcessor::interpolateAnchor(float parameterValue) const noexcept
{
    if (anchorCount <= 0)
        return {};

    const auto value = juce::jlimit(0.0f, 1.0f, parameterValue);
    if (anchorCount == 1 || value <= anchors[0].parameterValue)
        return anchors[0];

    const auto lastIndex = anchorCount - 1;
    if (value >= anchors[static_cast<size_t>(lastIndex)].parameterValue)
        return anchors[static_cast<size_t>(lastIndex)];

    for (int i = 0; i < lastIndex; ++i)
    {
        const auto& a = anchors[static_cast<size_t>(i)];
        const auto& b = anchors[static_cast<size_t>(i + 1)];
        if (value < a.parameterValue || value > b.parameterValue)
            continue;

        const auto range = juce::jmax(0.001f, b.parameterValue - a.parameterValue);
        const auto t = juce::jlimit(0.0f, 1.0f, (value - a.parameterValue) / range);

        RuntimeAnchor result;
        result.parameterValue = value;
        result.inputGainDb = a.inputGainDb + (b.inputGainDb - a.inputGainDb) * t;
        result.drive = a.drive + (b.drive - a.drive) * t;
        result.outputGainDb = a.outputGainDb + (b.outputGainDb - a.outputGainDb) * t;
        result.lowShelfDb = a.lowShelfDb + (b.lowShelfDb - a.lowShelfDb) * t;
        result.midPeakDb = a.midPeakDb + (b.midPeakDb - a.midPeakDb) * t;
        result.highShelfDb = a.highShelfDb + (b.highShelfDb - a.highShelfDb) * t;
        return result;
    }

    return anchors[static_cast<size_t>(lastIndex)];
}

void PreviewModelProcessor::updateToneCoefficients(const RuntimeAnchor& anchor) noexcept
{
    coefficients[0] = makeLowShelf(sampleRate, 180.0, anchor.lowShelfDb);
    coefficients[1] = makePeak(sampleRate, 950.0, 0.85f, anchor.midPeakDb);
    coefficients[2] = makeHighShelf(sampleRate, 3600.0, anchor.highShelfDb);
}

PreviewModelProcessor::BiquadCoefficients PreviewModelProcessor::makeLowShelf(double sr,
                                                                               double frequencyHz,
                                                                               float gainDb) noexcept
{
    const auto a = std::sqrt(juce::Decibels::decibelsToGain(gainDb));
    const auto w0 = juce::MathConstants<double>::twoPi * frequencyHz / sr;
    const auto cosW0 = static_cast<float>(std::cos(w0));
    const auto sinW0 = static_cast<float>(std::sin(w0));
    const auto sqrtA = std::sqrt(a);
    const auto alpha = sinW0 * std::sqrt(2.0f) / 2.0f;

    const auto b0 = a * ((a + 1.0f) - (a - 1.0f) * cosW0 + 2.0f * sqrtA * alpha);
    const auto b1 = 2.0f * a * ((a - 1.0f) - (a + 1.0f) * cosW0);
    const auto b2 = a * ((a + 1.0f) - (a - 1.0f) * cosW0 - 2.0f * sqrtA * alpha);
    const auto a0 = (a + 1.0f) + (a - 1.0f) * cosW0 + 2.0f * sqrtA * alpha;
    const auto a1 = -2.0f * ((a - 1.0f) + (a + 1.0f) * cosW0);
    const auto a2 = (a + 1.0f) + (a - 1.0f) * cosW0 - 2.0f * sqrtA * alpha;
    return normalise(b0, b1, b2, a0, a1, a2);
}

PreviewModelProcessor::BiquadCoefficients PreviewModelProcessor::makePeak(double sr,
                                                                          double frequencyHz,
                                                                          float q,
                                                                          float gainDb) noexcept
{
    const auto a = std::sqrt(juce::Decibels::decibelsToGain(gainDb));
    const auto w0 = juce::MathConstants<double>::twoPi * frequencyHz / sr;
    const auto alpha = static_cast<float>(std::sin(w0)) / (2.0f * juce::jmax(0.1f, q));
    const auto cosW0 = static_cast<float>(std::cos(w0));

    const auto b0 = 1.0f + alpha * a;
    const auto b1 = -2.0f * cosW0;
    const auto b2 = 1.0f - alpha * a;
    const auto a0 = 1.0f + alpha / a;
    const auto a1 = -2.0f * cosW0;
    const auto a2 = 1.0f - alpha / a;
    return normalise(b0, b1, b2, a0, a1, a2);
}

PreviewModelProcessor::BiquadCoefficients PreviewModelProcessor::makeHighShelf(double sr,
                                                                                double frequencyHz,
                                                                                float gainDb) noexcept
{
    const auto a = std::sqrt(juce::Decibels::decibelsToGain(gainDb));
    const auto w0 = juce::MathConstants<double>::twoPi * frequencyHz / sr;
    const auto cosW0 = static_cast<float>(std::cos(w0));
    const auto sinW0 = static_cast<float>(std::sin(w0));
    const auto sqrtA = std::sqrt(a);
    const auto alpha = sinW0 * std::sqrt(2.0f) / 2.0f;

    const auto b0 = a * ((a + 1.0f) + (a - 1.0f) * cosW0 + 2.0f * sqrtA * alpha);
    const auto b1 = -2.0f * a * ((a - 1.0f) + (a + 1.0f) * cosW0);
    const auto b2 = a * ((a + 1.0f) + (a - 1.0f) * cosW0 - 2.0f * sqrtA * alpha);
    const auto a0 = (a + 1.0f) - (a - 1.0f) * cosW0 + 2.0f * sqrtA * alpha;
    const auto a1 = 2.0f * ((a - 1.0f) - (a + 1.0f) * cosW0);
    const auto a2 = (a + 1.0f) - (a - 1.0f) * cosW0 - 2.0f * sqrtA * alpha;
    return normalise(b0, b1, b2, a0, a1, a2);
}

PreviewModelProcessor::BiquadCoefficients PreviewModelProcessor::normalise(float b0,
                                                                           float b1,
                                                                           float b2,
                                                                           float a0,
                                                                           float a1,
                                                                           float a2) noexcept
{
    BiquadCoefficients c;
    if (std::abs(a0) < 1.0e-8f)
        return c;

    c.b0 = b0 / a0;
    c.b1 = b1 / a0;
    c.b2 = b2 / a0;
    c.a1 = a1 / a0;
    c.a2 = a2 / a0;
    return c;
}
}
