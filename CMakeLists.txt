# ─────────────────────────────────────────────────────────────
# JS80P — the JUCE GUI on top of the original plugin.
#
# Two targets, from one set of sources:
#
#   JS80P_CompatVST3  the VST3 that ships. Built from JS80P's own hand-rolled
#                     VST3 wrapper (src/plugin/vst3) against the vendored SDK
#                     (lib/vst3sdk) — the same sources, defines and output layout
#                     the Makefile uses — so the class IDs, ParamID scheme, state
#                     format, MIDI mapping and program list are the original's by
#                     construction, and hosts load it into projects saved with
#                     the original JS80P. JUCE is linked for the GUI alone:
#                     juce_gui_extra, no juce_audio_processors, no
#                     juce_add_plugin, and none of JUCE's own VST3 wrapper.
#                     See src/plugin/vst3/plugin-juce.cpp.
#
#   JS80P_Standalone  a JUCE app wrapping the same editor, for working on the GUI
#                     without a host. Not a released artefact, and deliberately
#                     NOT built as a VST3: JUCE's wrapper cannot reproduce the
#                     original's parameter and state layout, which is the whole
#                     point of the target above.
#
# The editor itself (src/ui, src/gui/juce.cpp) knows only JS80P::Synth, so both
# targets host the same juce::Component — see src/ui/editor_root.hpp.
#
# The legacy Makefile still builds the FST/VST2 wrapper, the original non-JUCE
# GUI, and the test suite.
# ─────────────────────────────────────────────────────────────
cmake_minimum_required(VERSION 3.22)

# ── macOS minimum-OS floor ───────────────────────────────────
# Without this, the Mach-O's LC_BUILD_VERSION minos defaults to the SDK on the
# build machine (macOS 15 on the release runner), and dyld then refuses to load
# the bundle on any older system -- Bitwig on Monterey reports it as
# "Could not load executable: The bundle couldn't be loaded". The original
# Makefile build set this per slice (release.sh: x86_64-apple-macos10.12,
# arm64-apple-macos11); the CMake port dropped it. 10.13 is JUCE 8's floor (its
# GUI is linked here, so 10.12 is no longer reachable); the linker clamps the
# arm64 slice up to 11.0 on its own, matching the original. Set before project()
# so it reaches every target; ignored on non-Apple platforms. Overridable.
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.12" CACHE STRING "Minimum macOS version to build for")
endif()

project(JS80P VERSION 4.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ── Windows toolchain squeeze — the one compiler that works is clang-cl ──
#   * JUCE hard-errors on MinGW/GCC (juce_TargetPlatform.h: "MinGW is not
#     supported"), so a GNU-front-end compiler on Windows is out. This is also
#     why the JUCE GUI cannot be bolted onto the Makefile, whose Windows builds
#     are all MinGW cross-compiles: on Windows, this file is the only way in.
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

# ── JUCE via FetchContent (pinned to current stable) ─────────
include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        8.0.14
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ─────────────────────────────────────────────────────────────
# Version + platform tokens.
#
# These mirror the Makefile's variables of the same names, because the dist path
# below is built out of them exactly as DIST_DIR_PREFIX is. Overridable for the
# same reason scripts/release.sh overrides them.
# ─────────────────────────────────────────────────────────────
set(JS80P_VERSION_STR "${PROJECT_VERSION}" CACHE STRING "Version, as shown on the About screen")

# VERSION_INT, derived rather than typed, by scripts/release.sh's own recipe
# (version_str_to_int, release.sh:389): append a "0" to the dotted version and
# strip everything that is not a digit. The Makefile's defaults are the fixture
# that pins this -- VERSION_DOT ?= 999.0.0 alongside VERSION_INT ?= 999000 --
# and "999.0.0" -> "999.0.00" -> "999000" reproduces it. So 4.1.0 is 4100.
#
# Only the FST/VST2 wrapper reports this to a host (src/plugin/fst/plugin.hpp:103,
# via Constants::PLUGIN_VERSION_INT); nothing on the VST3 path reads it. It is
# derived anyway, because a value that is never read is exactly the kind that
# rots unnoticed.
string(REGEX REPLACE "[^0-9]" "" _js80p_default_version_int "${PROJECT_VERSION}0")

set(JS80P_VERSION_INT "${_js80p_default_version_int}" CACHE STRING "Version, as a packed integer")

# VERSION_AS_FILE_NAME (version_str_to_file_name, release.sh:396): everything
# outside [0-9a-f-] becomes an underscore, which is what keeps a dev version like
# "4.1.0-1a2b3c4" usable as a path component.
string(REGEX REPLACE "[^0-9a-f-]" "_" _js80p_default_version_file "${JS80P_VERSION_STR}")

set(JS80P_VERSION_AS_FILE_NAME "${_js80p_default_version_file}" CACHE STRING "Version, as it appears in the dist path")

# TARGET_OS in make/*.mk.
if(APPLE)
  set(_js80p_target_os "macos")
elseif(WIN32)
  set(_js80p_target_os "windows")
else()
  set(_js80p_target_os "linux")
endif()

# ─────────────────────────────────────────────────────────────
# The three shipped targets, and how they map onto the original's naming.
#
# scripts/release.sh builds ten platform:instruction-set pairs; these are the
# three that carry AVX, which is the whole of what this build ships:
#
#   linux   x86_64 -mavx       -> js80p-<v>-linux-x86_64-avx-vst3_single/js80p.vst3
#   windows x86_64 -mavx       -> js80p-<v>-windows-x86_64-avx-vst3_single/js80p.vst3
#   macos   arm64 + x86_64/avx -> js80p-<v>-macos-universal-vst3_bundle/js80p.vst3/
#
# NB the upstream sse2 variants are deliberately not reproduced: AVX is Sandy
# Bridge / Bulldozer (2011) and later, but Intel shipped AVX-less Celerons and
# Pentiums (Gemini Lake, Jasper Lake) well into the 2020s, and those machines get
# SIGILL out of these binaries rather than a diagnostic. Building an sse2 row is
# a matter of -DJS80P_INSTRUCTION_SET=sse2 plus a matrix entry in release.yml.
#
# macOS is the odd one out in the original too, and this mirrors it rather than
# inventing anything: release.sh drives make twice with SUFFIX=arm64 and
# SUFFIX=x86_64 (INSTRUCTION_SET=none in both, because the -mavx for the x86_64
# slice rides in on CPP_DEV_PLATFORM rather than the Makefile's -m$(ISA) rule),
# then lipos the two Mach-O binaries into a single bundle named
# js80p-<v>-macos-universal-vst3_bundle -- no arch and no ISA token. CMake builds
# the fat binary in one pass (CMAKE_OSX_ARCHITECTURES), so the per-arch
# vst3_single directories that only ever existed as lipo inputs have nothing to
# stand for here, and only the bundle is emitted.
# ─────────────────────────────────────────────────────────────

# SUFFIX in make/*.mk: the architecture token. Unused on macOS, whose shipped
# artefact is named "universal" rather than after an arch.
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(_js80p_default_suffix "x86_64")
else()
  set(_js80p_default_suffix "x86")
endif()

set(JS80P_SUFFIX "${_js80p_default_suffix}" CACHE STRING "Architecture token in the dist path")

# INSTRUCTION_SET in make/*.mk -- both the compile flag and the dist-path token,
# exactly as in the Makefile, whose vocabulary this shares: avx, sse2, none, lsx
# or native. It is NOT a free-form label; js80p_configure() below turns it into a
# real -m flag, and the About screen (src/gui/widgets.hpp:647) prints it.
if(APPLE)
  # As in release.sh's macOS path: the token is none, and the x86_64 slice gets
  # its -mavx from the per-arch flag in js80p_configure() instead.
  set(_js80p_default_isa "none")
else()
  set(_js80p_default_isa "avx")
endif()

set(JS80P_INSTRUCTION_SET "${_js80p_default_isa}" CACHE STRING "Instruction set: avx, sse2, none, lsx or native")

# JS80P_TARGET_PLATFORM in the Makefile is the toolchain triplet, and it too
# lands on the About screen. Name the real one rather than "juce".
if(APPLE)
  set(_js80p_target_platform "gpp")
elseif(WIN32)
  set(_js80p_target_platform "x86_64-w64-mingw32")
else()
  set(_js80p_target_platform "${JS80P_SUFFIX}-gpp")
endif()

set(JS80P_TARGET_PLATFORM "${_js80p_target_platform}" CACHE STRING "Toolchain triplet, as shown on the About screen")

# DIST_DIR_BASE / DIST_DIR_PREFIX in the Makefile, rooted in the build tree so a
# CMake build never writes into the Makefile's dist/.
set(JS80P_DIST_DIR_BASE "${CMAKE_CURRENT_BINARY_DIR}/dist" CACHE PATH "Root of the packaged output")

if(APPLE)
  set(JS80P_DIST_DIR
    "${JS80P_DIST_DIR_BASE}/js80p-${JS80P_VERSION_AS_FILE_NAME}-macos-universal-vst3_bundle")
else()
  set(JS80P_DIST_DIR
    "${JS80P_DIST_DIR_BASE}/js80p-${JS80P_VERSION_AS_FILE_NAME}-${_js80p_target_os}-${JS80P_SUFFIX}-${JS80P_INSTRUCTION_SET}-vst3_single")
endif()

# ── Sources shared by both targets ───────────────────────────
# DSP core (unity build: synth.cpp #includes all of src/dsp/*.cpp; bank.cpp
# #includes programs.cpp), the patch serializer, MTS-ESP, and the single JUCE GUI
# backend for the original editor (juce.cpp #includes gui.cpp + widgets.cpp).
set(JS80P_CORE_SOURCES
  src/synth.cpp
  src/serializer.cpp
  src/bank.cpp
  lib/mtsesp/Client/libMTSClient.cpp
  src/gui/juce.cpp)

# The new editor. Toolkit-only: juce_gui_basics / juce_graphics plus Synth.
set(JS80P_UI_SOURCES
  src/ui/modulation_manager.cpp
  src/ui/control.cpp
  src/ui/harmonic_slider.cpp
  src/ui/glyph_sheet.cpp
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
  src/ui/editor_root.cpp)

# ─────────────────────────────────────────────────────────────
# JS80P_INSTRUCTION_SET -> compile flag, following Makefile:123-128 exactly:
#
#   native      -march=native -mtune=native
#   none        (nothing)
#   <anything>  -m<anything>
#
# so the token that names the dist directory and the token the About screen
# prints are the same one that reached the compiler. The Makefile has no "auto":
# it would expand to -mauto and fail to compile, which is why the value is
# validated here rather than pasted into a flag and hoped for.
#
# macOS is the exception, and mirrors release.sh: the token stays "none" and the
# x86_64 slice gets -mavx via -Xarch_x86_64, which applies a flag to one slice of
# a fat binary only. -mavx is meaningless to the arm64 slice and clang rejects it
# outright there, so it cannot simply go in the global flags. SHELL: keeps the
# two tokens together -- CMake de-duplicates compile options individually and
# would otherwise be free to separate -Xarch_x86_64 from the flag it governs.
# ─────────────────────────────────────────────────────────────
set(_JS80P_KNOWN_ISA avx sse2 none lsx native)

if(NOT JS80P_INSTRUCTION_SET IN_LIST _JS80P_KNOWN_ISA)
  # "auto" is special: it was this project's own default until the dist paths
  # were aligned with the Makefile's, and it is not a token anyone would pass on
  # purpose. Seeing it means the build directory predates that change rather than
  # that someone mistyped -- so say what actually happened.
  #
  # It is worth singling out because it is the *only* loud symptom of a stale
  # tree. A set(... CACHE ...) default never overwrites an existing cache entry,
  # so JS80P_VERSION_AS_FILE_NAME and JS80P_VERSION_INT are stale in the same
  # directory, and those two just quietly misname the release ("4.1.0" instead of
  # "4_1_0") rather than stopping the build. Reconfiguring around this error with
  # -DJS80P_INSTRUCTION_SET=avx would therefore paper over the two that matter.
  if(JS80P_INSTRUCTION_SET STREQUAL "auto")
    message(FATAL_ERROR
      "This build directory's cache predates the dist-path changes: "
      "JS80P_INSTRUCTION_SET is \"auto\", which was the old default and was never "
      "a real instruction set.\n"
      "JS80P_VERSION_AS_FILE_NAME and JS80P_VERSION_INT are stale in here too, "
      "and they misname the release silently, so do not just override this one "
      "on the command line. Drop the cache and re-run the same cmake command "
      "(generator flags included -- they live in the cache too):\n"
      "    cmake -E rm -f \"${CMAKE_CURRENT_BINARY_DIR}/CMakeCache.txt\"\n"
      "Every cache entry lives in that one file, so this resets all three. "
      "Deleting the whole build directory works as well, but it is both slower "
      "-- FetchContent re-downloads JUCE, ~250MB, into _deps -- and likelier to "
      "fail: an IDE with the project open (or a lingering MSBuild node on "
      "Windows) holds handles to the tree, but rarely to the cache file.")
  endif()

  message(FATAL_ERROR
    "JS80P_INSTRUCTION_SET is \"${JS80P_INSTRUCTION_SET}\", which is not one of: "
    "${_JS80P_KNOWN_ISA}. This token both names the dist directory and becomes a "
    "-m flag, so it has to be a real instruction set the compiler knows.")
endif()

if(JS80P_INSTRUCTION_SET STREQUAL "native")
  set(_js80p_isa_flags -march=native -mtune=native)
elseif(JS80P_INSTRUCTION_SET STREQUAL "none")
  set(_js80p_isa_flags "")
else()
  set(_js80p_isa_flags "-m${JS80P_INSTRUCTION_SET}")
endif()

if(APPLE)
  # Per-slice, so the universal binary carries AVX code on Intel and plain arm64
  # code on Apple Silicon. release.sh:436 does the same with -mavx on the x86_64
  # make invocation only.
  set(_js80p_isa_flags "SHELL:-Xarch_x86_64 -mavx")
endif()

# JS80P_VERSION_* / JS80P_TARGET_PLATFORM / JS80P_INSTRUCTION_SET are consumed by
# the "About" screen (src/gui/widgets.hpp:645-649); the DSP does no runtime ISA
# dispatch, so the instruction set is a compile-time decision and the token is
# how a user tells which binary they installed.
function(js80p_configure target)
  target_include_directories(${target} PRIVATE src lib)

  target_compile_definitions(${target} PRIVATE
    JS80P_VERSION_STR=${JS80P_VERSION_STR}
    JS80P_VERSION_INT=${JS80P_VERSION_INT}
    JS80P_TARGET_PLATFORM=${JS80P_TARGET_PLATFORM}
    JS80P_INSTRUCTION_SET=${JS80P_INSTRUCTION_SET}
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

  if(_js80p_isa_flags)
    target_compile_options(${target} PRIVATE ${_js80p_isa_flags})
  endif()

  if(WIN32)
    # <windows.h> defines min/max as function-like macros unless NOMINMAX is set,
    # which then eats every std::numeric_limits<T>::max() and JS80P's own
    # Math::EXP_MIN / EXP_MAX (src/dsp/math.hpp).
    #
    # JUCE (juce_BasicNativeHeaders.h) and the VST3 SDK (base/source/fstring.cpp)
    # each define it themselves — but only immediately before their *own*
    # <windows.h>, which is worthless once anything earlier in the translation
    # unit has already pulled the header in. The SDK's unity build is exactly
    # that: vst3.cpp #includes fdebug.cpp — a bare <windows.h>, no NOMINMAX — at
    # line 25, long before fstring.cpp tries to set it at line 33. dll.cpp then
    # #includes gui/gui.hpp after all of it, so the DSP headers get hit too.
    #
    # Defining it here is the only order-independent fix. Harmless for the JUCE
    # target, which only got away with it because its headers happened to come
    # first.
    target_compile_definitions(${target} PRIVATE NOMINMAX)
  endif()

  # Release fast-math (spelling differs by front end), plus silence the harmless
  # "array designators are a C99 extension" warning on Clang/clang-cl (GCC does
  # not warn). Release -O level is already set by CMake per compiler.
  if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    # clang-cl: MSVC-style driver, but accepts clang -W warning flags.
    target_compile_options(${target} PRIVATE
      -Wno-c99-designator
      $<$<CONFIG:Release>:/fp:fast>)
  else()
    # GCC, Clang, MinGW, AppleClang.
    target_compile_options(${target} PRIVATE
      $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-c99-designator>
      $<$<CONFIG:Release>:-O3 -ffast-math>)
  endif()
endfunction()

# MTS-ESP client trips -Warray-bounds / -Wchar-subscripts on GCC/Clang; match the
# Makefile's per-file silencing. clang-cl (MSVC front end) doesn't accept these
# spellings, and doesn't warn here, so skip it there.
if(NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  set_source_files_properties(lib/mtsesp/Client/libMTSClient.cpp PROPERTIES
    COMPILE_OPTIONS "-Wno-array-bounds;-Wno-char-subscripts")
endif()

# ── Embed the 21 GUI PNGs as juce::BinaryData ────────────────
# The Makefile instead links them in with objcopy (Linux/Windows) or copies them
# into Contents/Resources/img (macOS), because the original GUI backends load
# them per platform. The JUCE backend reads them from BinaryData on every
# platform, so the macOS bundle below has no Resources/img — that part of the
# original's layout has nothing to hold.
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

# juce_add_binary_data defaults to a non-PIC static lib; both consumers are
# shared modules, so the object code must be relocatable.
set_target_properties(JS80PAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# ─────────────────────────────────────────────────────────────
# The compatibility VST3 — the artefact that ships.
# ─────────────────────────────────────────────────────────────
# Per-platform module entry point and the FIDString the view reports to the host.
# These mirror VST3_MAIN_SOURCES / VST3_GUI_PLATFORM in make/*.mk. The entry point
# is also the SDK's unity build: so.cpp/dll.cpp/mach-o.cpp #include vst3.cpp,
# which #includes every SDK .cpp JS80P needs — hence the whole Steinberg SDK
# costs exactly one source file here.
if(APPLE)
  set(_js80p_vst3_main     src/plugin/vst3/mach-o.cpp)
  set(_js80p_gui_platform  kPlatformTypeNSView)
elseif(WIN32)
  set(_js80p_vst3_main     src/plugin/vst3/dll.cpp)
  set(_js80p_gui_platform  kPlatformTypeHWND)
else()
  set(_js80p_vst3_main     src/plugin/vst3/so.cpp)
  set(_js80p_gui_platform  kPlatformTypeX11EmbedWindowID)
endif()

add_library(JS80P_CompatVST3 MODULE
  ${JS80P_CORE_SOURCES}
  ${JS80P_UI_SOURCES}
  src/plugin/vst3/plugin.cpp
  src/plugin/vst3/plugin-juce.cpp
  ${_js80p_vst3_main})

js80p_configure(JS80P_CompatVST3)

# lib/vst3sdk is JS80P's own vendored SDK. Nothing pulls in JUCE's bundled copy —
# juce_audio_processors is not linked here — so there is no clash between the two.
target_include_directories(JS80P_CompatVST3 PRIVATE lib/vst3sdk)

target_compile_definitions(JS80P_CompatVST3 PRIVATE
  # Selects the JUCE editor over the original xcb/win32/macos GUI in
  # src/plugin/vst3/plugin.cpp and plugin.hpp.
  JS80P_JUCE_GUI=1
  JS80P_VST3_GUI_PLATFORM=${_js80p_gui_platform}
  # The SDK keys its logging/assertion machinery off exactly one of these; the
  # Makefile always passes RELEASE.
  $<IF:$<CONFIG:Debug>,DEVELOPMENT=1,RELEASE=1>)

# The SDK does not build warning-clean; these are the Makefile's VST3_CXXFLAGS
# suppressions. clang-cl takes clang -W spellings, plain MSVC never gets here.
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_compile_options(JS80P_CompatVST3 PRIVATE
    -Wno-class-memaccess
    -Wno-error=deprecated-declarations
    -Wno-format
    -Wno-implicit-const-int-float-conversion
    -Wno-multichar
    -Wno-nontrivial-memcall
    -Wno-parentheses
    -Wno-pragmas
    -Wno-unknown-pragmas
    -Wno-unknown-warning-option
    -Wno-unused-but-set-variable
    -Wno-unused-value)
endif()

if(APPLE)
  # vst3.cpp #includes the SDK's .mm sources, so every translation unit that
  # reaches it has to be Objective-C++ — VST3_TARGET_PLATFORM_CXXFLAGS in
  # make/macos-gpp.mk does the same for the whole VST3 compile.
  set_source_files_properties(
    ${_js80p_vst3_main}
    src/plugin/vst3/plugin.cpp
    src/plugin/vst3/plugin-juce.cpp
    PROPERTIES COMPILE_OPTIONS "-fobjc-arc;-x;objective-c++")
endif()

# NB: src/plugin/vst3/js80p.def is deliberately NOT used here, unlike in the
# Makefile's MinGW build. GetPluginFactory / InitDll / ExitDll already carry the
# SDK's SMTG_EXPORT_SYMBOL, which is __declspec(dllexport) under _MSC_VER — so
# clang-cl exports them without help, and the .def's "LIBRARY js80p.dll" line
# would only contradict the js80p.vst3 module name set below. If the factory ever
# does go missing from the exports on Windows, this is the first place to look.

# juce_gui_extra pulls in juce_gui_basics / juce_graphics / juce_events /
# juce_data_structures / juce_core, which is the whole of JUCE that the editor
# uses. Module targets are INTERFACE libraries carrying their own sources, so
# linking them compiles JUCE into this plain MODULE library — no juce_add_plugin
# needed, and none of its VST3 wrapper comes along.
target_link_libraries(JS80P_CompatVST3
  PRIVATE
    JS80PAssets
    juce::juce_gui_extra
  PUBLIC
    juce::juce_recommended_config_flags)

# ── Output layout: the Makefile's, exactly ───────────────────
# Linux/Windows ship the "vst3_single" form — one shared library named
# js80p.vst3, no bundle around it (make/linux-gpp.mk:6, make/mingw.mk:24). macOS
# needs a real bundle. Both names are lower-case, as the original's are; a VST3's
# identity is its class ID rather than its file name, but the install paths
# people already have are not.
#
#   js80p-<v>-linux-x86_64-avx-vst3_single/js80p.vst3          <- ELF .so
#   js80p-<v>-windows-x86_64-avx-vst3_single/js80p.vst3        <- PE DLL
#   js80p-<v>-macos-universal-vst3_bundle/js80p.vst3/Contents/
#       Info.plist
#       PkgInfo
#       MacOS/js80p                                            <- fat Mach-O
#       Resources/moduleinfo.json
#       Resources/Snapshots/<class-id>_snapshot.png
#
# The bundle's Resources has no img/ subdirectory, unlike make/macos-gpp.mk's:
# the original's macOS GUI loads its 21 PNGs from there at runtime, whereas the
# JUCE backend reads them out of BinaryData, so there is nothing to put in it.
#
# The $<1:...> wrapper stops multi-config generators (Visual Studio) from
# appending a per-config directory to the path.
if(APPLE)
  set_target_properties(JS80P_CompatVST3 PROPERTIES
    OUTPUT_NAME "js80p"
    PREFIX ""
    SUFFIX ""
    LIBRARY_OUTPUT_DIRECTORY "$<1:${JS80P_DIST_DIR}/js80p.vst3/Contents/MacOS>")

  # The bundle will not load without this, and CFBundleExecutable in the template
  # already says "js80p", matching OUTPUT_NAME above. Substitutions are the ones
  # make/macos-gpp.mk applies to the same template.
  string(TIMESTAMP _js80p_year "%Y")
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/vst3/Info.plist.tpl" _js80p_plist)
  string(REPLACE "{VERSION}" "${JS80P_VERSION_STR}" _js80p_plist "${_js80p_plist}")
  string(REPLACE "{YEAR}" "${_js80p_year}" _js80p_plist "${_js80p_plist}")
  file(GENERATE
    OUTPUT "${JS80P_DIST_DIR}/js80p.vst3/Contents/Info.plist"
    CONTENT "${_js80p_plist}")

  # release.sh:520 writes this into every macOS bundle it builds; the classic
  # Mach-O bundle signature, matching CFBundlePackageType/CFBundleSignature in
  # the plist above.
  file(GENERATE
    OUTPUT "${JS80P_DIST_DIR}/js80p.vst3/Contents/PkgInfo"
    CONTENT "BNDL????\n")

  # ── Resources/Snapshots/<class-id>_snapshot.png ────────────
  # Hosts show this as the plugin's thumbnail, and find it by the processor's
  # class ID: the 32 hex digits of Vst3Plugin::Processor::ID (src/plugin/vst3/
  # plugin.cpp:92-94), upper-case.
  #
  # NB this does NOT reproduce release.sh:487 byte for byte, and deliberately so.
  # get_vst3_snapshot_id greps the four FUID words out of plugin.cpp as text and
  # concatenates them, which drops the leading zeros of the first one --
  # 0x565354 contributes "565354" rather than "00565354" -- so upstream ships a
  # 30-character name where the host looks for a 32-character one, and the
  # thumbnail never resolves. Spelling the ID out here keeps that off this build.
  # It is cosmetic either way: the class ID the plugin reports, and therefore
  # every compatibility question, is unaffected by what this file is called.
  set(_js80p_vst3_class_id "00565354414D4A386A73383070000000")

  add_custom_command(TARGET JS80P_CompatVST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/js80p.png"
      "${JS80P_DIST_DIR}/js80p.vst3/Contents/Resources/Snapshots/${_js80p_vst3_class_id}_snapshot.png"
    VERBATIM)

  # ── Resources/moduleinfo.json ──────────────────────────────
  # The VST3 module manifest: what the factory exposes, without a host having to
  # load the binary to find out. release.sh:466 generates it with the same tool.
  #
  # The tool is a host, not part of the plugin: moduleinfo.cpp pulls in the SDK
  # unity build (via mach-o.cpp -> vst3.cpp, which is SDK sources only) and
  # dlopens the finished bundle at runtime, so it links no JS80P code and no
  # JUCE. It runs on the build machine, hence no ISA flags -- and note it has no
  # Windows branch at all (moduleinfo.cpp:35 keeps SMTG_OS_WINDOWS commented out
  # behind an #error), which is survivable only because Windows ships the
  # vst3_single form, and release.sh puts moduleinfo.json in bundles alone.
  add_executable(vst3_module_info_tool src/plugin/vst3/moduleinfo.cpp)

  target_include_directories(vst3_module_info_tool PRIVATE src lib lib/vst3sdk)

  target_compile_definitions(vst3_module_info_tool PRIVATE
    NOMINMAX
    JS80P_VST3_GUI_PLATFORM=${_js80p_gui_platform}
    $<IF:$<CONFIG:Debug>,DEVELOPMENT=1,RELEASE=1>)

  # vst3.cpp reaches the SDK's .mm sources, as for the plugin proper.
  set_source_files_properties(src/plugin/vst3/moduleinfo.cpp PROPERTIES
    COMPILE_OPTIONS "-fobjc-arc;-x;objective-c++")

  target_compile_options(vst3_module_info_tool PRIVATE
    -Wno-c99-designator
    -Wno-class-memaccess
    -Wno-error=deprecated-declarations
    -Wno-format
    -Wno-implicit-const-int-float-conversion
    -Wno-multichar
    -Wno-nontrivial-memcall
    -Wno-parentheses
    -Wno-pragmas
    -Wno-unknown-pragmas
    -Wno-unknown-warning-option
    -Wno-unused-but-set-variable
    -Wno-unused-value)

  # -framework AppKit and -pthread -ldl, as in make/macos-gpp.mk:194-199.
  target_link_libraries(vst3_module_info_tool PRIVATE "-framework AppKit" ${CMAKE_DL_LIBS})
  find_package(Threads REQUIRED)
  target_link_libraries(vst3_module_info_tool PRIVATE Threads::Threads)

  # POST_BUILD, so the bundle it inspects is complete: the binary has just been
  # linked, and Info.plist / PkgInfo were written at generate time. Redirection
  # needs a shell, which is unconditionally fine inside this APPLE-only branch.
  add_dependencies(JS80P_CompatVST3 vst3_module_info_tool)

  add_custom_command(TARGET JS80P_CompatVST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "${JS80P_DIST_DIR}/js80p.vst3/Contents/Resources"
    COMMAND sh -c
      "'$<TARGET_FILE:vst3_module_info_tool>' '${JS80P_DIST_DIR}/js80p.vst3' > '${JS80P_DIST_DIR}/js80p.vst3/Contents/Resources/moduleinfo.json'"
    VERBATIM)
else()
  set_target_properties(JS80P_CompatVST3 PROPERTIES
    OUTPUT_NAME "js80p"
    PREFIX ""
    SUFFIX ".vst3"
    LIBRARY_OUTPUT_DIRECTORY "$<1:${JS80P_DIST_DIR}>")
endif()

if(WIN32)
  # Exporting from a module makes link.exe emit an import library beside it;
  # nothing links against a VST3, so keep the pair out of the dist directory.
  set_target_properties(JS80P_CompatVST3 PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_BINARY_DIR}/implib>")
endif()

# Where the .vst3 lands, for CI and for anyone packaging by hand. Mirrors the
# Makefile's `show_vst3_dir` target.
add_custom_target(show_vst3_dir COMMAND ${CMAKE_COMMAND} -E echo "${JS80P_DIST_DIR}")
message(STATUS "JS80P VST3 output directory: ${JS80P_DIST_DIR}")

# ─────────────────────────────────────────────────────────────
# The Standalone — GUI development only, never released.
# ─────────────────────────────────────────────────────────────
juce_add_plugin(JS80P
  COMPANY_NAME                "Attila M. Magyar"
  BUNDLE_ID                   com.attilammagyar.js80p
  PLUGIN_MANUFACTURER_CODE    Amma
  PLUGIN_CODE                 Js8p
  FORMATS                     Standalone
  PRODUCT_NAME                "JS80P"
  IS_SYNTH                    TRUE
  NEEDS_MIDI_INPUT            TRUE
  NEEDS_MIDI_OUTPUT           FALSE
  IS_MIDI_EFFECT              FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(JS80P PRIVATE
  ${JS80P_CORE_SOURCES}
  ${JS80P_UI_SOURCES}
  src/plugin/juce/PluginProcessor.cpp
  src/plugin/juce/PluginEditor.cpp)

js80p_configure(JS80P)

# vst3_midi_mapping.cpp exists to hand the original plugin's ParamIDs to JUCE's
# VST3 wrapper. The shipped VST3 no longer goes through that wrapper — it is the
# original implementation — so the only build left that compiles this file is the
# Standalone, where JucePlugin_Build_VST3 is 0 and it collapses to the nullptr
# fallback that PluginProcessor's create_vst3_midi_mapping_extensions() expects.
if(TARGET JS80P_Standalone)
  target_sources(JS80P_Standalone PRIVATE src/plugin/juce/vst3_midi_mapping.cpp)
  target_include_directories(JS80P_Standalone PRIVATE src)
endif()

target_link_libraries(JS80P
  PRIVATE
    JS80PAssets
    juce::juce_audio_utils
    juce::juce_gui_extra
  PUBLIC
    juce::juce_recommended_config_flags)
