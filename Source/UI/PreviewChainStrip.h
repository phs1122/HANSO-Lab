#pragma once

#include <JuceHeader.h>

#include <vector>

namespace hanso
{
// Horizontal signal-chain visualization for the Tone Preview: which blocks
// the current preview renders and where each one comes from. Solid blocks are
// the captured package, dashed blocks are preview-only complements, plain
// blocks are fixed I/O. Display only; block controls live in the panel.
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
        juce::String title;
        juce::String subtitle;
        BlockKind kind { BlockKind::Io };
    };

    void setBlocks(std::vector<Block> newBlocks);
    void paint(juce::Graphics& g) override;

private:
    std::vector<Block> blocks;
    juce::String layoutKey;
};
}
