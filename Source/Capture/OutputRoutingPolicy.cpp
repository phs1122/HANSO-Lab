#include "Capture/OutputRoutingPolicy.h"

namespace hanso
{
OutputRoutingMode OutputRoutingPolicy::forCaptureMode(CaptureMode mode) noexcept
{
    return mode == CaptureMode::Easy ? OutputRoutingMode::MonoLeftOnly : OutputRoutingMode::Default;
}

bool OutputRoutingPolicy::isRightChannelMuted(OutputRoutingMode mode) noexcept
{
    return mode == OutputRoutingMode::MonoLeftOnly;
}
}
