#include "UI/LogPanel.h"

namespace hanso
{
LogPanel::LogPanel(ApplicationState& state)
    : appState(state)
{
    logView.setMultiLine(true);
    logView.setReadOnly(true);
    logView.setScrollbarsShown(true);
    addAndMakeVisible(logView);
}

void LogPanel::paint(juce::Graphics& g)
{
    g.fillAll(panelBackground());
}

void LogPanel::resized()
{
    logView.setBounds(getLocalBounds().reduced(12));
}

void LogPanel::refresh()
{
    const auto logs = appState.consumePendingLogs();
    for (const auto& line : logs)
    {
        logView.moveCaretToEnd();
        logView.insertTextAtCaret(line + "\n");
    }
}
}
