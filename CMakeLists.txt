cmake_minimum_required(VERSION 3.22)

# Univerzalni build (Apple Silicon i Intel), macOS 11+
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")

project(DeVerb VERSION 0.5.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE se stahne automaticky pri prvnim buildu
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.9
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(DUSI_FORMATS AU VST3 Standalone)   # AU = Logic Pro
else()
    set(DUSI_FORMATS VST3 Standalone)
endif()

juce_add_plugin(DeVerb
    COMPANY_NAME "DusiX"
    BUNDLE_ID com.dusix.deverb
    PLUGIN_MANUFACTURER_CODE Dusx
    PLUGIN_CODE Dxdv
    PRODUCT_NAME "DusiX DeVerb"
    FORMATS ${DUSI_FORMATS}
    AU_MAIN_TYPE kAudioUnitType_Effect
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)   # nakopiruje plugin do ~/Library/Audio/Plug-Ins

target_sources(DeVerb PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SpaceView.cpp
    Source/Widgets.cpp
    Source/EqView.cpp
    Source/ChainStrip.cpp
    Source/MainPage.cpp
    Source/EffectsPage.cpp
    Source/PresetBrowser.cpp
    Source/PresetManager.cpp
    Source/Presets.cpp
    Source/Updater.cpp
    Source/EffectViews.cpp)

# Pismo Inter (SIL OFL) a instalacni skript aktualizaci
juce_add_binary_data(DeVerbData SOURCES
    Resources/Fonts/Inter-Medium.ttf
    Resources/Fonts/Inter-SemiBold.ttf
    Resources/Fonts/Inter-Bold.ttf
    Resources/deverb-install.sh)
set_target_properties(DeVerbData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_compile_definitions(DeVerb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(DeVerb
    PRIVATE
        DeVerbData
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_cryptography
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
