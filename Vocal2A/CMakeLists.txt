cmake_minimum_required(VERSION 3.22)

project(Vocal2A VERSION 0.1.0)

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

juce_add_plugin(Vocal2A
    COMPANY_NAME            "VocalEssential"
    PLUGIN_MANUFACTURER_CODE Vgls
    PLUGIN_CODE             V2al
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "Vocal2A"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

target_sources(Vocal2A
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_compile_definitions(Vocal2A
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Vocal2A
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# --- optional bundled UI font (drop SFProDisplay.ttf into <repo>/fonts/) ---
set(VG_FONTS_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/../fonts")
set(VG_FONT_REG   "${VG_FONTS_DIR}/SFProDisplay.ttf")
set(VG_FONT_BOLD  "${VG_FONTS_DIR}/SFProDisplayBold.ttf")
if(EXISTS "${VG_FONT_REG}")
    set(VG_FONT_SOURCES "${VG_FONT_REG}")
    if(EXISTS "${VG_FONT_BOLD}")
        list(APPEND VG_FONT_SOURCES "${VG_FONT_BOLD}")
    endif()
    juce_add_binary_data(Vocal2AFonts SOURCES ${VG_FONT_SOURCES})
    target_link_libraries(Vocal2A PRIVATE Vocal2AFonts)
    target_compile_definitions(Vocal2A PUBLIC VG_HAS_BUNDLED_FONT=1)
    if(EXISTS "${VG_FONT_BOLD}")
        target_compile_definitions(Vocal2A PUBLIC VG_HAS_BUNDLED_FONT_BOLD=1)
    endif()
endif()
