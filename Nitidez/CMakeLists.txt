cmake_minimum_required(VERSION 3.22)

project(Nitidez VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

option(NITIDEZ_BUILD_TESTS "Compilar o teste do motor de audio" OFF)

# --- JUCE (baixado automaticamente na primeira configuracao) -----------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# --- RNNoise (incluido em third_party, ja adaptado para o Visual Studio) -----
add_library(rnnoise STATIC
    third_party/rnnoise/src/celt_lpc.c
    third_party/rnnoise/src/denoise.c
    third_party/rnnoise/src/kiss_fft.c
    third_party/rnnoise/src/pitch.c
    third_party/rnnoise/src/rnn.c
    third_party/rnnoise/src/rnn_data.c)
target_include_directories(rnnoise
    PUBLIC  third_party/rnnoise/include
    PRIVATE third_party/rnnoise/src)
set_target_properties(rnnoise PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_definitions(rnnoise PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(rnnoise PRIVATE /w) # biblioteca de terceiros: sem avisos
else()
    target_compile_options(rnnoise PRIVATE -w -O2)
    target_link_libraries(rnnoise PRIVATE m)
endif()

# --- O plugin ----------------------------------------------------------------
juce_add_plugin(Nitidez
    COMPANY_NAME "Nitidez"
    PLUGIN_MANUFACTURER_CODE Ntdz
    PLUGIN_CODE Nt01
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    FORMATS VST3 Standalone
    VST3_CATEGORIES Fx Restoration
    PRODUCT_NAME "Nitidez"
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Nitidez PRIVATE
    Source/NitidezEngine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Nitidez PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Nitidez
    PRIVATE
        rnnoise
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(MSVC)
    target_compile_options(Nitidez PRIVATE /utf-8)
endif()

# --- Teste opcional do motor (sem JUCE) --------------------------------------
if(NITIDEZ_BUILD_TESTS)
    add_executable(nitidez_engine_test tests/engine_test.cpp Source/NitidezEngine.cpp)
    target_link_libraries(nitidez_engine_test PRIVATE rnnoise)
endif()
