cmake_minimum_required(VERSION 3.22)

project(VocalGrit VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Pull in JUCE automatically so you don't have to install it separately.
# The first configure will download it (needs internet); after that it's cached.
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4          # pinned version for reproducible builds
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# The plugin target. juce_add_plugin builds VST3 / AU / Standalone formats.
# ---------------------------------------------------------------------------
juce_add_plugin(VocalGrit
    COMPANY_NAME            "VocalEssential"
    PLUGIN_MANUFACTURER_CODE Vgls            # 4 chars, first uppercase
    PLUGIN_CODE             Vgrt             # 4 chars, unique per plugin
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "VocalGrit"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE             # auto-installs to your plugin folder
)

target_sources(VocalGrit
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

# JUCE_DISPLAY_SPLASH_SCREEN must stay TRUE unless you have a JUCE license.
target_compile_definitions(VocalGrit
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(VocalGrit
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
