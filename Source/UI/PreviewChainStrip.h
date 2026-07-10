#pragma once

#include <JuceHeader.h>

#include <functional>
#include <vector>

namespace hanso
{
// Horizontal signal-chain visualization for the Tone Preview: which blocks
// the current preview renders and where each one comes from. Solid blocks are
// captured/imported packages, dashed blocks are clean standard equipment,
// plain blocks are fixed I/O. Blocks can be clickable (expand/collapse) and
// resettable (back to the default equipment).
class PreviewChainStrip final : public juce::Component
{
public:
    enum class BlockKind
    {
        Io,
        Package,
        Complement
    };

    struct Block
    {
        juce::String id;
        juce::String title;
        juce::String subtitle;
        BlockKind kind { BlockKind::Io };
        bool resettable { false };
        bool expandable { false };
    };

    std::function<void(const juce::String&)> onBlockClicked;
    std::function<void(const juce::String&)> onBlockReset;

    void setBlocks(std::vector<Block> newBlocks);
    void paint(juce::Graphics& g) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;

private:
    struct BlockLayout
    {
        juce::Rectangle<float> bounds;
        juce::Rectangle<float> resetBounds; // empty when not resettable
        size_t blockIndex { 0 };
    };

    std::vector<BlockLayout> computeLayout() const;

    std::vector<Block> blocks;
    juce::String layoutKey;
};
}
