#include "UI/PreviewChainStrip.h"

namespace hanso
{
namespace
{
juce::String keyFor(const std::vector<PreviewChainStrip::Block>& blocks)
{
    juce::String key;
    for (const auto& block : blocks)
        key << block.title << "|" << block.subtitle << "|" << static_cast<int>(block.kind) << ";";
    return key;
}
}

void PreviewChainStrip::setBlocks(std::vector<Block> newBlocks)
{
    auto newKey = keyFor(newBlocks);
    if (newKey == layoutKey)
        return;

    blocks = std::move(newBlocks);
    layoutKey = std::move(newKey);
    repaint();
}

void PreviewChainStrip::paint(juce::Graphics& g)
{
    if (blocks.empty())
        return;

    const auto area = getLocalBounds().toFloat();
    constexpr float arrowWidth = 18.0f;
    constexpr float ioWidth = 56.0f;

    auto ioCount = 0;
    for (const auto& block : blocks)
        if (block.kind == BlockKind::Io)
            ++ioCount;

    const auto arrowTotal = arrowWidth * static_cast<float>(blocks.size() - 1);
    const auto flexCount = static_cast<float>(blocks.size() - static_cast<size_t>(ioCount));
    const auto flexWidth = flexCount > 0.0f
                         ? (area.getWidth() - arrowTotal - ioWidth * static_cast<float>(ioCount)) / flexCount
                         : ioWidth;

    auto x = area.getX();
    for (size_t i = 0; i < blocks.size(); ++i)
    {
        const auto& block = blocks[i];
        const auto width = block.kind == BlockKind::Io ? ioWidth : juce::jmax(72.0f, flexWidth);
        auto bounds = juce::Rectangle<float>(x, area.getY(), width, area.getHeight()).reduced(0.0f, 2.0f);

        if (block.kind == BlockKind::Package)
        {
            g.setColour(juce::Colour::fromRGB(24, 58, 46));
            g.fillRoundedRectangle(bounds, 7.0f);
            g.setColour(juce::Colour::fromRGB(93, 202, 165));
            g.drawRoundedRectangle(bounds, 7.0f, 1.4f);
        }
        else if (block.kind == BlockKind::Complement)
        {
            g.setColour(juce::Colour::fromRGB(120, 122, 126));
            juce::Path outline;
            outline.addRoundedRectangle(bounds, 7.0f);
            const float dashes[] = { 4.0f, 3.0f };
            juce::PathStrokeType stroke(1.2f);
            juce::Path dashed;
            stroke.createDashedStroke(dashed, outline, dashes, 2);
            g.fillPath(dashed);
        }
        else
        {
            g.setColour(juce::Colour::fromRGB(58, 60, 64));
            g.drawRoundedRectangle(bounds, 7.0f, 1.0f);
        }

        const auto titleColour = block.kind == BlockKind::Package
                               ? juce::Colour::fromRGB(159, 225, 203)
                               : juce::Colours::lightgrey;
        const auto subtitleColour = block.kind == BlockKind::Package
                                  ? juce::Colour::fromRGB(93, 202, 165)
                                  : juce::Colours::grey;

        auto textArea = bounds.reduced(6.0f, 5.0f);
        g.setColour(titleColour);
        g.setFont(juce::Font(13.5f, juce::Font::bold));
        g.drawText(block.title, textArea.removeFromTop(18.0f), juce::Justification::centred, true);
        if (block.subtitle.isNotEmpty())
        {
            g.setColour(subtitleColour);
            g.setFont(juce::Font(11.5f));
            g.drawText(block.subtitle, textArea, juce::Justification::centredTop, true);
        }

        x += width;
        if (i + 1 < blocks.size())
        {
            g.setColour(juce::Colour::fromRGB(120, 122, 126));
            const auto arrowY = bounds.getCentreY();
            const auto arrowEnd = x + arrowWidth - 5.0f;
            g.drawLine(x + 4.0f, arrowY, arrowEnd, arrowY, 1.2f);
            juce::Path head;
            head.addTriangle(arrowEnd, arrowY - 3.5f, arrowEnd, arrowY + 3.5f, arrowEnd + 4.0f, arrowY);
            g.fillPath(head);
            x += arrowWidth;
        }
    }
}
}
