cmake_minimum_required(VERSION 3.22)

project(HANSO_Lab VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(HANSO_JUCE_SOURCE_DIR "" CACHE PATH "Path to a JUCE source checkout, for example /path/to/JUCE-master")

if(HANSO_JUCE_SOURCE_DIR AND EXISTS "${HANSO_JUCE_SOURCE_DIR}/CMakeLists.txt")
    add_subdirectory("${HANSO_JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    find_package(JUCE CONFIG QUIET)

    if(NOT JUCE_FOUND)
        set(HANSO_JUCE_CANDIDATES
            "/Users/hanseo/Desktop/기타리스트 키우기/Raising-a-Guitarist-Guitar-VI/JUCE-master"
            "/Users/hanseo/Desktop/기타리스트 키우기/Guitar Plugins/JUCE-master")

        foreach(candidate IN LISTS HANSO_JUCE_CANDIDATES)
            if(EXISTS "${candidate}/CMakeLists.txt")
                message(STATUS "Using JUCE source checkout: ${candidate}")
                add_subdirectory("${candidate}" "${CMAKE_BINARY_DIR}/JUCE")
                break()
            endif()
        endforeach()
    endif()
endif()

if(NOT TARGET juce::juce_core)
    message(FATAL_ERROR "JUCE was not found. Set -DHANSO_JUCE_SOURCE_DIR=/path/to/JUCE-master or -DJUCE_DIR=/path/to/lib/cmake/JUCE.")
endif()

juce_add_gui_app(HANSO_Lab
    PRODUCT_NAME "HANSO Lab"
    COMPANY_NAME "HANSO"
    BUNDLE_ID "com.hanso.lab"
    VERSION "0.1.0"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "HANSO Lab needs microphone access to capture guitar hardware models.")

set(HANSO_LAB_APP_SOURCES
    Source/App/Main.cpp
    Source/App/MainComponent.cpp
    Source/App/ApplicationState.cpp
    Source/App/LabWorkflow.cpp
    Source/Audio/AudioDeviceController.cpp
    Source/Audio/AudioEngine.cpp
    Source/Audio/CaptureAudioSource.cpp
    Source/Audio/PreviewCabinetProcessor.cpp
    Source/Audio/PreviewCabinetIrProcessor.cpp
    Source/Audio/PreviewModelProcessor.cpp
    Source/Capture/CaptureEngine.cpp
    Source/Capture/CabinetIrImporter.cpp
    Source/Capture/CaptureRecipe.cpp
    Source/Capture/CaptureSession.cpp
    Source/Capture/CaptureWizardState.cpp
    Source/Capture/EasyCaptureSafetyPolicy.cpp
    Source/Capture/OutputRoutingPolicy.cpp
    Source/Capture/SignalAlignment.cpp
    Source/Capture/TestSignalGenerator.cpp
    Source/Analysis/AnalysisEngine.cpp
    Source/Analysis/CaptureQualityAnalyzer.cpp
    Source/Analysis/DynamicResponseAnalyzer.cpp
    Source/Analysis/FrequencyResponseAnalyzer.cpp
    Source/Analysis/ModelExtractionEngine.cpp
    Source/Analysis/ResidualDatasetBuilder.cpp
    Source/Analysis/TransferCurveAnalyzer.cpp
    Source/Model/HansoPackage.cpp
    Source/Preview/PreviewSampleLibrary.cpp
    Source/Serialization/HansoSerializer.cpp
    Source/Serialization/HansoAudioChunkCodec.cpp
    Source/Serialization/HansoModelCodec.cpp
    Source/UI/AssetPanel.cpp
    Source/UI/AudioSettingsPanel.cpp
    Source/UI/CaptureInputControls.cpp
    Source/UI/CapturePanel.cpp
    Source/UI/CaptureStatusIcon.cpp
    Source/UI/PreviewWaveformComponent.cpp
    Source/UI/TonePreviewPanel.cpp
    Source/UI/AnalysisPanel.cpp
    Source/UI/LogPanel.cpp
    Source/UI/LevelMeter.cpp)

target_sources(HANSO_Lab PRIVATE ${HANSO_LAB_APP_SOURCES})

target_include_directories(HANSO_Lab
    PRIVATE
        Source)

target_compile_definitions(HANSO_Lab
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:HANSO_Lab,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:HANSO_Lab,JUCE_VERSION>")

target_link_libraries(HANSO_Lab
    PRIVATE
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
