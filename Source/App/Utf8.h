#pragma once

#include <JuceHeader.h>

namespace hanso
{
inline juce::String utf8(const char* text)
{
    return juce::String::fromUTF8(text);
}
}
