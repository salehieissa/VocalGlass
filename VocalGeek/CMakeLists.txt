cmake_minimum_required(VERSION 3.22)

# Build a universal binary on macOS (Apple Silicon + Intel) so any host can load
# the plugin regardless of how it's launched (e.g. FL Studio under Rosetta).
# Ignored on non-Apple platforms. Override with -DCMAKE_OSX_ARCHITECTURES=...
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS universal binary")
endif()

project(VocalGeek VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Pull in JUCE automatically. First configure downloads it; then it's cached.
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# Plugin target: VST3 / AU / Standalone.
# ---------------------------------------------------------------------------
juce_add_plugin(VocalGeek
    COMPANY_NAME            "VocalEssential"
    PLUGIN_MANUFACTURER_CODE Vgls
    PLUGIN_CODE             Vgek
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "VocalGeek"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "VocalGeek needs audio input so the standalone app can process your voice."
    COPY_PLUGIN_AFTER_BUILD TRUE
)

target_sources(VocalGeek
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        ../common/Licensing/LicenseManager.cpp
)

target_compile_definitions(VocalGeek
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(VocalGeek
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# --- optional photoreal skin (assets/ui) ---
set(VG_SKIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/ui")
set(VG_SKIN_CANDIDATES
    "geek-chassis@2x.png"
    "geek-chassis-on@2x.png"
    "geek-chassis-on-smoke@2x.png"
    "geek-chassis-on-acid@2x.png"
    "geek-chassis-on-snow@2x.png"
    "geek-chassis-on-geeked@2x.png"
    "geek-bay-lean@2x.png"
    "geek-bay-smoke@2x.png"
    "geek-bay-acid@2x.png"
    "geek-bay-snow@2x.png"
    "geek-bay-geeked@2x.png"
    "geek-dpad@2x.png"
    "geek-hit@2x.png"
    "geek-hit-smoke@2x.png"
    "geek-hit-acid@2x.png"
    "geek-hit-snow@2x.png"
    "geek-hit-geeked@2x.png"
    "grit-knob-small@2x.png")
set(VG_SKIN_SOURCES "")
foreach(name ${VG_SKIN_CANDIDATES})
    if(EXISTS "${VG_SKIN_DIR}/${name}")
        list(APPEND VG_SKIN_SOURCES "${VG_SKIN_DIR}/${name}")
    endif()
endforeach()
if(VG_SKIN_SOURCES)
    juce_add_binary_data(VocalGeekSkin
        NAMESPACE   SkinBinary
        HEADER_NAME SkinBinary.h
        SOURCES     ${VG_SKIN_SOURCES})
    target_link_libraries(VocalGeek PRIVATE VocalGeekSkin)
    target_compile_definitions(VocalGeek PUBLIC VG_HAS_SKIN=1)
else()
    target_compile_definitions(VocalGeek PUBLIC VG_HAS_SKIN=0)
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
    juce_add_binary_data(VocalGeekFonts SOURCES ${VG_FONT_SOURCES})
    target_link_libraries(VocalGeek PRIVATE VocalGeekFonts)
    target_compile_definitions(VocalGeek PUBLIC VG_HAS_BUNDLED_FONT=1)
    if(EXISTS "${VG_FONT_BOLD}")
        target_compile_definitions(VocalGeek PUBLIC VG_HAS_BUNDLED_FONT_BOLD=1)
    endif()
endif()
