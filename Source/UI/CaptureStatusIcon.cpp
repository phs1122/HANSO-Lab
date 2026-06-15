#include "UI/CaptureStatusIcon.h"

namespace hanso
{
void CaptureStatusIcon::setStatus(CaptureStepStatus newStatus)
{
    status = newStatus;
    repaint();
}

void CaptureStatusIcon::setTooltipText(juce::String text)
{
    tooltipText = std::move(text);
}

juce::String CaptureStatusIcon::getTooltip()
{
    return tooltipText;
}

void CaptureStatusIcon::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    bounds = bounds.withSizeKeepingCentre(size, size).reduced(3.0f);
    const auto centre = bounds.getCentre();

    if (status == CaptureStepStatus::Warning)
    {
        juce::Path triangle;
        triangle.startNewSubPath(centre.x, bounds.getY());
        triangle.lineTo(bounds.getRight(), bounds.getBottom());
        triangle.lineTo(bounds.getX(), bounds.getBottom());
        triangle.closeSubPath();
        g.setColour(juce::Colours::orange);
        g.fillPath(triangle);
        g.setColour(juce::Colours::black);
        g.drawText("!", getLocalBounds(), juce::Justification::centred);
        return;
    }

    auto colour = juce::Colours::grey;
    if (status == CaptureStepStatus::Ready) colour = juce::Colours::lightblue;
    if (status == CaptureStepStatus::InProgress) colour = juce::Colours::dodgerblue;
    if (status == CaptureStepStatus::Passed) colour = juce::Colours::limegreen;
    if (status == CaptureStepStatus::Failed) colour = juce::Colours::red;

    g.setColour(colour);
    if (status == CaptureStepStatus::NotStarted)
        g.drawEllipse(bounds, 2.0f);
    else
        g.fillEllipse(bounds);

    if (status == CaptureStepStatus::Passed)
    {
        juce::Path check;
        check.startNewSubPath(bounds.getX() + bounds.getWidth() * 0.25f, centre.y);
        check.lineTo(bounds.getX() + bounds.getWidth() * 0.43f, bounds.getBottom() - bounds.getHeight() * 0.25f);
        check.lineTo(bounds.getRight() - bounds.getWidth() * 0.22f, bounds.getY() + bounds.getHeight() * 0.28f);
        g.setColour(juce::Colours::white);
        g.strokePath(check, juce::PathStrokeType(2.3f));
    }
    else if (status == CaptureStepStatus::Failed)
    {
        g.setColour(juce::Colours::white);
        g.drawLine(bounds.getX() + 6.0f, bounds.getY() + 6.0f, bounds.getRight() - 6.0f, bounds.getBottom() - 6.0f, 2.0f);
        g.drawLine(bounds.getRight() - 6.0f, bounds.getY() + 6.0f, bounds.getX() + 6.0f, bounds.getBottom() - 6.0f, 2.0f);
    }
}

void CaptureStatusIcon::mouseUp(const juce::MouseEvent&)
{
    if (onClick)
        onClick();
}
}
