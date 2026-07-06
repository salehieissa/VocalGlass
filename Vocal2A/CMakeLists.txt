cmake_minimum_required(VERSION 3.22)

# Build a universal binary on macOS (Apple Silicon + Intel) so any host can load
# the plugin regardless of how it's launched (e.g. FL Studio under Rosetta).
# Ignored on non-Apple platforms. Override with -DCMAKE_OSX_ARCHITECTURES=...
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS universal binary")
endif()

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
        ../common/Licensing/LicenseManager.cpp
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

# --- optional photoreal UI skin (assets/ui) ---
# Embed whichever candidate assets actually exist; missing ones are skipped and
# the code falls back to vector drawing for those pieces.
set(VG_SKIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/ui")
set(VG_SKIN_CANDIDATES
    "2a-vu-face@2x.png" "2a-vu-needle@2x.png"
    "knob@2x.png" "knob-large@2x.png" "knob-small@2x.png"
    "ring-glow@2x.png" "ring-glow-large@2x.png" "ring-glow-small@2x.png"
    "ring-off@2x.png" "ring-off-large@2x.png" "ring-off-small@2x.png"
    "ring-on@2x.png" "ring-on-large@2x.png" "ring-on-small@2x.png"
    "2a-chassis@2x.png" "2a-chassis-on@2x.png"
    "panel@2x.png"
    "faceplate@2x.png" "screw@2x.png" "nameplate@2x.png"
    "toggle-off@2x.png" "toggle-on@2x.png"
    "bat-off@2x.png" "bat-on@2x.png"
    "led-off@2x.png" "led-on@2x.png"
    "led-red-off@2x.png" "led-red-on@2x.png"
    "seg-off@2x.png" "seg-on@2x.png"
    "screen@2x.png" "texture@2x.png")
set(VG_SKIN_SOURCES "")
foreach(name ${VG_SKIN_CANDIDATES})
    if(EXISTS "${VG_SKIN_DIR}/${name}")
        list(APPEND VG_SKIN_SOURCES "${VG_SKIN_DIR}/${name}")
    endif()
endforeach()
if(VG_SKIN_SOURCES)
    juce_add_binary_data(Vocal2ASkin
        NAMESPACE   SkinBinary
        HEADER_NAME SkinBinary.h
        SOURCES     ${VG_SKIN_SOURCES})
    target_link_libraries(Vocal2A PRIVATE Vocal2ASkin)
    target_compile_definitions(Vocal2A PUBLIC VG_HAS_SKIN=1)
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
    juce_add_binary_data(Vocal2AFonts SOURCES ${VG_FONT_SOURCES})
    target_link_libraries(Vocal2A PRIVATE Vocal2AFonts)
    target_compile_definitions(Vocal2A PUBLIC VG_HAS_BUNDLED_FONT=1)
    if(EXISTS "${VG_FONT_BOLD}")
        target_compile_definitions(Vocal2A PUBLIC VG_HAS_BUNDLED_FONT_BOLD=1)
    endif()
endif()
