#pragma once

namespace hanso
{
struct EasyCaptureSafetyPolicy
{
    float calibrationAmplitude { 0.016f };
    float captureAmplitude { 0.12f };
    bool hardOutputLimitEnabled { true };
    bool calibrationRequired { true };
    bool clippingCheckRequired { true };
    bool rightChannelMutedRequired { true };

    float limitAmplitude(float value) const noexcept;
};
}
