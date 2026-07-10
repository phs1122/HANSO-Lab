#include "UI/PreviewChainStrip.h"

namespace hanso
{
namespace
{
constexpr float arrowWidth = 18.0f;
constexpr float ioWidth = 56.0f;

juce::String keyFor(const std::vector<PreviewChainStrip::Block>& blocks)
{
    juce::String key;
    for (const auto& block : blocks)
        key << block.id << "|" << block.title << "|" << block.subtitle << "|"
            << static_cast<int>(block.kind) << "|" << (block.resettable ? 1 : 0)
            << (block.expandable ? 1 : 0) << ";";
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

std::vector<PreviewChainStrip::BlockLayout> PreviewChainStrip::computeLayout() const
{
    std::vector<BlockLayout> layouts;
    if (blocks.empty())
        return layouts;

    const auto area = getLocalBounds().toFloat();

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

        BlockLayout layout;
        layout.blockIndex = i;
        layout.bounds = juce::Rectangle<float>(x, area.getY(), width, area.getHeight()).reduced(0.0f, 2.0f);
        if (block.resettable)
            layout.resetBounds = layout.bounds.withTrimmedLeft(layout.bounds.getWidth() - 16.0f)
                                              .withHeight(16.0f)
                                              .reduced(2.0f);
        layouts.push_back(layout);

        x += width;
        if (i + 1 < blocks.size())
            x += arrowWidth;
    }

    return layouts;
}

void PreviewChainStrip::paint(juce::Graphics& g)
{
    const auto layouts = computeLayout();

    for (const auto& layout : layouts)
    {
        const auto& block = blocks[layout.blockIndex];
        const auto bounds = layout.bounds;

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
        auto title = block.title;
        if (block.expandable)
            title << juce::String::fromUTF8(" ▾");
        g.drawText(title, textArea.removeFromTop(18.0f), juce::Justification::centred, true);
        if (block.subtitle.isNotEmpty())
        {
            g.setColour(subtitleColour);
            g.setFont(juce::Font(11.5f));
            g.drawText(block.subtitle, textArea, juce::Justification::centredTop, true);
        }

        if (! layout.resetBounds.isEmpty())
        {
            g.setColour(subtitleColour);
            g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.drawText(juce::String::fromUTF8("✕"), layout.resetBounds, juce::Justification::centred, false);
        }
    }

    g.setColour(juce::Colour::fromRGB(120, 122, 126));
    for (size_t i = 0; i + 1 < layouts.size(); ++i)
    {
        const auto arrowY = layouts[i].bounds.getCentreY();
        const auto startX = layouts[i].bounds.getRight() + 4.0f;
        const auto endX = layouts[i + 1].bounds.getX() - 5.0f;
        g.drawLine(startX, arrowY, endX, arrowY, 1.2f);
        juce::Path head;
        head.addTriangle(endX, arrowY - 3.5f, endX, arrowY + 3.5f, endX + 4.0f, arrowY);
        g.fillPath(head);
    }
}

void PreviewChainStrip::mouseUp(const juce::MouseEvent& event)
{
    const auto position = event.position;
    for (const auto& layout : computeLayout())
    {
        const auto& block = blocks[layout.blockIndex];
        if (! layout.resetBounds.isEmpty() && layout.resetBounds.expanded(2.0f).contains(position))
        {
            if (onBlockReset)
                onBlockReset(block.id);
            return;
        }

        if (layout.bounds.contains(position))
        {
            if (onBlockClicked)
                onBlockClicked(block.id);
            return;
        }
    }
}

void PreviewChainStrip::mouseMove(const juce::MouseEvent& event)
{
    for (const auto& layout : computeLayout())
    {
        const auto& block = blocks[layout.blockIndex];
        const auto interactive = block.expandable || ! layout.resetBounds.isEmpty();
        if (layout.bounds.contains(event.position) && interactive)
        {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
            return;
        }
    }

    setMouseCursor(juce::MouseCursor::NormalCursor);
}
}
