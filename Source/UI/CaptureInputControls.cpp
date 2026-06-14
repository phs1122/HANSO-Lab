#include "UI/CaptureInputControls.h"

namespace hanso
{
CaptureInputControls::CaptureInputControls(ChangeHandler onChange)
    : onSettingsChange(std::move(onChange))
{
    captureInputLabel.setText("Input pair", juce::dontSendNotification);
    captureModeLabel.setText("Capture mode", juce::dontSendNotification);

    populateItems();

    captureInputBox.setSelectedId(1, juce::dontSendNotification);
    captureModeBox.setSelectedId(2, juce::dontSendNotification);

    captureInputBox.onChange = [this] { notifyChange(); };
    captureModeBox.onChange = [this] { notifyChange(); };

    addAndMakeVisible(captureInputLabel);
    addAndMakeVisible(captureInputBox);
    addAndMakeVisible(captureModeLabel);
    addAndMakeVisible(captureModeBox);
}

void CaptureInputControls::setModeLabelText(const juce::String& text)
{
    captureModeLabel.setText(text, juce::dontSendNotification);
}

void CaptureInputControls::syncFromEngine(int inputChannel, int channelCount)
{
    suppressCallbacks = true;
    captureInputBox.setSelectedId(startChannelToPairId(inputChannel), juce::dontSendNotification);
    captureModeBox.setSelectedId(channelCountToModeId(channelCount), juce::dontSendNotification);
    suppressCallbacks = false;
}

void CaptureInputControls::layoutRows(juce::Rectangle<int> area, bool singleRowLayout)
{
    if (singleRowLayout)
    {
        captureInputLabel.setBounds(area.removeFromLeft(100));
        captureInputBox.setBounds(area.removeFromLeft(180));
        area.removeFromLeft(18);
        captureModeLabel.setBounds(area.removeFromLeft(55));
        captureModeBox.setBounds(area.removeFromLeft(150));
        return;
    }

    auto row = area.removeFromTop(32);
    captureInputLabel.setBounds(row.removeFromLeft(120));
    captureInputBox.setBounds(row.removeFromLeft(140));
    area.removeFromTop(10);
    row = area.removeFromTop(32);
    captureModeLabel.setBounds(row.removeFromLeft(120));
    captureModeBox.setBounds(row.removeFromLeft(140));
}

int CaptureInputControls::pairIdToStartChannel(int selectedId) noexcept
{
    return selectedId == 1 ? -1 : (selectedId - 2) * 2;
}

int CaptureInputControls::startChannelToPairId(int startChannel) noexcept
{
    return startChannel < 0 ? 1 : (startChannel / 2) + 2;
}

int CaptureInputControls::modeIdToChannelCount(int selectedId) noexcept
{
    return selectedId == 2 ? 2 : 1;
}

int CaptureInputControls::channelCountToModeId(int channelCount) noexcept
{
    return channelCount == 2 ? 2 : 1;
}

void CaptureInputControls::notifyChange()
{
    if (suppressCallbacks || onSettingsChange == nullptr)
        return;

    onSettingsChange(pairIdToStartChannel(captureInputBox.getSelectedId()),
                     modeIdToChannelCount(captureModeBox.getSelectedId()));
}

void CaptureInputControls::populateItems()
{
    captureInputBox.clear(juce::dontSendNotification);
    captureInputBox.addItem("Auto strongest pair", 1);
    captureInputBox.addItem("Input 1 + 2", 2);
    captureInputBox.addItem("Input 3 + 4", 3);
    captureInputBox.addItem("Input 5 + 6", 4);
    captureInputBox.addItem("Input 7 + 8", 5);

    captureModeBox.clear(juce::dontSendNotification);
    captureModeBox.addItem("Mono", 1);
    captureModeBox.addItem("Stereo pair", 2);
}
}
