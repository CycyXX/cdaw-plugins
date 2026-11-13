# CDAW plugins

Eleven open-source audio plugins with displays that show what the sound is doing: nine effects and two
synths. They started in [CDAW](https://github.com/CycyXX/cdaw), a free DAW, and work in any host that
loads VST3 (or Audio Units, on macOS).

| Effects | |
|---|---|
| **C-Comp** | Compressor: soft knee, sidechain, the transfer curve with a live dot, a scrolling level history |
| **C-Multicomp** | 4-band compressor: phase-aligned crossovers you drag on a live spectrum |
| **C-Esser** | De-esser: split or wideband, automatic threshold, listen to the band or to what it takes away |
| **C-Verb** | Reverb in eight types (Hall, Room, Plate, Shimmer, Reverse, Bloom, Gated, Cloud), 26 presets: separate decay for lows, mids and highs, ducking, freeze, cuts before and after, a map of how long each frequency rings |
| **C-Delay** | Stereo or ping-pong, free or in notes; drive and filters in the loop, tape wobble, ducking |
| **C-Chorus** | One to four voices, flanging, a view of the voices moving and the comb filter they make |
| **C-Phaser** | 2 to 12 stages, a sonogram of the sweep |
| **C-Filter** | Low, high, band pass or notch, an LFO and an envelope follower moving the cutoff |
| **C-Dist** | Distortion in six voicings after modern guitar amps, with a noise gate |

| Synths | |
|---|---|
| **C-Synth** | Wavetable synth for pads and atmospheres, 21 presets |
| **C-Lead** | Virtual analog synth for leads: hard sync, FM, a ladder filter, glide and vibrato, 22 presets |

![C-Dist](docs/images/c-dist.png)

![C-Synth](docs/images/c-synth.png)

![C-Lead](docs/images/c-lead.png)

| | |
|---|---|
| ![C-Comp](docs/images/c-comp.png) | ![C-Multicomp](docs/images/c-multicomp.png) |
| ![C-Verb](docs/images/c-verb.png) | ![C-Delay](docs/images/c-delay.png) |
| ![C-Chorus](docs/images/c-chorus.png) | ![C-Phaser](docs/images/c-phaser.png) |
| ![C-Filter](docs/images/c-filter.png) | ![C-Esser](docs/images/c-esser.png) |

## Install (to use the plugins)

The ready-made plugins are on the [Releases](https://github.com/CycyXX/cdaw-plugins/releases/latest) page.
You don't need a compiler or any other tools, just the zip for your system. (To change the plugins or try code
that isn't released yet, see [Build from source](#build-from-source-for-contributors) instead.)

Each plugin is a folder ending in `.vst3` (or `.component`, for Audio Units on macOS). Always copy the whole
folder, not just the files inside it.

### Windows

1. Download `cdaw-plugins-windows.zip` from the [latest release](https://github.com/CycyXX/cdaw-plugins/releases/latest).
2. Close your host (Cubase, CDAW, …). It keeps loaded plugins locked.
3. Right-click the zip, choose **Extract All…**, then **Extract**.
4. In the extracted `cdaw-plugins-windows` folder, open `VST3` and select the plugins you want
   (`C-Comp.vst3`, `C-Verb.vst3`, …). Press Ctrl+C.
5. Open `C:\Program Files\Common Files\VST3` in File Explorer. Create a folder named `CDAW` in it, open
   that folder, and press Ctrl+V. When Windows asks for administrator permission, click **Continue**.
6. Start your host. Cubase finds new plugins when it starts; in other hosts, rescan plugins. They're listed
   by name (C-Comp, C-Verb, …), and the maker is Cyrill Durrer.

To update, close your host, delete the old `.vst3` folders in `C:\Program Files\Common Files\VST3\CDAW`
and copy in the new ones. To uninstall, just delete them.

### macOS

1. Download `cdaw-plugins-macos.zip` from the [latest release](https://github.com/CycyXX/cdaw-plugins/releases/latest)
   and double-click it to unzip it.
2. Close your host.
3. In Finder, choose **Go › Go to Folder…** and enter `~/Library/Audio/Plug-Ins/VST3`. Copy the `.vst3`
   folders from the zip's `VST3` folder there.
4. For Logic Pro, GarageBand and other hosts that use Audio Units, go to `~/Library/Audio/Plug-Ins/Components`
   and copy in the `.component` folders from the zip's `AU` folder.
5. The plugins aren't signed with an Apple certificate yet, so macOS blocks them at first. Open Terminal and run
   these two commands, which allow the plugins you copied:
   ```
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/C-*.vst3
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/C-*.component
   ```
   (If you skipped step 4, run only the first one.)
6. Start your host and rescan plugins. Logic checks new Audio Units when it starts.

### Linux

1. Download `cdaw-plugins-linux.zip` from the [latest release](https://github.com/CycyXX/cdaw-plugins/releases/latest).
2. Unzip it and copy the plugins to `~/.vst3`:
   ```
   unzip cdaw-plugins-linux.zip
   mkdir -p ~/.vst3
   cp -r cdaw-plugins-linux/VST3/*.vst3 ~/.vst3/
   ```
3. Start your host and rescan plugins.

**Status by system:** the plugins are developed and listened to on Windows, in Cubase and in CDAW. The
macOS and Linux builds pass the same automated tests and [pluginval](https://github.com/Tracktion/pluginval),
but nobody has played them by ear yet. Reports are welcome.

Your own presets are kept in `CDAW/Presets/<plugin>` in your user settings folder (`%APPDATA%` on
Windows, `~/Library` on macOS, `~/.config` on Linux).

## Build from source (for contributors)

You only need this to change the plugins or to try code that isn't released yet. To just use them,
[install a release](#install-to-use-the-plugins).

### 1. Install the tools

You need git, CMake 3.25 or newer and a C++20 compiler.

**Windows:** open a terminal (Start, type `Terminal`) and run these. The second command downloads several GB
and asks for administrator permission. It installs Visual Studio's C++ build tools, which include CMake and Ninja.

```
winget install Git.Git
winget install Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.CMake.Project --includeRecommended"
```

If you already have Visual Studio 2022, add the **Desktop development with C++** workload and
**C++ CMake tools for Windows** in the Visual Studio Installer instead. Then close the terminal and open a new
one, so it finds the new tools.

**macOS:** install Apple's command line tools, then CMake (here with [Homebrew](https://brew.sh); the installer
from [cmake.org](https://cmake.org/download/) works too):

```
xcode-select --install
brew install cmake
```

**Linux (Debian, Ubuntu):** the compiler, CMake and git, plus the libraries JUCE needs:

```
sudo apt-get install build-essential cmake git libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev \
  libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libglu1-mesa-dev mesa-common-dev
```

### 2. Get the code

JUCE comes as a submodule, so clone with `--recurse-submodules`:

```
git clone --recurse-submodules --shallow-submodules https://github.com/CycyXX/cdaw-plugins.git
cd cdaw-plugins
```

On Windows, if the clone stops with "Filename too long", use a shorter folder or run
`git config --global core.longpaths true` and clone again. If you cloned without `--recurse-submodules`, run
`git submodule update --init --depth 1`.

### 3. Build and install

**Windows, with a double-click:** `Install-Plugins.bat` builds every plugin and installs them in
`C:\Program Files\Common Files\VST3\CDAW`, the same folder the release instructions use.

1. Close Cubase, CDAW and any other host. They keep loaded plugins locked.
2. In the `cdaw-plugins` folder, double-click `Install-Plugins.bat` (or run `.\scripts\install.ps1`). The first
   build takes about 20 minutes because each plugin compiles JUCE too; later builds are quicker.
3. The first time, Windows asks for administrator permission right away. Click **Yes**: it lets your account
   install in that one folder, so later runs never ask. If you click No, plugins installed for the first time go to
   `%LOCALAPPDATA%\Programs\Common\VST3\CDAW`, for your user only.
4. The window lists each plugin it installed and any warnings, such as not being on the latest `main`, then
   ends with `Done.` Press a key to close it, then start your host.

**Any system, by hand:** configure and build with the preset for your system, then copy the plugins to the
folders listed under [Install](#install-to-use-the-plugins).

| System | Configure and build | Plugins end up in |
|---|---|---|
| Windows | `cmake --preset windows` then `cmake --build --preset windows` | `build/windows/VST3/*/*_artefacts/Release/VST3` |
| macOS | `cmake --preset macos` then `cmake --build --preset macos -j8` | `build/macos/VST3/*/*_artefacts/Release/{VST3,AU}` |
| Linux | `cmake --preset linux` then `cmake --build --preset linux -j8` | `build/linux/VST3/*/*_artefacts/Release/VST3` |

To update later, run `git pull` and `git submodule update`, then build and install again.

### 4. Test

Run the tests with the `cdaw_plugins_tests` program, in `build/<preset>/tests/cdaw_plugins_tests_artefacts/`.
Before opening a pull request, read [CONTRIBUTING](CONTRIBUTING.md).

## How the code is laid out

| Folder | What |
|---|---|
| `Shared/DSP` | The engines: all the sound, with no host and no UI |
| `Shared/<Plugin>` | A plugin's parameters, its view and its presets |
| `Shared/UI`, `Shared/LookAndFeel`, `Shared/Common` | Knobs, widgets, colours and the parameter list type, used by every plugin |
| `VST3/<Plugin>` | The small part that's specific to being a plugin: the processor and its build target |
| `VST3/Common` | What most plugins share of that |
| `tests` | Tests of the engines and views, with no host |

The plugins share code on purpose: C-Synth and C-Lead run C-Chorus's, C-Delay's and C-Verb's engines
as their effects, and C-Lead has C-Dist's. CDAW builds the same `Shared` code into its built-in effects,
using this repo as a submodule.

## License

Copyright (C) 2026 Cyrill Durrer and contributors.

Free software under the [GNU Affero General Public License](LICENSE), version 3 or (at your option) any
later version. There is no warranty. Third-party code keeps its own licence: see [THIRD_PARTY](THIRD_PARTY.md).

VST is a registered trademark of Steinberg Media Technologies GmbH. Other product names are trademarks of
their owners.
