#pragma once

#include "Capture/CaptureMode.h"

namespace hanso
{
enum class OutputRoutingMode
{
    Default,
    MonoLeftOnly
};

struct OutputRoutingPolicy
{
    static OutputRoutingMode forCaptureMode(CaptureMode mode) noexcept;
    static bool isRightChannelMuted(OutputRoutingMode mode) noexcept;
};
}
