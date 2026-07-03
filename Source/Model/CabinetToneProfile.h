#pragma once

#include <JuceHeader.h>

#include <array>

namespace hanso
{
// Lightweight tonal fingerprint of a cabinet mic position: per-band gains
// relative to the position's own average level. This is the payload consumers
// use when they render mic-position tone changes with EQ instead of running
// full IR convolution. Real IR chunks remain available for high-quality paths.
struct CabinetToneBandDef
{
    double lowHz;
    double highHz;
};

inline constexpr CabinetToneBandDef kCabinetToneBands[] =
{
    { 80.0, 160.0 },
    { 160.0, 350.0 },
    { 350.0, 800.0 },
    { 800.0, 1800.0 },
    { 1800.0, 4000.0 },
    { 4000.0, 9000.0 },
};

inline constexpr int cabinetToneBandCount = static_cast<int>(sizeof(kCabinetToneBands) / sizeof(kCabinetToneBands[0]));

struct CabinetToneProfile
{
    bool valid { false };
    bool estimated { false };
    float levelDb { 0.0f };
    std::array<float, static_cast<size_t>(cabinetToneBandCount)> bandGainsDb {};

    juce::var toVar() const
    {
        if (! valid)
            return {};

        auto object = new juce::DynamicObject();
        object->setProperty("estimated", estimated);
        object->setProperty("levelDb", levelDb);

        juce::Array<juce::var> bands;
        for (int band = 0; band < cabinetToneBandCount; ++band)
        {
            auto entry = new juce::DynamicObject();
            entry->setProperty("lowHz", kCabinetToneBands[band].lowHz);
            entry->setProperty("highHz", kCabinetToneBands[band].highHz);
            entry->setProperty("gainDb", bandGainsDb[static_cast<size_t>(band)]);
            bands.add(entry);
        }

        object->setProperty("bands", bands);
        return object;
    }

    static CabinetToneProfile fromVar(const juce::var& value)
    {
        CabinetToneProfile profile;
        const auto* object = value.getDynamicObject();
        if (object == nullptr)
            return profile;

        profile.estimated = static_cast<bool>(object->getProperty("estimated"));
        profile.levelDb = static_cast<float>(static_cast<double>(object->getProperty("levelDb")));

        const auto bands = object->getProperty("bands");
        const auto* bandArray = bands.getArray();
        if (bandArray == nullptr || bandArray->size() != cabinetToneBandCount)
            return profile;

        for (int band = 0; band < cabinetToneBandCount; ++band)
        {
            const auto* entry = bandArray->getReference(band).getDynamicObject();
            if (entry == nullptr)
                return profile;

            profile.bandGainsDb[static_cast<size_t>(band)] =
                static_cast<float>(static_cast<double>(entry->getProperty("gainDb")));
        }

        profile.valid = true;
        return profile;
    }

    static CabinetToneProfile interpolate(const CabinetToneProfile& a, const CabinetToneProfile& b, float t)
    {
        if (! a.valid)
            return b;
        if (! b.valid)
            return a;

        const auto mix = juce::jlimit(0.0f, 1.0f, t);
        CabinetToneProfile profile;
        profile.valid = true;
        profile.estimated = true;
        profile.levelDb = a.levelDb + (b.levelDb - a.levelDb) * mix;
        for (size_t band = 0; band < profile.bandGainsDb.size(); ++band)
            profile.bandGainsDb[band] = a.bandGainsDb[band] + (b.bandGainsDb[band] - a.bandGainsDb[band]) * mix;

        return profile;
    }
};
}
