# MDA Plug-ins for JUCE — Complete Edition

All **39 MDA plug-ins** by Paul Kellett, rebuilt as modern cross-platform **VST3, AU, CLAP, and LV2** binaries using [JUCE 8](https://juce.com) and CMake.

Pre-built binaries for **macOS** (ARM + Intel), **Windows**, and **Linux** are available on the [Releases](../../releases) page.

This is a fork of [hollance/mda-plugins-juce](https://github.com/hollance/mda-plugins-juce), which ported 24 of the original plug-ins. This fork completes the collection and adds a production-oriented build system.

## What's different from the original project

### Complete plug-in coverage

The original project had 24 out of 39 plug-ins ported. This fork adds the remaining **15 plug-ins**, ported from the [original VST2 source code](https://sourceforge.net/projects/mda-vst/):

Combo, De-ess, DubDelay, Leslie, Looplex, MultiBand, RePsycho!, RoundPan, SpecMeter, TalkBox, ThruZero, Tracker, Transient, VocInput, Vocoder.

### CMake build system (no Projucer needed)

The original project required Projucer and a pre-installed copy of JUCE 7. This fork uses a single **CMakeLists.txt** that:

- Fetches JUCE 8 automatically via `FetchContent` — no manual JUCE installation required
- Builds all 39 plug-ins in one step
- Works on **macOS, Windows, and Linux** out of the box
- Supports **VST3**, **AU** (macOS), **CLAP**, and **LV2** formats
- **CLAP** support via [clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions), pinned to commit `e1f6789` — the latest tagged release (0.26.0) is not compatible with JUCE 8, so we pin to a known working commit on `main`

The original `.jucer` files are preserved for anyone who prefers the Projucer workflow.

### GitHub Actions CI with pre-built binaries

Every push triggers automated builds on all platforms. Tagged releases (`v*`) automatically publish ready-to-use binaries:

- **macOS ARM64** (Apple Silicon) and **x86_64** (Intel) — VST3, AU, CLAP, LV2
- **Windows x64** — VST3, CLAP, LV2
- **Linux x64** — VST3, CLAP, LV2

### VST2-compatible naming and IDs

Each plug-in is configured with the **original VST2 unique ID** (`PLUGIN_CODE`) and **exact original name** (`PLUGIN_NAME`), extracted from the [original source](https://sourceforge.net/projects/mda-vst/). This means:

- DAWs like **Reaper** and **Cubase** can automatically substitute these VST3s for the legacy VST2 versions
- Existing projects using mda VST2 plug-ins will find the correct replacements
- `JUCE_VST3_CAN_REPLACE_VST2` is enabled for all plug-ins

### JUCE 8 / macOS 15+ compatibility

The original project used JUCE 7, which fails to compile on macOS 15 (Sequoia) due to deprecated CoreGraphics APIs. This fork uses **JUCE 8.0.4**, which resolves these issues and builds natively on Apple Silicon.

## Quick start

### Requirements

- **CMake** 3.22+ (`brew install cmake` on macOS, or download from [cmake.org](https://cmake.org))
- A C++ compiler (Xcode Command Line Tools on macOS, Visual Studio on Windows, GCC/Clang on Linux)

### Build

```bash
git clone https://github.com/blastart/mda-plugins-juce.git
cd mda-plugins-juce
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

On Apple Silicon Macs, add `-DCMAKE_OSX_ARCHITECTURES=arm64` (or `arm64;x86_64` for universal binaries).

### Install

The built plug-ins are in `build/MDA*_artefacts/Release/`. Copy the plugin bundles to your system plug-in folders:

| Format | macOS | Windows | Linux |
|---|---|---|---|
| VST3 | `~/Library/Audio/Plug-Ins/VST3/` | `C:\Program Files\Common Files\VST3\` | `~/.vst3/` |
| AU | `~/Library/Audio/Plug-Ins/Components/` | — | — |
| CLAP | `~/Library/Audio/Plug-Ins/CLAP/` | `C:\Program Files\Common Files\CLAP\` | `~/.clap/` |
| LV2 | `~/Library/Audio/Plug-Ins/LV2/` | `C:\Program Files\Common Files\LV2\` | `~/.lv2/` |

Or install everything at once (macOS):

```bash
cp -R build/MDA*_artefacts/Release/VST3/*.vst3 ~/Library/Audio/Plug-Ins/VST3/
cp -R build/MDA*_artefacts/Release/AU/*.component ~/Library/Audio/Plug-Ins/Components/
cp -R build/MDA*_artefacts/Release/CLAP/*.clap ~/Library/Audio/Plug-Ins/CLAP/
cp -R build/MDA*_artefacts/Release/LV2/*.lv2 ~/Library/Audio/Plug-Ins/LV2/
```

## Plug-in list

### Effects

| Plug-in | Description | VST2 ID |
|---|---|---|
| Ambience | Small space reverberator | `mdaA` |
| Bandisto | Multi-band distortion | `mdaD` |
| BeatBox | Drum replacer | `mdaG` |
| Combo | Amp & speaker simulator | `mdaX` |
| De-ess | High frequency dynamics processor | `mdas` |
| Degrade | Low-quality digital sampling | `mdaC` |
| Delay | Simple stereo delay with feedback tone control | `mday` |
| Detune | Simple up/down pitch shifting thickener | `mdat` |
| Dither | Range of dither types including noise shaping | `mdad` |
| DubDelay | Delay with feedback saturation and modulation | `mdab` |
| Dynamics | Compressor / Limiter / Gate | `mdaN` |
| Envelope | Envelope follower and VCA | `mdae` |
| Image | Stereo image adjustment and M-S matrix | `mdaI` |
| Leslie | Rotary speaker simulator | `mdaH` |
| Limiter | Opto-electronic style limiter | `mdaL` |
| Loudness | Equal loudness contours for bass EQ and mix correction | `mdal` |
| MultiBand | Multi-band compressor with M-S processing | `mdaM` |
| Overdrive | Soft distortion | `mdaO` |
| RePsycho! | Drum loop pitch changer | `mdaY` |
| RezFilter | Resonant filter with LFO and envelope follower | `mdaF` |
| RingMod | Simple ring modulator | `mdaR` |
| RoundPan | 3D circular panner | `mdaP` |
| Shepard | Shepard tone generator | `mdah` |
| SpecMeter | Spectrum analyzer and level meter | `mda?` |
| Splitter | Frequency / level crossover for dynamic processing | `mda7` |
| Stereo | Add artificial width to a mono signal | `mdaS` |
| SubSynth | Several low frequency enhancement methods | `mdaB` |
| TalkBox | High resolution vocoder | `mda&` |
| TestTone | Signal generator with noise, impulses, and sweeps | `mdaT` |
| ThruZero | Classic tape-flanging simulation | `mdaZ` |
| Tracker | Pitch tracking oscillator / pitch tracking EQ | `mdaJ` |
| Transient | Transient shaper | `mdaK` |
| VocInput | Pitch tracking oscillator for vocoder carrier input | `mda3` |
| Vocoder | Switchable 8 or 16 band vocoder | `mdav` |

### Instruments

| Plug-in | Description | VST2 ID |
|---|---|---|
| DX10 | Simple FM synthesizer | `MDAx` |
| ePiano | Rhodes piano | `MDAe` |
| JX10 | 2-oscillator analog synthesizer | `MDAj` |
| Looplex | Live looping instrument | `MDA~` |
| Piano | Acoustic piano | `MDAp` |

## VST2 to VST3 migration

If you have existing DAW projects that use the original mda VST2 plug-ins (Windows `.dll` files), these VST3 builds are designed to be drop-in replacements:

1. Install the VST3 plug-ins to your system plug-in folder
2. Open your DAW project — the DAW should automatically find and load the VST3 versions
3. Parameter values and automation should transfer (the plug-in names and IDs match the originals)

This has been tested with **Reaper** but should work with any DAW that supports VST2-to-VST3 migration.

## How the code is structured

Each plug-in is in its own folder with just two source files: **PluginProcessor.h** and **.cpp**. There is no custom UI — all plug-ins use the default JUCE generic editor.

Key methods:

- `createParameterLayout()` — parameter definitions
- `prepareToPlay()` / `resetState()` — initialization and state clearing
- `update()` — reads current parameter values and computes derived coefficients
- `processBlock()` — the actual audio DSP

For a detailed walkthrough of the code structure and parameter conversion approach, see the original project's [documentation](https://github.com/hollance/mda-plugins-juce#how-the-code-is-structured).

## Credits

- **Paul Kellett** — original MDA VST2 plug-ins ([mda-vst.com](http://mda.smartelectronix.com))
- **Sophia Poirier** — Audio Unit conversions ([destroyfx.org](http://destroyfx.org))
- **Matthijs Hollemans** — original JUCE port of 24 plug-ins ([hollance/mda-plugins-juce](https://github.com/hollance/mda-plugins-juce))
- **This fork** — CMake build system, remaining 15 plug-in ports, VST2 ID mapping

## License

[MIT License](LICENSE.txt) — same as the original project. Note that JUCE has its own [licensing terms](https://juce.com/legal/juce-8-licence/).
