#include "Preview/PreviewSampleLibrary.h"

namespace hanso
{
juce::File PreviewSampleLibrary::samplesDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("HANSO Lab")
        .getChildFile("Preview Samples");
}

bool PreviewSampleLibrary::ensureSamplesDirectory(juce::String& error)
{
    auto directory = samplesDirectory();
    if (directory.isDirectory())
        return true;

    if (directory.createDirectory())
        return true;

    error = "Could not create Preview Samples folder: " + directory.getFullPathName();
    return false;
}

juce::Array<juce::File> PreviewSampleLibrary::listSampleFiles()
{
    juce::String error;
    juce::ignoreUnused(error);
    ensureSamplesDirectory(error);

    juce::Array<juce::File> files;
    samplesDirectory().findChildFiles(files,
                                      juce::File::findFiles,
                                      false,
                                      "*.wav;*.aif;*.aiff;*.flac");

    files.sort();
    return files;
}

juce::File PreviewSampleLibrary::importSampleFile(const juce::File& source, juce::String& error)
{
    if (! source.existsAsFile())
    {
        error = "Selected preview sample does not exist.";
        return {};
    }

    if (! isSupportedAudioFile(source))
    {
        error = "Unsupported preview sample format. Use WAV, AIFF, or FLAC.";
        return {};
    }

    if (! ensureSamplesDirectory(error))
        return {};

    auto destination = samplesDirectory().getChildFile(source.getFileName());
    if (destination.exists())
        destination = createUniqueDestination(destination);

    if (! source.copyFileTo(destination))
    {
        error = "Could not copy sample to Preview Samples folder.";
        return {};
    }

    return destination;
}

juce::File PreviewSampleLibrary::createUniqueDestination(const juce::File& requestedFile)
{
    const auto directory = requestedFile.getParentDirectory();
    const auto baseName = requestedFile.getFileNameWithoutExtension();
    const auto extension = requestedFile.getFileExtension();

    for (int i = 1; i < 10000; ++i)
    {
        auto candidate = directory.getChildFile(baseName + "_" + juce::String(i).paddedLeft('0', 2) + extension);
        if (! candidate.exists())
            return candidate;
    }

    return requestedFile.getNonexistentSibling();
}

bool PreviewSampleLibrary::isSupportedAudioFile(const juce::File& file)
{
    return file.hasFileExtension(".wav;.aif;.aiff;.flac");
}
}
