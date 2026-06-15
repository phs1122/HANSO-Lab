#pragma once

#include <JuceHeader.h>

namespace hanso
{
class LevelMeter final : public juce::Component
{
public:
    void setRange(float minimumDb, float maximumDb);
    void setTargetRange(float minimumDb, float maximumDb);
    void setLevel(float levelDb);

    void paint(juce::Graphics& g) override;

private:
    float minDb { -60.0f };
    float maxDb { 0.0f };
    float targetMinDb { -36.0f };
    float targetMaxDb { -8.0f };
    float currentDb { -120.0f };

    float normalized(float db) const noexcept;
};
}
