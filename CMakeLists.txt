cmake_minimum_required(VERSION 3.22)
project(Scratcher VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Deployment target ─────────────────────────────────────────────────────────
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

# ── JUCE ──────────────────────────────────────────────────────────────────────
add_subdirectory(JUCE JUCE_build)

# ── Platform-specific plugin formats ─────────────────────────────────────────
if(APPLE)
    set(SCRATCHER_FORMATS VST3 AU Standalone)
elseif(WIN32)
    set(SCRATCHER_FORMATS VST3 Standalone)
else()
    set(SCRATCHER_FORMATS VST3 Standalone)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WEBKIT2GTK webkit2gtk-4.1 QUIET)
    if(NOT WEBKIT2GTK_FOUND)
        pkg_check_modules(WEBKIT2GTK webkit2gtk-4.0 REQUIRED)
    endif()
    pkg_check_modules(ALSA REQUIRED alsa)
    pkg_check_modules(FREETYPE REQUIRED freetype2)
    pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
endif()

# ── Icons ─────────────────────────────────────────────────────────────────────
set(SCRATCHER_ICON_PNG "${CMAKE_CURRENT_SOURCE_DIR}/assets/app.png")

# ── Plugin target ─────────────────────────────────────────────────────────────
juce_add_plugin(Scratcher
    VERSION                     "1.0"
    COMPANY_NAME                "Resonaura"
    COMPANY_WEBSITE             "https://resonaura.com"
    PLUGIN_MANUFACTURER_CODE    Rsna
    PLUGIN_CODE                 Scrt
    FORMATS                     ${SCRATCHER_FORMATS}
    PRODUCT_NAME                "Scratcher"
    DESCRIPTION                 "Vinyl scratch emulator by Resonaura"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE

    COPY_PLUGIN_AFTER_BUILD     FALSE

    ICON_BIG                    "${SCRATCHER_ICON_PNG}"
    ICON_SMALL                  "${SCRATCHER_ICON_PNG}"

    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    AU_EXPORT_PREFIX            ScratcherAU

    BUNDLE_ID                   com.resonaura.scratcher
    PLUGIN_AU_EXPORT_PREFIX     ScratcherAU

    SUPPRESS_AU_PLIST_RESOURCE_USAGE TRUE
)

juce_generate_juce_header(Scratcher)

# ── Binary data: fonts + assets ───────────────────────────────────────────────
juce_add_binary_data(ScratcherAssets
    NAMESPACE BinaryData
    HEADER_NAME BinaryData.h
    SOURCES
        fonts/PixgamerRegular.ttf
        fonts/PixelEmulator.ttf
        assets/scanlines.png
        assets/vinyl.mp3
)

# ── Sources ───────────────────────────────────────────────────────────────────
target_sources(Scratcher
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
        src/DeckProcessor.cpp
        src/SampleManager.cpp
        src/MidiLearnManager.cpp
        src/EnvelopeEditor.cpp
)

target_include_directories(Scratcher
    PRIVATE
        src
)

# ── Compile definitions ───────────────────────────────────────────────────────
target_compile_definitions(Scratcher
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_USE_OPENGL=1
)

# ── Link libraries ────────────────────────────────────────────────────────────
target_link_libraries(Scratcher
    PRIVATE
        ScratcherAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_plugin_client
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_events
        juce::juce_core
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ── Windows ───────────────────────────────────────────────────────────────────
if(WIN32)
    target_compile_definitions(Scratcher
        PRIVATE
            _CRT_SECURE_NO_WARNINGS
            _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
    )
endif()

# ── Linux ─────────────────────────────────────────────────────────────────────
if(UNIX AND NOT APPLE)
    target_link_libraries(Scratcher
        PRIVATE
            ${WEBKIT2GTK_LIBRARIES}
            ${ALSA_LIBRARIES}
            ${FREETYPE_LIBRARIES}
            ${FONTCONFIG_LIBRARIES}
            dl pthread
    )
    target_include_directories(Scratcher
        PRIVATE
            ${WEBKIT2GTK_INCLUDE_DIRS}
            ${ALSA_INCLUDE_DIRS}
            ${FREETYPE_INCLUDE_DIRS}
            ${FONTCONFIG_INCLUDE_DIRS}
    )
endif()
