#include "Audio/PreviewMicColorProcessor.h"

#include "Capture/CabinetMicPositions.h"

#include <cmath>

namespace hanso
{
namespace
{
constexpr float positionEpsilon = 0.0005f;
constexpr float previewDeltaLimitDb = 12.0f;

int classIndexFor(CabinetMicClass micClass) noexcept
{
    switch (micClass)
    {
        case CabinetMicClass::Dynamic: return 0;
        case CabinetMicClass::Ribbon: return 1;
        case CabinetMicClass::Condenser: return 2;
        case CabinetMicClass::Unknown: break;
    }

    return -1;
}
}

void PreviewMicColorProcessor::prepare(double newSampleRate, int outputChannels) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    preparedChannels = juce::jlimit(1, maxChannels, outputChannels);
    reset();
    loadedClassIndex = -2;
    loadedPosition = -1.0f;
}

void PreviewMicColorProcessor::reset() noexcept
{
    for (auto& channel : filters)
        for (auto& state : channel)
            state.reset();
    currentLevelGainLinear = targetLevelGainLinear;
}

void PreviewMicColorProcessor::clear() noexcept
{
    matrixLoaded.store(false);
    positionDeltas.clear();
    targetClassIndex.store(-1);
    loadedClassIndex = -2;
    loadedPosition = -1.0f;
    targetLevelGainLinear = 1.0f;
    reset();
}

bool PreviewMicColorProcessor::loadFromPackage(const HansoPackage& package)
{
    clear();

    const auto* profileObject = package.cabinetProfile.getDynamicObject();
    if (profileObject == nullptr)
        return false;

    const auto* matrixObject = profileObject->getProperty("micMatrix").getDynamicObject();
    if (matrixObject == nullptr)
        return false;

    const auto* entryArray = matrixObject->getProperty("entries").getArray();
    if (entryArray == nullptr || entryArray->isEmpty())
        return false;

    // Reference mic class per position: the slot's own mic, dynamic when unknown.
    std::array<int, 8> referenceClassByPosition {};
    referenceClassByPosition.fill(0);
    if (const auto* positionArray = profileObject->getProperty("positions").getArray())
    {
        for (const auto& entry : *positionArray)
        {
            const auto* positionObject = entry.getDynamicObject();
            if (positionObject == nullptr)
                continue;

            const auto index = positionIndexForCabinetId(positionObject->getProperty("id").toString());
            const auto micClass = cabinetMicClassFromString(positionObject->getProperty("micClass").toString());
            const auto classIndex = classIndexFor(micClass);
            if (index >= 0 && index < static_cast<int>(referenceClassByPosition.size()) && classIndex >= 0)
                referenceClassByPosition[static_cast<size_t>(index)] = classIndex;
        }
    }

    struct ParsedEntry
    {
        bool valid { false };
        CabinetToneProfile profile;
    };

    std::array<std::array<ParsedEntry, classCount>, 8> parsed {};
    auto maxPositionIndex = -1;
    for (const auto& entry : *entryArray)
    {
        const auto* entryObject = entry.getDynamicObject();
        if (entryObject == nullptr)
            continue;

        const auto classIndex = classIndexFor(cabinetMicClassFromString(entryObject->getProperty("micClass").toString()));
        const auto positionIndex = positionIndexForCabinetId(entryObject->getProperty("positionId").toString());
        if (classIndex < 0 || positionIndex < 0 || positionIndex >= static_cast<int>(parsed.size()))
            continue;

        auto& slot = parsed[static_cast<size_t>(positionIndex)][static_cast<size_t>(classIndex)];
        slot.profile = CabinetToneProfile::fromVar(entryObject->getProperty("toneProfile"));
        slot.valid = slot.profile.valid;
        maxPositionIndex = juce::jmax(maxPositionIndex, positionIndex);
    }

    if (maxPositionIndex < 0)
        return false;

    for (int positionIndex = 0; positionIndex <= maxPositionIndex; ++positionIndex)
    {
        const auto& classEntries = parsed[static_cast<size_t>(positionIndex)];
        const auto referenceIndex = referenceClassByPosition[static_cast<size_t>(positionIndex)];
        const auto& reference = classEntries[static_cast<size_t>(referenceIndex)];
        if (! reference.valid)
            continue;

        PositionDeltas deltas;
        deltas.normalizedPosition = kCabinetMicPositions[positionIndex].normalizedPosition;
        for (int classIndex = 0; classIndex < classCount; ++classIndex)
        {
            const auto& target = classEntries[static_cast<size_t>(classIndex)];
            if (! target.valid)
                continue;

            for (size_t band = 0; band < deltas.bandDeltasDb[static_cast<size_t>(classIndex)].size(); ++band)
                deltas.bandDeltasDb[static_cast<size_t>(classIndex)][band] =
                    juce::jlimit(-previewDeltaLimitDb, previewDeltaLimitDb,
                                 target.profile.bandGainsDb[band] - reference.profile.bandGainsDb[band]);
            deltas.levelDeltasDb[static_cast<size_t>(classIndex)] =
                juce::jlimit(-previewDeltaLimitDb, previewDeltaLimitDb,
                             target.profile.levelDb - reference.profile.levelDb);
        }

        positionDeltas.push_back(deltas);
    }

    if (positionDeltas.empty())
        return false;

    std::sort(positionDeltas.begin(), positionDeltas.end(),
              [](const PositionDeltas& a, const PositionDeltas& b)
              {
                  return a.normalizedPosition < b.normalizedPosition;
              });

    loadedClassIndex = -2;
    loadedPosition = -1.0f;
    matrixLoaded.store(true);
    return true;
}

void PreviewMicColorProcessor::setTargetMicClass(CabinetMicClass micClass) noexcept
{
    targetClassIndex.store(classIndexFor(micClass));
}

void PreviewMicColorProcessor::setMicPositionNormalized(float normalizedPosition) noexcept
{
    micPositionNormalized.store(juce::jlimit(0.0f, 1.0f, normalizedPosition));
}

bool PreviewMicColorProcessor::hasMatrix() const noexcept
{
    return matrixLoaded.load();
}

PreviewMicColorProcessor::BiquadCoefficients PreviewMicColorProcessor::makePeak(double sampleRate,
                                                                                double frequencyHz,
                                                                                float q,
                                                                                float gainDb) noexcept
{
    const auto w0 = juce::MathConstants<double>::twoPi * frequencyHz / sampleRate;
    const auto cosW0 = std::cos(w0);
    const auto alpha = std::sin(w0) / (2.0 * juce::jmax(0.1, static_cast<double>(q)));
    const auto A = std::pow(10.0, static_cast<double>(gainDb) / 40.0);

    const auto b0 = 1.0 + alpha * A;
    const auto b1 = -2.0 * cosW0;
    const auto b2 = 1.0 - alpha * A;
    const auto a0 = 1.0 + alpha / A;
    const auto a1 = -2.0 * cosW0;
    const auto a2 = 1.0 - alpha / A;

    BiquadCoefficients coeffs;
    coeffs.b0 = static_cast<float>(b0 / a0);
    coeffs.b1 = static_cast<float>(b1 / a0);
    coeffs.b2 = static_cast<float>(b2 / a0);
    coeffs.a1 = static_cast<float>(a1 / a0);
    coeffs.a2 = static_cast<float>(a2 / a0);
    return coeffs;
}

void PreviewMicColorProcessor::interpolateDeltas(float normalizedPosition,
                                                 int classIndex,
                                                 std::array<float, static_cast<size_t>(cabinetToneBandCount)>& bandsDb,
                                                 float& levelDb) const noexcept
{
    bandsDb.fill(0.0f);
    levelDb = 0.0f;
    if (positionDeltas.empty() || classIndex < 0 || classIndex >= classCount)
        return;

    const auto clamped = juce::jlimit(0.0f, 1.0f, normalizedPosition);
    const auto* lower = &positionDeltas.front();
    const auto* upper = &positionDeltas.back();
    for (size_t i = 1; i < positionDeltas.size(); ++i)
    {
        if (positionDeltas[i].normalizedPosition >= clamped)
        {
            upper = &positionDeltas[i];
            lower = &positionDeltas[i - 1];
            break;
        }
    }

    const auto span = juce::jmax(0.0001f, upper->normalizedPosition - lower->normalizedPosition);
    const auto mix = juce::jlimit(0.0f, 1.0f, (clamped - lower->normalizedPosition) / span);

    const auto& lowerBands = lower->bandDeltasDb[static_cast<size_t>(classIndex)];
    const auto& upperBands = upper->bandDeltasDb[static_cast<size_t>(classIndex)];
    for (size_t band = 0; band < bandsDb.size(); ++band)
        bandsDb[band] = lowerBands[band] + (upperBands[band] - lowerBands[band]) * mix;

    const auto lowerLevel = lower->levelDeltasDb[static_cast<size_t>(classIndex)];
    const auto upperLevel = upper->levelDeltasDb[static_cast<size_t>(classIndex)];
    levelDb = lowerLevel + (upperLevel - lowerLevel) * mix;
}

void PreviewMicColorProcessor::rebuildCoefficientsIfNeeded() noexcept
{
    const auto classIndex = targetClassIndex.load();
    const auto position = micPositionNormalized.load();
    if (classIndex == loadedClassIndex && std::abs(position - loadedPosition) < positionEpsilon)
        return;

    std::array<float, static_cast<size_t>(cabinetToneBandCount)> bandsDb {};
    auto levelDb = 0.0f;
    interpolateDeltas(position, classIndex, bandsDb, levelDb);

    for (int band = 0; band < cabinetToneBandCount; ++band)
    {
        const auto lowHz = kCabinetToneBands[band].lowHz;
        const auto highHz = kCabinetToneBands[band].highHz;
        const auto centreHz = std::sqrt(lowHz * highHz);
        const auto q = static_cast<float>(centreHz / juce::jmax(1.0, highHz - lowHz));
        coefficients[static_cast<size_t>(band)] =
            makePeak(sampleRate, centreHz, q, bandsDb[static_cast<size_t>(band)]);
    }

    targetLevelGainLinear = std::pow(10.0f, levelDb / 20.0f);
    loadedClassIndex = classIndex;
    loadedPosition = position;
}

void PreviewMicColorProcessor::process(juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    if (! matrixLoaded.load() || numSamples <= 0)
        return;

    rebuildCoefficientsIfNeeded();

    if (loadedClassIndex < 0)
        return; // original mic selected: strict bypass

    const auto channels = juce::jmin(buffer.getNumChannels(), maxChannels);
    for (int channel = 0; channel < channels; ++channel)
    {
        auto* samples = buffer.getWritePointer(channel);
        auto& channelFilters = filters[static_cast<size_t>(channel)];
        auto gain = currentLevelGainLinear;

        for (int i = 0; i < numSamples; ++i)
        {
            auto sample = samples[i];
            for (size_t band = 0; band < channelFilters.size(); ++band)
                sample = channelFilters[band].process(sample, coefficients[band]);

            // One-pole smoothing keeps level-delta changes click-free.
            gain += (targetLevelGainLinear - gain) * 0.002f;
            samples[i] = sample * gain;
        }

        if (channel == channels - 1)
            currentLevelGainLinear = gain;
    }
}
}
