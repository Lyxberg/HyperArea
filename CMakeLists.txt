cmake_minimum_required(VERSION 3.22)

project(Hyperarea VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MINGW)
    add_link_options(-static -static-libgcc -static-libstdc++)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    message(FATAL_ERROR "JUCE not found. Put the JUCE source in ${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
endif()

juce_add_plugin(Hyperarea
    COMPANY_NAME            "BergAudio"
    BUNDLE_ID               "com.bergaudio.hyperarea"
    PLUGIN_MANUFACTURER_CODE Berg
    PLUGIN_CODE             Hra1
    FORMATS                 VST3 Standalone
    PRODUCT_NAME            "HYPERAREA"
    DESCRIPTION             "Pitch-tracked hyperarea bank for colour bass."
    IS_SYNTH                FALSE
    IS_MIDI_EFFECT          FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_IS_FREEWARE      TRUE
    VST3_CATEGORIES         Fx)

juce_generate_juce_header(Hyperarea)

target_sources(Hyperarea PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Hyperarea PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(Hyperarea
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags)

# ---- offline self-test ------------------------------------------------------
option(RESONATOR_BUILD_TEST "Build the offline self-test" ON)

if(RESONATOR_BUILD_TEST)
    juce_add_console_app(HyperareaTest PRODUCT_NAME "HyperareaTest")
    juce_generate_juce_header(HyperareaTest)

    target_include_directories(HyperareaTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")

    target_sources(HyperareaTest PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Test/EngineTest.cpp)

    target_compile_definitions(HyperareaTest PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JucePlugin_Name="Hyperarea")

    target_link_libraries(HyperareaTest
        PRIVATE
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_dsp
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags)
endif()
