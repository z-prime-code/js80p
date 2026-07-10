# ─────────────────────────────────────────────────────────────
# JS80P — JUCE 8 plugin build (VST3 + Standalone).
#
# Wraps the unchanged JS80P `Synth` DSP in a juce::AudioProcessor and hosts the
# toolkit-agnostic GUI (src/gui/gui.cpp + widgets.cpp) through a single new JUCE
# `Widget` backend (src/gui/juce.cpp). The legacy Makefile still builds the
# hand-rolled FST/VST3 wrappers and the test suite; this file is additive.
# ─────────────────────────────────────────────────────────────
cmake_minimum_required(VERSION 3.22)

project(JS80P VERSION 4.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ── JUCE via FetchContent (pinned to current stable) ─────────
include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        8.0.14
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ── The plugin target ────────────────────────────────────────
juce_add_plugin(JS80P
  COMPANY_NAME                "Attila M. Magyar"
  BUNDLE_ID                   com.attilammagyar.js80p
  PLUGIN_MANUFACTURER_CODE    Amma
  PLUGIN_CODE                 Js8p
  FORMATS                     VST3 Standalone
  PRODUCT_NAME                "JS80P"
  IS_SYNTH                    TRUE
  NEEDS_MIDI_INPUT            TRUE
  NEEDS_MIDI_OUTPUT           FALSE
  IS_MIDI_EFFECT              FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD     FALSE
  VST3_CATEGORIES             "Instrument" "Synth")

# DSP core (unity build: synth.cpp #includes all of src/dsp/*.cpp), the patch
# serializer + program bank, MTS-ESP, the single JUCE GUI backend (which
# #includes gui.cpp + widgets.cpp), and the processor/editor.
target_sources(JS80P PRIVATE
  src/synth.cpp
  src/serializer.cpp
  src/bank.cpp
  lib/mtsesp/Client/libMTSClient.cpp
  src/gui/juce.cpp
  src/ui/modulation_manager.cpp
  src/ui/control.cpp
  src/ui/harmonic_slider.cpp
  src/ui/mini_button.cpp
  src/ui/waveform_selector.cpp
  src/ui/filter_type_selector.cpp
  src/ui/selector.cpp
  src/ui/filter_panel.cpp
  src/ui/per_type_editor.cpp
  src/ui/curve_selector.cpp
  src/ui/macro_strip.cpp
  src/ui/modulator_card.cpp
  src/ui/effects_page.cpp
  src/ui/new_gui.cpp
  src/plugin/juce/PluginProcessor.cpp
  src/plugin/juce/PluginEditor.cpp)

target_include_directories(JS80P PRIVATE src lib)

# The VST3 IMidiMapping is VST3-only code: it pulls in the Steinberg VST3 SDK,
# whose interface-IID symbols (FUnknown::iid, IMidiMapping::iid) are supplied by
# JUCE's VST3 wrapper in the SAME link. Compiling it into the shared code would
# leak those ODR-uses into non-VST3 targets (undefined in the Standalone link on
# MSVC/lld-link). So vst3_midi_mapping.cpp is compiled into EVERY plugin-format
# target (juce_add_plugin names them JS80P_<FORMAT> and defines
# JucePlugin_Build_<FORMAT> 1/0 on each): under JucePlugin_Build_VST3 it is the
# real mapping, in every other format it compiles to a nullptr fallback. The
# shared PluginProcessor calls create_vst3_midi_mapping_extensions() and the
# per-format object resolves it. The real branch depends only on JUCE + the SDK
# (the supported-controller set is passed in), so only the SDK include path is
# added, and only for VST3. The SDK path is JUCE-internal, hence pinned
# alongside the JUCE GIT_TAG above.
foreach(_js80p_fmt VST3 Standalone AU AUv3 VST LV2 AAX Unity)
  if(TARGET JS80P_${_js80p_fmt})
    target_sources(JS80P_${_js80p_fmt}
      PRIVATE src/plugin/juce/vst3_midi_mapping.cpp)
    target_include_directories(JS80P_${_js80p_fmt} PRIVATE src)
  endif()
endforeach()

if(TARGET JS80P_VST3)
  target_include_directories(JS80P_VST3 PRIVATE
    "${juce_SOURCE_DIR}/modules/juce_audio_processors_headless/format_types/VST3_SDK")
endif()

# Windows toolchain squeeze — the one compiler that works is clang-cl:
#   * JUCE hard-errors on MinGW/GCC (juce_TargetPlatform.h: "MinGW is not
#     supported"), so a GNU-front-end compiler on Windows is out.
#   * JS80P's DSP/GUI use C99 *array*-designated initializers ("[INDEX] = ...",
#     e.g. Synth::MPE_ZONES in synth.hpp) that MSVC's cl.exe rejects, so plain
#     MSVC is out.
#   * clang-cl (Clang front end + MSVC ABI) satisfies both. It reports
#     CMAKE_CXX_COMPILER_ID == "Clang" with FRONTEND_VARIANT == "MSVC", so the
#     guards below only trip on cl.exe / MinGW, never on clang-cl. See BUILD.md.
set(_JS80P_WIN_HINT
  "The only supported Windows compiler is clang-cl (Clang front end + MSVC ABI):\n"
  "  Install \"Build Tools for Visual Studio 2022\" (or the full IDE) with the\n"
  "  \"Desktop development with C++\" workload + the \"C++ Clang tools for Windows\"\n"
  "  component, then from an \"x64 Native Tools Command Prompt for VS 2022\":\n"
  "    cmake -B build -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release\n"
  "  or, with the full IDE:\n"
  "    cmake -B build -G \"Visual Studio 17 2022\" -A x64 -T ClangCL")

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  message(FATAL_ERROR
    "JS80P cannot be built with the MSVC compiler (cl.exe): its DSP uses C99 "
    "array-designated initializers ([index] = ...) that MSVC does not support.\n"
    ${_JS80P_WIN_HINT})
endif()

if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  message(FATAL_ERROR
    "JUCE does not support MinGW/GCC on Windows (juce_TargetPlatform.h emits "
    "#error \"MinGW is not supported\").\n"
    ${_JS80P_WIN_HINT})
endif()

# MTS-ESP client trips -Warray-bounds / -Wchar-subscripts on GCC/Clang; match
# the Makefile's per-file silencing. clang-cl (MSVC front end) doesn't accept
# these spellings, and doesn't warn here, so skip it there.
if(NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  set_source_files_properties(lib/mtsesp/Client/libMTSClient.cpp PROPERTIES
    COMPILE_OPTIONS "-Wno-array-bounds;-Wno-char-subscripts")
endif()

# ── Embed the 21 GUI PNGs as juce::BinaryData ────────────────
juce_add_binary_data(JS80PAssets
  HEADER_NAME BinaryData.h
  NAMESPACE   BinaryData
  SOURCES
    gui/img/about.png
    gui/img/effects.png
    gui/img/envelopes1.png
    gui/img/envelopes2.png
    gui/img/env_shapes-01.png
    gui/img/env_shapes-10.png
    gui/img/knob_states-controlled.png
    gui/img/knob_states-free.png
    gui/img/knob_states-none.png
    gui/img/knob_states-red.png
    gui/img/lfos.png
    gui/img/macros1.png
    gui/img/macros2.png
    gui/img/macros3.png
    gui/img/macro_distortions.png
    gui/img/macro_midpoint_states.png
    gui/img/reversed.png
    gui/img/screw_states.png
    gui/img/screw_states_synced.png
    gui/img/synth.png
    gui/img/vst_logo.png)

# juce_add_binary_data defaults to a non-PIC static lib; the plugin is a shared
# module, so the object code must be relocatable.
set_target_properties(JS80PAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# ── Drop-in-replacement identity: force the legacy VST3 class ID ──
# A host keys a plugin instance in a saved project off the VST3 *component*
# (audio-module / kVstAudioEffectClass) class ID. To load transparently into
# projects that used the original hand-rolled JS80P VST3, the JUCE port must
# expose the SAME component class ID instead of the ID JUCE would otherwise
# derive by hashing PLUGIN_MANUFACTURER_CODE + PLUGIN_CODE.
#
# The legacy ID is the Steinberg FUID(0x565354, 0x414d4a38, 0x6a733830,
# 0x70000000) — bytes "\0VST" "AMJ8" "js80" "p" — registered via the SDK's
# INLINE_UID macro. That macro lays the 16 bytes out DIFFERENTLY depending on
# COM_COMPATIBLE (1 on Windows, 0 on macOS/Linux), so the raw class ID a host
# stores is platform-specific. JUCE's JUCE_VST3_COMPONENT_CLASS override feeds
# the hex straight through (juce::VST3Interface::hexStringToId, no swap) and the
# factory reports those bytes verbatim, so we must supply the matching layout
# per platform. Values below were verified against the actual SDK macro.
#   * macOS/Linux (COM_COMPATIBLE=0): 00 56 53 54 41 4D 4A 38 6A 73 38 30 70 …
#   * Windows     (COM_COMPATIBLE=1): 54 53 56 00 4D 41 38 4A 6A 73 38 30 70 …
# Only the component class is overridable (and only it matters for identity);
# the controller class ID is not persisted by hosts, so JUCE's own is fine.
# The saved-state chunk is already the raw Serializer patch on both sides, so
# old projects restore their patch unchanged once the class ID matches.
if(WIN32)
  set(JS80P_VST3_COMPONENT_CLASS "545356004D41384A6A73383070000000")
else()
  set(JS80P_VST3_COMPONENT_CLASS "00565354414D4A386A73383070000000")
endif()

# JS80P_* are consumed only by the "About" screen (version/platform strings);
# the DSP does no compile-time ISA dispatch, so these are display-only tokens.
target_compile_definitions(JS80P PRIVATE
  JS80P_VERSION_STR=${PROJECT_VERSION}
  JS80P_VERSION_INT=40100
  JS80P_TARGET_PLATFORM=juce
  JS80P_INSTRUCTION_SET=auto
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_VST3_COMPONENT_CLASS="${JS80P_VST3_COMPONENT_CLASS}"
  # Keep JUCE's built-in MIDI-CC emulation ENABLED (=1 is the JUCE default; kept
  # explicit because it is load-bearing). The VST3 controller object itself
  # implements IMidiMapping, and hosts that query that interface BEFORE
  # connecting the component and controller (Bitwig) cache the controller's
  # stub and never see the legacy IMidiMapping we expose through
  # VST3ClientExtensions — with emulation off that stub answers kResultFalse
  # for every controller, so MIDI CC / pitch bend / channel pressure are never
  # delivered at all in such hosts. With emulation on, that same always-available
  # mapping routes every (channel, controller) to a hidden non-automatable
  # "MIDI CC" parameter and the wrapper turns host parameter changes back into
  # MIDI events for processBlock() — the standard JUCE-synth path. Hosts that
  # query after connecting (REAPER) still receive our VST3ClientExtensions
  # IMidiMapping with the legacy ParamIDs instead (user-provided interfaces win
  # in the wrapper), exactly like the original plugin. See
  # src/plugin/juce/vst3_midi_mapping.hpp and BUILD.md.
  JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=1
  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_STRICT_REFCOUNTEDPOINTER=1)

# Release fast-math (spelling differs by front end), plus silence the harmless
# "array designators are a C99 extension" warning on Clang/clang-cl (GCC does
# not warn). Release -O level is already set by CMake per compiler.
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  # clang-cl: MSVC-style driver, but accepts clang -W warning flags.
  target_compile_options(JS80P PRIVATE
    -Wno-c99-designator
    $<$<CONFIG:Release>:/fp:fast>)
else()
  # GCC, Clang, MinGW, AppleClang.
  target_compile_options(JS80P PRIVATE
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-c99-designator>
    $<$<CONFIG:Release>:-O3 -ffast-math>)
endif()

target_link_libraries(JS80P
  PRIVATE
    JS80PAssets
    juce::juce_audio_utils
    juce::juce_gui_extra
  PUBLIC
    juce::juce_recommended_config_flags)
