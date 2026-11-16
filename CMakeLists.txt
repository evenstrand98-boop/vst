cmake_minimum_required(VERSION 3.22)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(Stepbox VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.8
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Stepbox
  COMPANY_NAME "Stepbox"
  PLUGIN_MANUFACTURER_CODE Stbx
  PLUGIN_CODE Sbx1
  FORMATS VST3 Standalone
  PRODUCT_NAME "Stepbox"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  VST3_CATEGORIES Instrument Drum
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Stepbox PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_compile_definitions(Stepbox PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Stepbox
  PRIVATE juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

option(STEPBOX_TESTS "Build the headless smoke test" OFF)
if(STEPBOX_TESTS)
  juce_add_console_app(StepboxSmoke PRODUCT_NAME "StepboxSmoke")
  target_sources(StepboxSmoke PRIVATE Tests/Smoke.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
  target_compile_definitions(StepboxSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(StepboxSmoke PRIVATE juce::juce_audio_utils)
endif()
