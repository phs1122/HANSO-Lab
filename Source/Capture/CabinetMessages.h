#pragma once

#include "App/Utf8.h"

namespace hanso
{
inline juce::String cabinetExportRequiresRealSourceMessage()
{
    return utf8("캐비넷을 내보내려면 최소 1개 이상의 마이크 위치를 캡쳐하거나 IR로 가져와야 합니다.");
}
}
