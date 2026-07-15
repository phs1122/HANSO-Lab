#pragma once

#include <JuceHeader.h>

#include "Capture/CabinetCaptureState.h"

namespace hanso
{
inline constexpr float cabinetCaptureReferenceDistanceCm = 2.0f;
inline constexpr float cabinetCaptureOffAxisDegrees = 30.0f;

struct CabinetMicPositionDef
{
    const char* id;
    const char* label;
    float normalizedPosition;
    const char* instruction;
};

inline constexpr CabinetMicPositionDef kCabinetMicPositions[] =
{
    // These IDs are retained for .hanso chunk compatibility. The display
    // terminology is the radial scheme used by the preview and mic matrix.
    { "cab-center", "Cone", 0.0f,
      "그릴 천에서 마이크 캡슐 중심까지 2 cm를 띄우세요. 더스트캡 가장자리와 콘 안쪽을 향해 수직으로 둡니다.\n다른 위치도 같은 거리를 유지해야 위치 비교가 정확합니다." },
    { "cab-edge", "Cone Edge", 0.33f,
      "그릴 천에서 마이크 캡슐 중심까지 2 cm를 유지하세요. 더스트캡과 콘이 만나는 경계를 향해 수직으로 둡니다.\n거리와 프리앰프 게인은 이전 슬롯과 동일하게 유지합니다." },
    { "cab-cone", "Edge", 0.66f,
      "그릴 천에서 마이크 캡슐 중심까지 2 cm를 유지하세요. 스피커 콘의 바깥쪽 Edge 영역을 향해 수직으로 둡니다.\n거리와 프리앰프 게인은 이전 슬롯과 동일하게 유지합니다." },
    { "cab-off-axis", "Off-Axis", 1.0f,
      "캡슐 중심 위치와 그릴 기준 2 cm 거리를 유지한 채 마이크를 30도로 돌리세요. 캡슐이 Edge 기준점을 계속 향해야 합니다.\n거리와 프리앰프 게인은 이전 슬롯과 동일하게 유지합니다." },
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
    return "cabinet/positions/" + stepId + "/ir.f32";
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
