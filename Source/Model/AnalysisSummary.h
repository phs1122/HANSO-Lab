#pragma once

#include <JuceHeader.h>

namespace hanso
{
struct AnalysisSummary
{
    float peakLevelDb { -120.0f };
    float rmsLevelDb { -120.0f };
    int estimatedLatencySamples { 0 };
    double estimatedLatencyMs { 0.0 };
    float lowBandDb { -120.0f };
    float midBandDb { -120.0f };
    float highBandDb { -120.0f };
    float estimatedGainDb { 0.0f };
    float dynamicRangeDb { 0.0f };
    float envelopeAttackMs { 0.0f };
    float envelopeReleaseMs { 0.0f };

    juce::var toVar() const
    {
        auto object = new juce::DynamicObject();
        object->setProperty("peakLevelDb", peakLevelDb);
        object->setProperty("rmsLevelDb", rmsLevelDb);
        object->setProperty("estimatedLatencySamples", estimatedLatencySamples);
        object->setProperty("estimatedLatencyMs", estimatedLatencyMs);
        object->setProperty("lowBandDb", lowBandDb);
        object->setProperty("midBandDb", midBandDb);
        object->setProperty("highBandDb", highBandDb);
        object->setProperty("estimatedGainDb", estimatedGainDb);
        object->setProperty("dynamicRangeDb", dynamicRangeDb);
        object->setProperty("envelopeAttackMs", envelopeAttackMs);
        object->setProperty("envelopeReleaseMs", envelopeReleaseMs);
        return object;
    }
};
}
