#include "Audio/PreviewMicColorProcessor.h"

#include "Capture/CabinetCaptureState.h"
#include "Capture/CabinetMicPositions.h"

#include <algorithm>
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

CabinetMicClass effectiveMicClass(CabinetMicClass micClass) noexcept
{
    return micClass == CabinetMicClass::Unknown ? CabinetMicClass::Dynamic : micClass;
}

bool isRealSource(const juce::String& source) noexcept
{
    const auto parsed = cabinetSlotSourceFromString(source);
    return parsed == CabinetSlotSource::CapturedIr || parsed == CabinetSlotSource::ImportedIr;
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
    profileDataLoaded.store(false);
    matrixLoaded.store(false);
    sourceProfiles.clear();
    positionProfiles.clear();
    for (auto& profiles : matrixProfiles)
        profiles.clear();
    targetClassIndex.store(-1);
    loadedClassIndex = -2;
    loadedPosition = -1.0f;
    profileCorrectionActive = false;
    targetLevelGainLinear = 1.0f;
    reset();
}

bool PreviewMicColorProcessor::loadFromPackage(const HansoPackage& package)
{
    clear();

    const auto* profileObject = package.cabinetProfile.getDynamicObject();
    if (profileObject == nullptr)
        return false;

    if (const auto* positionArray = profileObject->getProperty("positions").getArray())
    {
        for (const auto& entry : *positionArray)
        {
            const auto* positionObject = entry.getDynamicObject();
            if (positionObject == nullptr)
                continue;

            const auto positionIndex = positionIndexForCabinetId(positionObject->getProperty("id").toString());
            if (positionIndex < 0)
                continue;

            ProfileAnchor anchor;
            anchor.normalizedPosition = kCabinetMicPositions[positionIndex].normalizedPosition;
            anchor.profile = CabinetToneProfile::fromVar(positionObject->getProperty("toneProfile"));
            anchor.micClass = cabinetMicClassFromString(positionObject->getProperty("micClass").toString());
            if (! anchor.profile.valid)
                continue;

            positionProfiles.push_back(anchor);
            if (isRealSource(positionObject->getProperty("source").toString()))
                sourceProfiles.push_back(anchor);
        }
    }

    if (const auto* matrixObject = profileObject->getProperty("micMatrix").getDynamicObject())
    {
        if (const auto* entryArray = matrixObject->getProperty("entries").getArray())
        {
            for (const auto& entry : *entryArray)
            {
                const auto* entryObject = entry.getDynamicObject();
                if (entryObject == nullptr)
                    continue;

                const auto classIndex = classIndexFor(
                    cabinetMicClassFromString(entryObject->getProperty("micClass").toString()));
                const auto positionIndex = positionIndexForCabinetId(
                    entryObject->getProperty("positionId").toString());
                if (classIndex < 0 || positionIndex < 0)
                    continue;

                ProfileAnchor anchor;
                anchor.normalizedPosition = kCabinetMicPositions[positionIndex].normalizedPosition;
                anchor.profile = CabinetToneProfile::fromVar(entryObject->getProperty("toneProfile"));
                if (! anchor.profile.valid)
                    continue;

                anchor.micClass = cabinetMicClassFromString(entryObject->getProperty("micClass").toString());
                matrixProfiles[static_cast<size_t>(classIndex)].push_back(anchor);
            }
        }
    }

    const auto sortByPosition = [](auto& anchors)
    {
        std::sort(anchors.begin(), anchors.end(),
                  [](const auto& a, const auto& b)
                  {
                      return a.normalizedPosition < b.normalizedPosition;
                  });
    };

    sortByPosition(sourceProfiles);
    sortByPosition(positionProfiles);
    auto anyMatrixProfile = false;
    for (auto& profiles : matrixProfiles)
    {
        sortByPosition(profiles);
        anyMatrixProfile = anyMatrixProfile || ! profiles.empty();
    }

    matrixLoaded.store(anyMatrixProfile);
    const auto hasTargets = ! positionProfiles.empty() || anyMatrixProfile;
    profileDataLoaded.store(! sourceProfiles.empty() && hasTargets);
    return profileDataLoaded.load();
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

bool PreviewMicColorProcessor::interpolateProfile(const std::vector<ProfileAnchor>& anchors,
                                                   float normalizedPosition,
                                                   CabinetToneProfile& destination,
                                                   CabinetMicClass* nearestMicClass) noexcept
{
    destination = {};
    if (anchors.empty())
        return false;

    const auto clamped = juce::jlimit(0.0f, 1.0f, normalizedPosition);
    const auto& first = anchors.front();
    const auto& last = anchors.back();
    if (clamped <= first.normalizedPosition)
    {
        destination = first.profile;
        if (nearestMicClass != nullptr)
            *nearestMicClass = first.micClass;
        return destination.valid;
    }

    if (clamped >= last.normalizedPosition)
    {
        destination = last.profile;
        if (nearestMicClass != nullptr)
            *nearestMicClass = last.micClass;
        return destination.valid;
    }

    const ProfileAnchor* lower = &first;
    const ProfileAnchor* upper = &last;
    for (size_t i = 1; i < anchors.size(); ++i)
    {
        if (anchors[i].normalizedPosition >= clamped)
        {
            upper = &anchors[i];
            lower = &anchors[i - 1];
            break;
        }
    }

    const auto span = juce::jmax(0.0001f, upper->normalizedPosition - lower->normalizedPosition);
    const auto mix = juce::jlimit(0.0f, 1.0f, (clamped - lower->normalizedPosition) / span);
    destination = CabinetToneProfile::interpolate(lower->profile, upper->profile, mix);
    if (nearestMicClass != nullptr)
        *nearestMicClass = mix < 0.5f ? lower->micClass : upper->micClass;
    return destination.valid;
}

bool PreviewMicColorProcessor::targetProfileFor(float normalizedPosition,
                                                 CabinetMicClass sourceMicClass,
                                                 int requestedClassIndex,
                                                 CabinetToneProfile& destination) const noexcept
{
    auto classIndex = requestedClassIndex;
    if (classIndex < 0)
        classIndex = classIndexFor(effectiveMicClass(sourceMicClass));

    if (classIndex >= 0 && classIndex < classCount
        && interpolateProfile(matrixProfiles[static_cast<size_t>(classIndex)], normalizedPosition, destination))
        return true;

    return interpolateProfile(positionProfiles, normalizedPosition, destination);
}

void PreviewMicColorProcessor::rebuildCoefficientsIfNeeded() noexcept
{
    const auto requestedClass = targetClassIndex.load();
    const auto position = micPositionNormalized.load();
    if (requestedClass == loadedClassIndex && std::abs(position - loadedPosition) < positionEpsilon)
        return;

    CabinetToneProfile sourceProfile;
    CabinetMicClass sourceMicClass = CabinetMicClass::Unknown;
    CabinetToneProfile targetProfile;
    const auto haveSource = interpolateProfile(sourceProfiles, position, sourceProfile, &sourceMicClass);
    const auto haveTarget = haveSource && targetProfileFor(position, sourceMicClass, requestedClass, targetProfile);

    std::array<float, static_cast<size_t>(cabinetToneBandCount)> bandsDb {};
    auto levelDb = 0.0f;
    profileCorrectionActive = false;
    if (haveTarget)
    {
        for (size_t band = 0; band < bandsDb.size(); ++band)
        {
            bandsDb[band] = juce::jlimit(-previewDeltaLimitDb, previewDeltaLimitDb,
                                         targetProfile.bandGainsDb[band] - sourceProfile.bandGainsDb[band]);
            profileCorrectionActive = profileCorrectionActive || std::abs(bandsDb[band]) > 1.0e-5f;
        }
        levelDb = juce::jlimit(-previewDeltaLimitDb, previewDeltaLimitDb,
                                targetProfile.levelDb - sourceProfile.levelDb);
        profileCorrectionActive = profileCorrectionActive || std::abs(levelDb) > 1.0e-5f;
    }

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
    loadedClassIndex = requestedClass;
    loadedPosition = position;
}

void PreviewMicColorProcessor::process(juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    if (! profileDataLoaded.load() || numSamples <= 0)
        return;

    rebuildCoefficientsIfNeeded();
    if (! profileCorrectionActive)
        return;

    const auto channels = juce::jmin(juce::jmin(buffer.getNumChannels(), maxChannels), preparedChannels);
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

            // One-pole smoothing keeps profile changes click-free.
            gain += (targetLevelGainLinear - gain) * 0.002f;
            samples[i] = sample * gain;
        }

        if (channel == channels - 1)
            currentLevelGainLinear = gain;
    }
}
}
