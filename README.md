# Horn Delay (VST3)

**Horn Delay** is a stereo VST3 delay that gives you the classic 90s hip-hop horn-stab delay without setup: short, filtered, slightly crunchy throws locked to your DAW's tempo. Put it on a horn or brass sample, pick a note division, and the repeats come out band-limited and lo-fi, like a sampler from that era.

Built with C++17, [JUCE 8](https://juce.com) and CMake. JUCE downloads automatically when you configure, so all you need is **CMake** and a **C++ compiler**.

---

## Features

| Section | Controls | What it does |
|---|---|---|
| **Time** | Tempo Sync (on by default), Division, Fallback BPM, Free Time, Throws, Duck | With Tempo Sync on, the delay time comes from the **DAW host BPM** (via `AudioPlayHead`). Divisions: 1/16, 1/8 triplet, **1/8**, **1/8 dotted**, 1/4 triplet, **1/4**, **3/8 (dotted 1/4)**, **1/2**, 1/2 dotted, 1 bar. If the host doesn't send a tempo, the **Fallback BPM** is used (default 90). Turn Tempo Sync off to use Free Time (1–2000 ms). **Throws** sets how many audible repeats you get (1–3 is the typical style) and works out the feedback for you, with a hard cap at 0.85 so the delay can't run away. **Duck** turns the repeats down while the dry signal plays, so stabs stay punchy. |
| **Horn Tone** | Low Cut, High Cut, Horn Freq, Honk | Highpass and lowpass filters sit inside the feedback loop, so each repeat gets thinner and darker. A resonant bell in the horn range (400 Hz–3 kHz) adds the brassy, nasal character. **Honk** sets the boost (up to +12 dB) and how narrow it is. |
| **Lo-Fi** | Drive, Bits, Rate | Asymmetric tape/console-style saturation, then a bit crusher (4–16 bit) and sample-rate reduction (defaults to 26.04 kHz, the SP-1200's rate, for MPC/SP-style grit). These only affect the wet signal. |
| **Output** | Ping-Pong, Width, Mix, Output | Stereo ping-pong, mid/side width for the repeats (0–200 %), dry/wet mix and output gain. |

- Every parameter goes through `AudioProcessorValueTreeState`, so you can automate it in the DAW and it saves and recalls with your project.
- Parameter changes are smoothed. When the delay time changes (new division, tempo change, Free Time knob), the plugin crossfades between two read heads instead of jumping, so there are no clicks or zipper noise.
- Accepts stereo or mono input (mono in, stereo out works too).

### Built-in presets
These show up in the plugin's preset menu and in your DAW's program list:

1. **Classic Throw**: 1/4 note, 2 throws, the default settings
2. **Pete Rock Stab**: dotted 1/8, honky mids, 12-bit SP-1200 grit
3. **Bad Boy Brass**: 1/4 note ping-pong, wider and brighter, more drive
4. **Dusty Throw**: 3/8, dark and band-limited, 10-bit at 22 kHz
5. **Large Pro Slap**: a single 1/8 slap echo
6. **Ping-Pong Horns**: dotted 1/8 ping-pong, 4 throws, wide
7. **Half-Time Fade**: 1/2 note throws that fade out

---

## Building

### Requirements
- **CMake 3.22 or newer**: <https://cmake.org/download/>
- **Git**: CMake uses it to download JUCE
- **Windows:** Visual Studio 2022 or 2026 (Community is fine) with the *Desktop development with C++* workload
- **macOS:** Xcode 14 or newer (run `xcode-select --install` once for the command-line tools)

The first configure downloads JUCE (about 100 MB), so it needs an internet connection.

### Windows (Visual Studio / MSVC)
Open a **Developer PowerShell for VS 2022** (a normal terminal also works if CMake is on your PATH), go to the project folder and run:

```powershell
cmake -B build -A x64
cmake --build build --config Release --target HornDelay_VST3
```

CMake picks up whichever Visual Studio you have installed (2022 or 2026). To force one, add `-G "Visual Studio 17 2022"` or `-G "Visual Studio 18 2026"`.

The plugin ends up here:
```
build\HornDelay_artefacts\Release\VST3\Horn Delay.vst3
```

If you prefer the Visual Studio IDE, open `build\HornDelay.sln`, set the configuration to **Release**, and build the `HornDelay_VST3` project.

### macOS (Xcode / clang)
```bash
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release --target HornDelay_VST3
```
(You can use `-G "Unix Makefiles"` or `-G Ninja` instead of Xcode if you like. The architectures flag builds a Universal binary for Apple Silicon and Intel.)

The plugin ends up here:
```
build/HornDelay_artefacts/Release/VST3/Horn Delay.vst3
```

Plugins you build yourself are trusted locally. If you copy the plugin to another Mac, ad-hoc sign it first and remove the quarantine flag:
```bash
codesign --force --deep --sign - "Horn Delay.vst3"
xattr -dr com.apple.quarantine "Horn Delay.vst3"
```

### Linux (optional)
```bash
sudo apt install cmake ninja-build libasound2-dev libx11-dev libxrandr-dev libxinerama-dev \
                 libxcursor-dev libxcomposite-dev libfreetype6-dev libfontconfig1-dev
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target HornDelay_VST3
```

### Standalone app (for quick testing without a DAW)
Build the target `HornDelay_Standalone`. The app is in `build/HornDelay_artefacts/Release/Standalone/`. A standalone app has no host tempo, so it runs at the **Fallback BPM**.

---

## Installing the .vst3

Copy the whole **`Horn Delay.vst3`** item (on Windows it's a folder, on macOS it's a bundle) to your system's VST3 folder:

| OS | Folder |
|---|---|
| **Windows** | `C:\Program Files\Common Files\VST3\` |
| **macOS (all users)** | `/Library/Audio/Plug-Ins/VST3/` |
| **macOS (just you)** | `~/Library/Audio/Plug-Ins/VST3/` |
| **Linux** | `~/.vst3/` |

Then restart your DAW or rescan plugins. Horn Delay is listed under **BoomBapAudio → Horn Delay** in the *Fx / Delay* category.

> **Logic Pro** doesn't load VST3 plugins (it only supports AU). For Logic, add `AU` to `FORMATS` in `CMakeLists.txt` (`FORMATS VST3 AU Standalone`), rebuild, and copy the `.component` to `~/Library/Audio/Plug-Ins/Components/`.

---

## Building in the cloud (GitHub Actions)

`.github/workflows/build.yml` builds the VST3 on **Windows** (MSVC x64) and **macOS** (Universal, ad-hoc signed) whenever you push, open a pull request, or start it manually. It also builds on Linux and runs the offline DSP tests.

1. Push this folder to a GitHub repository.
2. Open the **Actions** tab, then **Build Horn Delay VST3**, then the latest run. You can also click **Run workflow** to start one.
3. When it finishes, download **HornDelay-VST3-Windows** and/or **HornDelay-VST3-macOS** from the run's *Artifacts* section, unzip, and copy to the VST3 folder listed above.

---

## Offline tests

There is a console test that runs audio through the processor without a DAW:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DHORN_DELAY_BUILD_TESTS=ON
cmake --build build --target HornDelayTests
./build/HornDelayTests_artefacts/Release/HornDelayTests      # Windows: build\HornDelayTests_artefacts\Release\HornDelayTests.exe
```

It checks:
- echo timing using the fallback tempo (90 BPM, 1/4 note) and a simulated host tempo (120 BPM, dotted 1/8)
- Free Time mode
- feedback stability at the most extreme settings, and that the tail decays afterwards
- that changing the division mid-signal doesn't click
- that presets load and the plugin state saves and recalls correctly

It also writes demo WAV files (synthetic horn stabs played through every preset) to `demo_renders/` in the current directory.

---

## Project layout
```
CMakeLists.txt              JUCE via FetchContent, juce_add_plugin (VST3 + Standalone)
Source/PluginProcessor.*    DSP, parameters, presets, state
Source/PluginEditor.*       Dark custom GUI (LookAndFeel, knobs, division selector)
tests/OfflineTest.cpp       Offline render and test executable
.github/workflows/build.yml CI builds for Windows and macOS, plus Linux tests
```

### Signal flow
```
in ─┬──────────────────────────────────────────────── dry ──┐
    └─► delay line ─► HP ─► LP ─┬─► drive ─► crush ─► horn bell ─► width ─► duck ─► wet ─► mix ─► output
            ▲                   │
            └── tanh × feedback ┘   (feedback set by Throws, capped at 0.85; ping-pong swaps L/R)
```

## Customising
- To change the company name, plugin codes or bundle ID, edit the `juce_add_plugin(...)` block in `CMakeLists.txt`. If you plan to distribute the plugin, set your own unique 4-character `PLUGIN_MANUFACTURER_CODE` and `PLUGIN_CODE`.
- Presets live in `HornDelayAudioProcessor::getPresets()` in `Source/PluginProcessor.cpp`.

## License
The Horn Delay source is yours to use. JUCE has its own licence (AGPLv3 or a commercial JUCE licence). Read <https://juce.com/legal/juce-8-licence/> before you distribute binaries. The VST3 SDK comes bundled with JUCE (this project uses JUCE 8.0.15). Steinberg moved the SDK to the MIT licence starting with VST3 SDK 3.8; older SDK releases were dual-licensed (GPLv3 or Steinberg's proprietary licence). Check which terms apply to your build before you distribute it.
