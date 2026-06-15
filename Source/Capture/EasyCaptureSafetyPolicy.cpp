#include "Capture/EasyCaptureSafetyPolicy.h"

#include <algorithm>

namespace hanso
{
float EasyCaptureSafetyPolicy::limitAmplitude(float value) const noexcept
{
    if (! hardOutputLimitEnabled)
        return value;

    return std::clamp(value, 0.0f, captureAmplitude);
}
}
