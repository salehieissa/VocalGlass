cmake_minimum_required(VERSION 3.22)

# Build a universal binary on macOS (Apple Silicon + Intel) so any host can load
# the plugin regardless of how it's launched (e.g. FL Studio under Rosetta).
# Ignored on non-Apple platforms. Override with -DCMAKE_OSX_ARCHITECTURES=...
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS universal binary")
endif()

project(VocalChop VERSION 0.1.0)

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

juce_add_plugin(VocalChop
    COMPANY_NAME            "VocalEssential"
    PLUGIN_MANUFACTURER_CODE Vgls
    PLUGIN_CODE             Vchp
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "VocalChop"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

target_sources(VocalChop
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        ../common/Licensing/LicenseManager.cpp
)

target_compile_definitions(VocalChop
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(VocalChop
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# --- optional photoreal UI skin (assets/ui) ---
# Embed whichever candidate assets actually exist; missing ones are skipped and
# the code falls back to vector drawing for those pieces.
set(VG_SKIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/ui")
set(VG_SKIN_CANDIDATES
    "chop-chassis@2x.png" "chop-chassis-on@2x.png"
    "grit-knob-large@2x.png" "grit-knob-small@2x.png")
set(VG_SKIN_SOURCES "")
foreach(name ${VG_SKIN_CANDIDATES})
    if(EXISTS "${VG_SKIN_DIR}/${name}")
        list(APPEND VG_SKIN_SOURCES "${VG_SKIN_DIR}/${name}")
    endif()
endforeach()
if(VG_SKIN_SOURCES)
    juce_add_binary_data(VocalChopSkin
        NAMESPACE   SkinBinary
        HEADER_NAME SkinBinary.h
        SOURCES     ${VG_SKIN_SOURCES})
    target_link_libraries(VocalChop PRIVATE VocalChopSkin)
    target_compile_definitions(VocalChop PUBLIC VG_HAS_SKIN=1)
endif()

# --- optional bundled UI font (drop SFProDisplay.ttf into <repo>/fonts/) ---
set(VG_FONTS_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/../fonts")
set(VG_FONT_REG   "${VG_FONTS_DIR}/SFProDisplay.ttf")
set(VG_FONT_BOLD  "${VG_FONTS_DIR}/SFProDisplayBold.ttf")
if(EXISTS "${VG_FONT_REG}")
    set(VG_FONT_SOURCES "${VG_FONT_REG}")
    if(EXISTS "${VG_FONT_BOLD}")
        list(APPEND VG_FONT_SOURCES "${VG_FONT_BOLD}")
    endif()
    juce_add_binary_data(VocalChopFonts SOURCES ${VG_FONT_SOURCES})
    target_link_libraries(VocalChop PRIVATE VocalChopFonts)
    target_compile_definitions(VocalChop PUBLIC VG_HAS_BUNDLED_FONT=1)
    if(EXISTS "${VG_FONT_BOLD}")
        target_compile_definitions(VocalChop PUBLIC VG_HAS_BUNDLED_FONT_BOLD=1)
    endif()
endif()
