#include "UI/LevelMeter.h"

namespace hanso
{
void LevelMeter::setRange(float minimumDb, float maximumDb)
{
    minDb = minimumDb;
    maxDb = juce::jmax(minimumDb + 1.0f, maximumDb);
    repaint();
}

void LevelMeter::setTargetRange(float minimumDb, float maximumDb)
{
    targetMinDb = minimumDb;
    targetMaxDb = maximumDb;
    repaint();
}

void LevelMeter::setLevel(float levelDb)
{
    currentDb = levelDb;
    repaint();
}

float LevelMeter::normalized(float db) const noexcept
{
    return juce::jlimit(0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
}

void LevelMeter::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const auto radius = 4.0f;

    g.setColour(juce::Colour::fromRGB(20, 27, 31));
    g.fillRoundedRectangle(bounds, radius);

    const auto targetStart = bounds.getX() + bounds.getWidth() * normalized(targetMinDb);
    const auto targetEnd = bounds.getX() + bounds.getWidth() * normalized(targetMaxDb);
    g.setColour(juce::Colour::fromRGB(42, 82, 54));
    g.fillRoundedRectangle(juce::Rectangle<float>(targetStart,
                                                  bounds.getY(),
                                                  juce::jmax(2.0f, targetEnd - targetStart),
                                                  bounds.getHeight()),
                           radius);

    const auto fillWidth = bounds.getWidth() * normalized(currentDb);
    const auto inTarget = currentDb >= targetMinDb && currentDb <= targetMaxDb;
    const auto tooHot = currentDb > targetMaxDb;
    g.setColour(tooHot ? juce::Colour::fromRGB(210, 80, 58)
                       : (inTarget ? juce::Colour::fromRGB(80, 190, 118)
                                   : juce::Colour::fromRGB(73, 151, 191)));
    g.fillRoundedRectangle(bounds.withWidth(fillWidth), radius);

    const auto markerX = bounds.getX() + bounds.getWidth() * normalized(currentDb);
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.drawVerticalLine(static_cast<int>(markerX), bounds.getY(), bounds.getBottom());

    g.setColour(juce::Colour::fromRGB(92, 108, 116));
    g.drawRoundedRectangle(bounds, radius, 1.0f);
}
}
