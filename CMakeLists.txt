cmake_minimum_required(VERSION 3.22)
project(StreamLUFS VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.6
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(StreamLUFS
  COMPANY_NAME "YourName"
  PLUGIN_MANUFACTURER_CODE Ynam
  PLUGIN_CODE Slfs
  FORMATS VST3
  PRODUCT_NAME "Stream LUFS"
  VST3_CATEGORIES Fx Analyzer
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE)   # installs into your system VST3 folder

target_sources(StreamLUFS PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_compile_definitions(StreamLUFS PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(StreamLUFS
  PRIVATE juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# JUCE-free unit test of the loudness maths: cmake -DBUILD_TESTS=ON
option(BUILD_TESTS "Build meter self-test" OFF)
if(BUILD_TESTS)
  add_executable(meter_test test/test_meter.cpp)
  target_include_directories(meter_test PRIVATE Source)
endif()
