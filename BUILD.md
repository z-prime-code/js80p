# Building JS80P (JUCE)

Builds the JUCE plugin (VST3 + Standalone). This is a CMake build added alongside the
legacy `Makefile`.

## Status

- Adds a JUCE VST3 + Standalone target; the legacy `Makefile` build is unchanged.
- Milestone 1 (original GUI running inside JUCE) is complete; verified on Linux.
  Windows and macOS builds are not yet verified.
- Milestone 2 (new simplified GUI, branch `feat/juce-gui`) is in progress. It lives
  under `src/ui/` and is reached via the editor's "Detailed view" toggle; the engine
  and patch format are never modified.

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
- `build/JS80P_artefacts/Release/VST3/JS80P.vst3`
- `build/JS80P_artefacts/Release/Standalone/JS80P`

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
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
     -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
   cmake --build build
   ```

   If using Visual Studio community edition, you can also use:
   ```bat
   cmake -B build -G "Visual Studio 17 2022" -A x64 -T ClangCL
   cmake --build build --config Release
    ```

Output:
- `build\JS80P_artefacts\Release\VST3\JS80P.vst3`
- `build\JS80P_artefacts\Release\Standalone\JS80P.exe`

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
- `build/JS80P_artefacts/Release/VST3/JS80P.vst3`
- `build/JS80P_artefacts/Release/Standalone/JS80P.app`

---

## Installing the VST3

Copy `JS80P.vst3` to the system VST3 directory:

- Linux: `~/.vst3/`
- Windows: `C:\Program Files\Common Files\VST3\`
- macOS: `~/Library/Audio/Plug-Ins/VST3/`

Or configure with `-DCOPY_PLUGIN_AFTER_BUILD=TRUE` to install on build.

---

## Drop-in replacement for the original JS80P

The JUCE VST3 is built to load transparently into DAW projects that used the
original (hand-rolled) JS80P VST3:

- **Component class ID matches.** A host identifies a saved plugin by its VST3
  *component* (audio-module) class ID. `CMakeLists.txt` forces JUCE to expose the
  original's ID via `JUCE_VST3_COMPONENT_CLASS` (legacy `FUID(0x565354, 0x414d4a38,
  0x6a733830, 0x70000000)` — bytes `\0VST AMJ8 js80 p`) instead of the ID JUCE
  would hash from the manufacturer/plugin codes. The value is set per platform
  because the Steinberg `INLINE_UID` byte order depends on `COM_COMPATIBLE`
  (Windows vs macOS/Linux). Verify after a build:
  ```bash
  grep -A1 '"Audio Module Class"' \
    build/JS80P_artefacts/Release/VST3/JS80P.vst3/Contents/Resources/moduleinfo.json
  # CID must be 00565354414D4A386A73383070000000 (macOS/Linux)
  #          or 545356004D41384A6A73383070000000 (Windows)
  ```
- **Patch/state is compatible both ways.** Both builds store the raw `Serializer`
  patch as the plugin state; JUCE appends its private block behind a leading
  `int64(0)`, which the legacy reader stops at, so old projects restore their patch
  unchanged and vice-versa.

- **MIDI-CC automation reconnects in every host.** The port re-exposes the legacy
  MIDI-controller parameter matrix — a pitch-bend, a channel-pressure, and one
  parameter per supported MIDI CC on each of the 16 channels (1152 params) — with
  the *exact* legacy VST3 ParamIDs `(channel<<8)|controller`. Because JUCE derives a
  VST3 ParamID by hashing the parameter's string id
  (`String::hashCode()&0x7fffffff`), each parameter's id is a single Unicode code
  point equal to the wanted ParamID, so JUCE hands the host that exact number — no
  host-side remapping needed (see `src/plugin/juce/midi_ctl_parameter.hpp`). On
  automation the value is translated into the same `Synth::control_change` /
  `pitch_wheel_change` / `channel_pressure` call the legacy processor made. Verify
  with the param dumper approach, or trust pluginval `--strictness-level 10` (its
  parameter/automation/fuzz tests exercise the whole matrix).

- **`IMidiMapping` matches the legacy plugin — with JUCE's emulation as the
  early-query fallback.** The port hands the VST3 host its own `IMidiMapping` (via
  `AudioProcessor::getVST3ClientExtensions()` → `queryIEditController`, see
  `src/plugin/juce/vst3_midi_mapping.{hpp,cpp}`) whose `getMidiControllerAssignment`
  returns the same `(channel<<8)|controller` ParamIDs as the parameter matrix — a
  line-for-line port of the legacy controller. This does two things the legacy plugin
  also did: (1) hosts recognise the 1152 params as MIDI-CC destinations and keep them
  out of the ordinary automation list instead of showing the whole grid; (2) live
  MIDI CC / pitch-bend / **channel pressure** arrive as parameter changes on those
  IDs and are forwarded to the synth by `dispatch_parameter_automation()` — the same
  single sink used for host automation, mirroring the legacy `process()`. VST3
  delivers CC / pitch-bend / channel-pressure *only* through `IMidiMapping` (they are
  not note-style events), so a host must get a working mapping or those messages are
  dropped. `dispatch_midi()` still handles raw MIDI as a harmless fallback for hosts
  that send it (REAPER sends legacy CC events).

  **Reach caveat (the Bitwig lesson):** JUCE's wrapper can only hand out the
  extension's `IMidiMapping` once the host has *connected* the component and the
  controller — the controller reaches `getVST3ClientExtensions()` through the
  `AudioProcessor` pointer it learns in that connection handshake. The wrapper's
  controller object also implements `IMidiMapping` itself, and a host that queries
  the interface *before* connecting caches that controller implementation forever
  (COM interfaces are queried once) — verified with a minimal probing host
  (`scripts/vst3_midi_mapping_probe.cpp`, build/run instructions in its header): a
  pre-connect `queryInterface` returns the wrapper's own mapping and keeps answering
  through it even after the handshake completes, while a post-connect query returns
  ours. REAPER queries late → gets the legacy mapping; **Bitwig queries early → gets
  the wrapper's own mapping**. That is why
  `JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS` must stay **enabled** (`=1`, the JUCE
  default): with it, the wrapper's always-available mapping routes every
  (channel, controller) — including pitch bend (129) and channel pressure /
  aftertouch (128) — to a hidden non-automatable "MIDI CC" parameter and converts
  host parameter changes back into MIDI events that land in `processBlock()`'s
  `MidiBuffer` → `dispatch_midi()`, the standard stock-JUCE-synth path that Bitwig
  is known to work with. With it *disabled* (as this port briefly did), the wrapper
  mapping is a dead `kResultFalse` stub, and early-querying hosts get **no CC /
  pitch-bend / aftertouch at all** and list the whole 1152-param grid as ordinary
  automation targets. Net per host: late-querying hosts behave exactly like the
  legacy plugin (mapped params hidden, CC moves the legacy params); early-querying
  hosts get MIDI through JUCE's emulation and additionally list the legacy params as
  automatable — a cosmetic deviation from the original, which showed none in Bitwig.

  This is **VST3-only code**: `vst3_midi_mapping.cpp` pulls in the Steinberg VST3 SDK,
  whose interface-IID symbols (`FUnknown::iid`, `IMidiMapping::iid`) are provided by
  JUCE's VST3 wrapper *in the same link* — in a non-VST3 link they end up undefined
  (MSVC/lld-link). So the file is compiled into **every plugin-format target**
  (never the shared-code target, which is compiled once with
  `JucePlugin_Build_<FORMAT>=1` for *all* enabled formats) and switches on JUCE's
  per-format `JucePlugin_Build_VST3` define — the same idiom JUCE's own
  plugin-client wrapper TUs use: the VST3 target compiles the real mapping, every
  other format compiles a `nullptr` fallback, and the two can't drift apart because
  they share one translation unit and one declaration. To keep the SDK out of the
  shared code, the implementation depends only on JUCE + the SDK: the shared
  `PluginProcessor` computes which controllers have a parameter
  (`collect_supported_midi_controllers()`) and passes that set into
  `create_vst3_midi_mapping_extensions()`.

Known gaps (not identity, but not yet at parity with the legacy VST3):

- **No VST2.** The legacy also shipped a VST2 (`uniqueID 'amj8'`); JUCE cannot build
  VST2, so VST2-based projects are not covered. Keep the legacy `Makefile` VST2 for
  those.
- **Program-change & patch-changed params.** The legacy exposed a program-change
  (id 130) and a read-only "Patch Changed" (id 255) parameter. Programs work through
  JUCE's own program mechanism, so these remain cosmetic/host-convenience gaps rather
  than automation-reconnection gaps. (`IMidiMapping` itself is now implemented — see
  above.)

---

## Verifying

Run [pluginval](https://github.com/Tracktion/pluginval) against the built VST3:

```bash
pluginval --strictness-level 5 --skip-gui-tests --validate <path>/JS80P.vst3
```

`--skip-gui-tests` is required in headless environments (pluginval's editor-embedding
test crashes without a display server).
