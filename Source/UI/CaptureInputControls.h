#pragma once

#include <JuceHeader.h>

namespace hanso
{
class CaptureInputControls final : public juce::Component
{
public:
    using ChangeHandler = std::function<void(int startChannel, int channelCount)>;

    explicit CaptureInputControls(ChangeHandler onChange);

    void setModeLabelText(const juce::String& text);
    void syncFromEngine(int inputChannel, int channelCount);
    void layoutRows(juce::Rectangle<int> area, bool singleRowLayout);

private:
    static int pairIdToStartChannel(int selectedId) noexcept;
    static int startChannelToPairId(int startChannel) noexcept;
    static int modeIdToChannelCount(int selectedId) noexcept;
    static int channelCountToModeId(int channelCount) noexcept;

    void notifyChange();
    void populateItems();

    ChangeHandler onSettingsChange;
    bool suppressCallbacks { false };

    juce::Label captureInputLabel;
    juce::ComboBox captureInputBox;
    juce::Label captureModeLabel;
    juce::ComboBox captureModeBox;
};
}
