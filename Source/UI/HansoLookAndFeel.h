#pragma once

#include <JuceHeader.h>

namespace hanso
{
// App-wide look derived from the HANSO Lab UX prototype (docs/wireframes).
// Header-only so no CMake source list change is required.
class HansoLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    HansoLookAndFeel()
    {
        const auto cBg       = juce::Colour::fromRGB(24, 25, 27);
        const auto cPanel    = juce::Colour::fromRGB(31, 33, 36);
        const auto cPanel2   = juce::Colour::fromRGB(38, 40, 43);
        const auto cBorder   = juce::Colour::fromRGB(58, 62, 66);
        const auto cText     = juce::Colour::fromRGB(233, 234, 236);
        const auto cTextDim  = juce::Colour::fromRGB(154, 160, 166);
        const auto cBlue     = juce::Colour::fromRGB(73, 151, 191);   // meters / accents
        const auto cBlueBtn  = juce::Colour::fromRGB(53, 111, 176);   // primary / selected
        const auto cMeterTrk = juce::Colour::fromRGB(20, 27, 31);

        setColour(juce::ResizableWindow::backgroundColourId, cBg);
        setColour(juce::TabbedComponent::backgroundColourId, cBg);
        setColour(juce::TabbedButtonBar::tabTextColourId, juce::Colours::white);

        setColour(juce::Label::textColourId, cText);

        setColour(juce::TextButton::buttonColourId, cPanel2);
        setColour(juce::TextButton::buttonOnColourId, cBlueBtn);
        setColour(juce::TextButton::textColourOffId, cText);
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);

        setColour(juce::ComboBox::backgroundColourId, cPanel);
        setColour(juce::ComboBox::textColourId, cText);
        setColour(juce::ComboBox::outlineColourId, cBorder);
        setColour(juce::ComboBox::arrowColourId, cTextDim);
        setColour(juce::ComboBox::buttonColourId, cPanel2);

        setColour(juce::PopupMenu::backgroundColourId, cPanel);
        setColour(juce::PopupMenu::textColourId, cText);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, cBlueBtn);
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

        setColour(juce::TextEditor::backgroundColourId, cPanel);
        setColour(juce::TextEditor::textColourId, cText);
        setColour(juce::TextEditor::outlineColourId, cBorder);
        setColour(juce::TextEditor::focusedOutlineColourId, cBlueBtn);

        setColour(juce::Slider::thumbColourId, cBlue);
        setColour(juce::Slider::trackColourId, cBlueBtn);
        setColour(juce::Slider::backgroundColourId, cMeterTrk);
        setColour(juce::Slider::textBoxTextColourId, cText);
        setColour(juce::Slider::textBoxBackgroundColourId, cPanel);
        setColour(juce::Slider::textBoxOutlineColourId, cBorder);

        setColour(juce::ScrollBar::thumbColourId, cBorder);
        setColour(juce::ToggleButton::textColourId, cText);
        setColour(juce::ToggleButton::tickColourId, cBlue);
    }

    // Flat, rounded button surface (prototype .btn: 8px radius, subtle border).
    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        constexpr float radius = 8.0f;
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);

        auto fill = backgroundColour;
        if (! button.isEnabled())
            fill = fill.withAlpha(0.45f);
        else if (shouldDrawButtonAsDown)
            fill = fill.darker(0.18f);
        else if (shouldDrawButtonAsHighlighted)
            fill = fill.brighter(0.14f);

        g.setColour(fill);
        g.fillRoundedRectangle(bounds, radius);

        g.setColour(juce::Colour::fromRGB(58, 62, 66).withAlpha(button.isEnabled() ? 0.55f : 0.25f));
        g.drawRoundedRectangle(bounds, radius, 1.0f);
    }

    // App-wide readability bump: small fonts (<=16pt) grow by +2.5; larger
    // titles (>=~20pt) are left alone. Applies to every Label/Button/ComboBox/
    // Popup font, including default (unset) ones.
    static juce::Font bumpFont(juce::Font f)
    {
        const auto h = f.getHeight();
        return h <= 16.0f ? f.withHeight(h + 2.5f) : f;
    }

    juce::Font getLabelFont(juce::Label& label) override
    {
        return bumpFont(juce::LookAndFeel_V4::getLabelFont(label));
    }

    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override
    {
        return bumpFont(juce::LookAndFeel_V4::getTextButtonFont(button, buttonHeight));
    }

    juce::Font getComboBoxFont(juce::ComboBox& box) override
    {
        return bumpFont(juce::LookAndFeel_V4::getComboBoxFont(box));
    }

    juce::Font getPopupMenuFont() override
    {
        return bumpFont(juce::LookAndFeel_V4::getPopupMenuFont());
    }
};
}
