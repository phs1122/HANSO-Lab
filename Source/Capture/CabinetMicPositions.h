#pragma once

#include <JuceHeader.h>

#include "Capture/CabinetCaptureState.h"

namespace hanso
{
struct CabinetMicPositionDef
{
    const char* id;
    const char* label;
    float normalizedPosition;
    const char* instruction;
};

inline constexpr CabinetMicPositionDef kCabinetMicPositions[] =
{
    { "cab-center", "Center", 0.0f,
      "마이크를 스피커 더스트캡 중심에 둔 위치입니다.\n직접 캡쳐하거나 외부 IR을 Import할 수 있습니다." },
    { "cab-edge", "Edge", 0.33f,
      "마이크를 더스트캡과 콘의 경계 근처에 둔 위치입니다.\n비워두면 Finish 단계에서 실제 source를 기반으로 추정됩니다." },
    { "cab-cone", "Cone", 0.66f,
      "마이크를 스피커 콘 영역 쪽으로 이동한 위치입니다.\n직접 캡쳐, IR Import, 또는 자동 추정을 사용할 수 있습니다." },
    { "cab-off-axis", "Off-Axis", 1.0f,
      "마이크를 축에서 살짝 벗어난 각도로 둔 위치입니다.\n직접 캡쳐하거나 IR을 Import할 수 있습니다." },
};

inline int cabinetMicPositionCount() noexcept
{
    return static_cast<int>(sizeof(kCabinetMicPositions) / sizeof(kCabinetMicPositions[0]));
}

inline int positionIndexForCabinetId(const juce::String& id) noexcept
{
    for (int index = 0; index < cabinetMicPositionCount(); ++index)
        if (id == kCabinetMicPositions[index].id)
            return index;

    return -1;
}

inline float normalizedPositionForCabinetId(const juce::String& id) noexcept
{
    for (const auto& position : kCabinetMicPositions)
        if (id == position.id)
            return position.normalizedPosition;

    return 0.5f;
}

inline juce::String impulseResponseChunkIdForCabinetPosition(const juce::String& stepId)
{
    return "cabinet/positions/" + stepId + "/ir.pcm16";
}

inline std::vector<CabinetMicPositionSlot> createDefaultCabinetSlots()
{
    std::vector<CabinetMicPositionSlot> slots;
    slots.reserve(static_cast<size_t>(cabinetMicPositionCount()));

    for (const auto& position : kCabinetMicPositions)
    {
        CabinetMicPositionSlot slot;
        slot.id = position.id;
        slot.label = position.label;
        slots.push_back(std::move(slot));
    }

    return slots;
}
}
