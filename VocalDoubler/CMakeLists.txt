cmake_minimum_required(VERSION 3.22)

# Build a universal binary on macOS (Apple Silicon + Intel) so any host can load
# the plugin regardless of how it's launched (e.g. FL Studio under Rosetta).
# Ignored on non-Apple platforms. Override with -DCMAKE_OSX_ARCHITECTURES=...
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS universal binary")
endif()

project(VocalDoubler VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(VocalDoubler
    COMPANY_NAME            "VocalEssential"
    PLUGIN_MANUFACTURER_CODE Vgls
    PLUGIN_CODE             Vdbl
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "VocalDoubler"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

target_sources(VocalDoubler
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        ../common/Licensing/LicenseManager.cpp
)

target_compile_definitions(VocalDoubler
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(VocalDoubler
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
