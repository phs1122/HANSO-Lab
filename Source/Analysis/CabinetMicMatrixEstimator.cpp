#include "Analysis/CabinetMicMatrixEstimator.h"

#include "Analysis/MicColorationProfiles.h"
#include "Capture/CabinetMicPositions.h"

namespace hanso
{
namespace
{
constexpr CabinetMicClass kMatrixMicClasses[] = {
    CabinetMicClass::Dynamic,
    CabinetMicClass::Ribbon,
    CabinetMicClass::Condenser,
};

CabinetMicClass effectiveMicClass(CabinetMicClass micClass) noexcept
{
    return micClass == CabinetMicClass::Unknown ? CabinetMicClass::Dynamic : micClass;
}

float bandMean(const CabinetToneBandArray& bands) noexcept
{
    auto sum = 0.0f;
    for (const auto value : bands)
        sum += value;
    return sum / static_cast<float>(bands.size());
}
}

juce::var CabinetMicMatrix::toVar() const
{
    if (! valid)
        return {};

    auto object = new juce::DynamicObject();
    object->setProperty("status", "computed");

    juce::Array<juce::var> entryVars;
    for (const auto& entry : entries)
    {
        auto entryObject = new juce::DynamicObject();
        entryObject->setProperty("micClass", toString(entry.micClass));
        entryObject->setProperty("positionId", entry.positionId);
        entryObject->setProperty("source", entry.measured ? "measured" : "estimated");
        entryObject->setProperty("estimatedFrom",
                                 entry.estimatedFrom.isNotEmpty()
                                     ? juce::var(entry.estimatedFrom)
                                     : juce::var());
        entryObject->setProperty("toneProfile", entry.toneProfile.toVar());
        entryVars.add(entryObject);
    }

    object->setProperty("entries", entryVars);
    return object;
}

CabinetMicMatrix CabinetMicMatrixEstimator::estimate(const std::vector<CabinetMicPositionSlot>& slots)
{
    CabinetMicMatrix matrix;

    // Recover one neutral cabinet estimate per real slot, then average. Tone
    // profile band gains are stored relative to their own mean, so the mean
    // level information lives in levelDb; the de-embedded curves keep that
    // convention by re-normalising at the end.
    struct NeutralEstimate
    {
        CabinetToneBandArray bands {};
        float levelDb { 0.0f };
    };

    std::vector<NeutralEstimate> neutrals;
    juce::StringArray sourceIds;

    for (const auto& slot : slots)
    {
        if (! slot.hasRealSource() || ! slot.toneProfile.valid)
            continue;

        const auto positionIndex = positionIndexForCabinetId(slot.id);
        if (positionIndex < 0)
            continue;

        const auto micBands = MicColorationProfiles::micColorationBandsDb(effectiveMicClass(slot.micClass));
        const auto positionBands = MicColorationProfiles::positionColorationBandsDb(positionIndex);

        NeutralEstimate neutral;
        for (size_t band = 0; band < neutral.bands.size(); ++band)
            neutral.bands[band] = slot.toneProfile.bandGainsDb[band] - micBands[band] - positionBands[band];
        neutral.levelDb = slot.toneProfile.levelDb
                          - MicColorationProfiles::positionGainTrimDb(positionIndex);

        neutrals.push_back(neutral);
        sourceIds.add(slot.id);
    }

    if (neutrals.empty())
        return matrix;

    NeutralEstimate averaged;
    for (const auto& neutral : neutrals)
    {
        for (size_t band = 0; band < averaged.bands.size(); ++band)
            averaged.bands[band] += neutral.bands[band] / static_cast<float>(neutrals.size());
        averaged.levelDb += neutral.levelDb / static_cast<float>(neutrals.size());
    }

    const auto estimatedFrom = sourceIds.joinIntoString(",");

    for (const auto micClass : kMatrixMicClasses)
    {
        const auto micBands = MicColorationProfiles::micColorationBandsDb(micClass);

        for (int positionIndex = 0; positionIndex < cabinetMicPositionCount(); ++positionIndex)
        {
            const auto& positionDef = kCabinetMicPositions[positionIndex];

            CabinetMicMatrixEntry entry;
            entry.micClass = micClass;
            entry.positionId = positionDef.id;

            // A real slot measured with this exact combination wins over the
            // estimate: keep the measured profile untouched.
            const CabinetMicPositionSlot* measuredSlot = nullptr;
            for (const auto& slot : slots)
                if (slot.id == positionDef.id && slot.hasRealSource() && slot.toneProfile.valid
                    && effectiveMicClass(slot.micClass) == micClass)
                    measuredSlot = &slot;

            if (measuredSlot != nullptr)
            {
                entry.measured = true;
                entry.toneProfile = measuredSlot->toneProfile;
            }
            else
            {
                const auto positionBands = MicColorationProfiles::positionColorationBandsDb(positionIndex);

                CabinetToneBandArray raw {};
                for (size_t band = 0; band < raw.size(); ++band)
                    raw[band] = averaged.bands[band] + micBands[band] + positionBands[band];

                const auto mean = bandMean(raw);
                entry.estimatedFrom = estimatedFrom;
                entry.toneProfile.valid = true;
                entry.toneProfile.estimated = true;
                entry.toneProfile.levelDb = averaged.levelDb + mean
                                            + MicColorationProfiles::positionGainTrimDb(positionIndex);
                for (size_t band = 0; band < raw.size(); ++band)
                    entry.toneProfile.bandGainsDb[band] = juce::jlimit(-36.0f, 36.0f, raw[band] - mean);
            }

            matrix.entries.push_back(std::move(entry));
        }
    }

    matrix.valid = true;
    return matrix;
}
}
