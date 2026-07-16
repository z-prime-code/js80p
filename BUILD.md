# Building JS80P (JUCE GUI)

Builds JS80P's **original** VST3 with the **JUCE GUI** on top. This is a CMake build
added alongside the legacy `Makefile`.

The VST3 is compiled from JS80P's own hand-rolled wrapper (`src/plugin/vst3`) against
the vendored Steinberg SDK (`lib/vst3sdk`) — the same sources the `Makefile` uses —
so its class IDs, ParamID scheme, state format, MIDI mapping and program list are the
original's *by construction*. JUCE is linked for the GUI alone (`juce_gui_extra`); the
build contains no `juce_audio_processors`, no `juce_add_plugin`, and none of JUCE's own
VST3 wrapper. The seam is `src/plugin/vst3/plugin-juce.cpp`.

A JUCE **Standalone** also builds. It is a GUI development aid — it lets you work on
the editor without a host — and is not a released artefact. It is deliberately *not*
built as a VST3: JUCE's wrapper cannot reproduce the original's parameter and state
layout, which is the entire point of the VST3 target.

## Status

- The `Makefile` build (FST/VST2 wrapper, original non-JUCE GUI, test suite) is
  unchanged.
- The editor (`src/ui/`, `src/gui/juce.cpp`) depends only on `JS80P::Synth`, so the
  same `juce::Component` is hosted both by the VST3's `IPlugView` and by the
  Standalone's `AudioProcessorEditor`. See `src/ui/editor_root.hpp`.
- Verified on Linux. Windows and macOS builds are not yet verified.
- The engine and patch format are never modified.

### New GUI — current features

- **Two pages** via SYNTH / EFFECTS tabs centered in the header.
  - *Synth page*: header + macro strip (macros 1-8 with per-macro MIDI-CC source) +
    OSC 1 / MIX / OSC 2 columns + a right-third scrollable Modulators panel.
    - Each oscillator column is one panel that also contains its two compact
      filters at the bottom (a 2-column filter-type glyph grid then CUTOFF / Q /
      GAIN). OSC 2 additionally exposes its distortion TYPE as a knob beside DIST.
      MIX and MODULATORS are title-less transparent sections; the MIX column shows
      two small signal-flow triangles (OSC 1 → mix knobs → OSC 2).
    - The MIX column stacks MIX / PM / FM / AM knobs over **POLY** (note handling),
      a combined **TUNING** selector, and **MODE**. TUNING and POLY apply to both
      oscillators / the whole synth and read their live value on load.
    - Two tiny pie-fill dot controls on each OSC title (oscillator inaccuracy /
      instability) and on each envelope card title (time / level inaccuracy); each
      fills clockwise from the bottom.
  - *Effects page*: recreates the original effects chain as panels of knobs, with
    the macro strip still shown on top. The top line holds Input, Vol 1, Dist 1/2,
    Filter 1/2, Vol 2 and OUT (Vol 3); Tape, Chorus, Echo and Reverb each get their
    own full-width single-line row. Continuous knobs are macro-modulation
    destinations (right-click → Modulate by); the Filter cutoffs use the 1 kHz-
    center scaling and the Dist / Filter type knobs display names, not indices.
    No LG toggles.
- **Modulation** (see `doc/z-gui.md` §5-6): knobs are modulation-aware (rotation =
  base, ring-band / badge drag = amount); envelope/LFO groups sharing a shape are one
  editor card; assigning an existing modulator clones a fresh pool slot; global
  sources (macros / CC / wheels / **Random**) route through an intermediate macro.
  Grouped copies are modulated together; unassigning garbage-collects orphaned pool
  slots. DTN/FIN pitch knobs snap base and amount to semitones (Ctrl = fine).
  Envelope cards expose SCL as a modulatable slider in the card title.

## Requirements

- **CMake ≥ 3.22** and **Git**. The first configure downloads **JUCE 8.0.14** (internet
  required, ~2–4 min, one time).
- A compiler:
  - **Linux:** GCC or Clang.
  - **macOS:** Apple Clang (Xcode).
  - **Windows:** **`clang-cl`**. MSVC `cl.exe` and MinGW/GCC are not supported.

---

## Linux

1. Install the toolchain and JUCE dependencies:

   ```bash
   sudo apt install build-essential cmake ninja-build \
     libasound2-dev libjack-jackd2-dev libfreetype-dev libfontconfig1-dev \
     libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev \
     libxrandr-dev libxrender-dev libglu1-mesa-dev mesa-common-dev libcurl4-openssl-dev
   ```

2. Configure and build:

   ```bash
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```

Output:
- `build/dist/js80p-<version>-linux-x86_64-auto-vst3_single/js80p.vst3` — the plugin
- `build/JS80P_artefacts/Release/Standalone/JS80P` — GUI development aid

The `.vst3` path reproduces the `Makefile`'s layout, down to the lower-case name;
`cmake --build build --target show_vst3_dir` prints the directory.

---

## Windows

1. Install **Build Tools for Visual Studio 2022**
   (<https://aka.ms/vs/17/release/vs_BuildTools.exe>). In the installer, select the
   **Desktop development with C++** workload, then on the **Individual components** tab
   ensure these are checked:
   - MSVC v143 - VS 2022 C++ x64/x86 build tools
   - Windows 11 SDK (or Windows 10 SDK)
   - C++ Clang Compiler for Windows
   - C++ CMake tools for Windows

2. Open **x64 Native Tools Command Prompt for VS 2022** (Start menu).

3. Configure and build (run from the repository root):

   ```bat
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
   cmake --build build
   ```

   If using Visual Studio community edition, you can also use:
   ```bat
   cmake -B build -G "Visual Studio 17 2022" -A x64 -T ClangCL
   cmake --build build --config Release
    ```

Output:
- `build\dist\js80p-<version>-windows-x86_64-auto-vst3_single\js80p.vst3` — the plugin
- `build\JS80P_artefacts\Release\Standalone\JS80P.exe` — GUI development aid

On Windows the module is itself named `js80p.vst3`, not `.dll` — the single-file
VST3 form the `Makefile` ships.

> Reconfiguring with a different compiler requires a clean build directory: delete
> `build\` first (`rmdir /s /q build`).

---

## macOS

1. Install the toolchain:

   ```bash
   xcode-select --install
   brew install cmake ninja
   ```

2. Configure and build:

   ```bash
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```

Output:
- `build/dist/js80p-<version>-macos--auto-vst3_single/js80p.vst3` — the plugin
- `build/JS80P_artefacts/Release/Standalone/JS80P.app` — GUI development aid

macOS is the one platform that gets a real bundle
(`js80p.vst3/Contents/MacOS/js80p` + `Info.plist`). The double dash is the
`Makefile`'s own naming: its macOS `SUFFIX` is empty.

---

## Installing the VST3

Copy `js80p.vst3` to the system VST3 directory:

- Linux: `~/.vst3/`
- Windows: `C:\Program Files\Common Files\VST3\`
- macOS: `~/Library/Audio/Plug-Ins/VST3/`

It replaces an existing JS80P install in place — same name, same class ID (below).

---

## Drop-in replacement for the original JS80P

The VST3 **is** the original implementation: `src/plugin/vst3/plugin.cpp` compiled
against `lib/vst3sdk`, exactly as the `Makefile` builds it. Only the editor is new.
So the things a host keys on are not reproduced or approximated — they are simply the
original's:

- **Class IDs.** Both of them, not just the component's. A host identifies a saved
  plugin by its VST3 *component* (audio-module) class ID; these come from
  `Vst3Plugin::Processor::ID` / `Vst3Plugin::Controller::ID` in `plugin.cpp` and are
  registered through the SDK's own `INLINE_UID`, so the platform-specific
  `COM_COMPATIBLE` byte order is handled by the SDK rather than by a hard-coded hex
  string. Verify a build by dumping the factory out of the module itself:
  ```
  component:  00565354414D4A386A73383070000000   .VSTAMJ8js80p...
  controller: 00565345414D4A386A73383070000000   .VSEAMJ8js80p...
  ```
- **Parameters.** The original `ParamID` scheme, including the channel/type
  bit-packing (`PARAM_CHANNEL_MASK` / `PARAM_TYPE_BITS` in `plugin.hpp`), with no
  extra parameters injected into the layout, so automation lanes in old projects land
  on the same targets.
- **State.** `Processor::getState()` / `setState()` write and read the raw
  `Serializer` patch, unchanged and unwrapped.
- **MIDI.** `Controller::getMidiControllerAssignment()` — the original `IMidiMapping`
  with the original ParamIDs, in every host, with no emulation layer in between.
- **Programs.** The original program list (`kCtrlProgramChange`) and `Bank`.

### Why not JUCE's VST3 wrapper?

This is the reason the shipped VST3 does not go through `juce_add_plugin`, and why
the Standalone is the only JUCE-wrapped target left. JUCE's wrapper can be pushed
partway there — an earlier revision of this port forced the component class ID with
`JUCE_VST3_COMPONENT_CLASS` and re-exposed the legacy MIDI map through
`VST3ClientExtensions` — but not all the way:

- It cannot set the **controller** class ID, only the component's.
- `JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS` had to stay enabled, because hosts that
  query `IMidiMapping` *before* connecting the component and controller (Bitwig) cache
  the wrapper's own mapping and never see the legacy one. Enabling it injects hidden
  MIDI-CC parameters into the layout, which shifts what the host sees; disabling it
  meant those hosts got no CC, pitch-bend or aftertouch at all.

Hosting the JUCE GUI on the original wrapper removes the whole class of problem
instead of trading one symptom for another. `src/plugin/juce/vst3_midi_mapping.cpp`
survives only because the Standalone links it as a `nullptr` fallback.

---

## Debug builds & leak detection (JUCE_LEAK_DETECTOR)

Every JUCE class (and anything using `JUCE_LEAK_DETECTOR` / `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR`, which includes all of the GUI's `juce::Component`s) keeps a per-class instance counter. When the last static destructor runs at shutdown, any class with a non-zero count triggers an assertion:

```
*** Leaked objects detected: N instance(s) of class Foo ***
```

This is the cheapest first pass for leaks in the new GUI — it needs no external tooling, only a **Debug build**. The counter is compiled in whenever `JUCE_CHECK_MEMORY_LEAKS` is set, which JUCE defaults to `1` in debug builds; a `Release` build compiles it out entirely, so you must build `Debug`. Use a separate build directory so it doesn't clobber your Release artefacts.

**Linux / macOS:**

```bash
cmake -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
# run the Standalone; the assert fires on exit and aborts under a debugger
build-debug/JS80P_artefacts/Debug/Standalone/JS80P     # Linux
# build-debug/JS80P_artefacts/Debug/Standalone/JS80P.app/Contents/MacOS/JS80P   # macOS
```

Run it under `gdb`/`lldb` (`gdb --args …`, then `run`) so the leak assertion breaks with a backtrace pinpointing the leaked class rather than just aborting. On Linux the Standalone can be exercised headlessly under `Xvfb :99` (`DISPLAY=:99 …`); open and close the editor / switch pages so GUI components are created and destroyed before shutdown.

**Windows (Visual Studio Community):**

```bat
:: from "x64 Native Tools Command Prompt for VS 2022"
cmake -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
cmake --build build-debug
```

Launch `build-debug\JS80P_artefacts\Debug\Standalone\JS80P.exe` **under the VS debugger** (open the folder / exe in VS, or attach) so the assertion surfaces in the Output window with the leaked class name; running it standalone just pops the abort dialog. To catch leaks inside a *hosted* VST3, build the VST3 in Debug, load it in your DAW, and attach the VS debugger to the DAW process — the assert fires when the DAW unloads the plugin.

**Scope & caveats:**

- It only reports classes that carry the leak-detector macro (all `juce::` types and GUI components) — **not** raw `new`/`malloc`, `std::` containers, or DSP allocations. For those, and for per-frame allocation churn, use the allocation profilers below.
- It reports *that* a class leaked and *how many*, not the allocation stack. Pair it with ASan (Linux) or the VS Memory Usage snapshot diff (Windows) to find the offending site.
- A leak is only reported if the process shuts down cleanly; a crash or `_exit` skips the static destructors and the check.

---

## Verifying

Run [pluginval](https://github.com/Tracktion/pluginval) against the built VST3:

```bash
pluginval --strictness-level 5 --skip-gui-tests --validate \
  "$(cmake --build build --target show_vst3_dir | tail -1)/js80p.vst3"
```

`--skip-gui-tests` is required in headless environments (pluginval's editor-embedding
test crashes without a display server).
