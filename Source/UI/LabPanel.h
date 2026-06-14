#pragma once

#include <JuceHeader.h>

namespace hanso
{
class LabPanel : public juce::Component
{
public:
    static constexpr int contentMargin = 24;

    void paint(juce::Graphics& g) override
    {
        g.fillAll(panelBackground());
        g.setColour(panelBorder());
        g.drawRect(getLocalBounds().reduced(1));
    }

    static void styleSectionTitle(juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    }

protected:
    static juce::Colour panelBackground() { return juce::Colour::fromRGB(31, 33, 36); }
    static juce::Colour panelBorder() { return juce::Colour::fromRGB(58, 62, 66); }
};
}
