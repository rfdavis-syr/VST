cmake_minimum_required(VERSION 3.22)

# Universal-friendly macOS deployment target (must be set before project()).
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

project(HornDelay VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(HORN_DELAY_BUILD_TESTS "Build the offline DSP test executable" OFF)

# ---------------------------------------------------------------------------
# JUCE (downloaded automatically). To use a local checkout instead, configure with
#   -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(HornDelay
    COMPANY_NAME                "BoomBapAudio"
    BUNDLE_ID                   "com.boombapaudio.horndelay"
    PLUGIN_MANUFACTURER_CODE    "BbAu"
    PLUGIN_CODE                 "HnDl"
    PRODUCT_NAME                "Horn Delay"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Delay
    FORMATS                     VST3 Standalone)

set(HORN_DELAY_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_sources(HornDelay PRIVATE ${HORN_DELAY_SOURCES})

set(HORN_DELAY_DEFINES
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_compile_definitions(HornDelay PUBLIC ${HORN_DELAY_DEFINES})

target_link_libraries(HornDelay
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Offline test: renders audio through the processor and checks timing,
# stability, presets and state recall. Also writes a demo WAV.
# ---------------------------------------------------------------------------
if(HORN_DELAY_BUILD_TESTS)
    juce_add_console_app(HornDelayTests PRODUCT_NAME "HornDelayTests")
    target_sources(HornDelayTests PRIVATE tests/OfflineTest.cpp ${HORN_DELAY_SOURCES})
    target_compile_definitions(HornDelayTests PRIVATE ${HORN_DELAY_DEFINES})
    target_link_libraries(HornDelayTests
        PRIVATE
            juce::juce_audio_utils
            juce::juce_audio_formats
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
