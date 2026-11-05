# Contributing

Bug reports, sound reports from macOS and Linux, fixes and new ideas are all welcome. For anything big,
open an issue first so we can agree on the approach.

## Sign your commits

This project uses the [Developer Certificate of Origin](https://developercertificate.org). By signing off,
you state that you wrote the change or have the right to submit it under the project's licence
(AGPL-3.0-or-later). You keep your copyright.

Add a `Signed-off-by` line with `-s`, using your real name:

```
git commit -s -m "C-Delay: fix the echo map at very short times"
```

## Build and test

See [Build](README.md#build). Before opening a pull request, run `cdaw_plugins_tests` and try your change in a host.

CI builds every plugin on Windows, macOS and Linux, runs the tests, and runs
[pluginval](https://github.com/Tracktion/pluginval) on each VST3. A pull request that only changes docs skips
the build.

## Three rules that protect people's projects

**1. Never change a plugin's name or codes, or a parameter ID.** Hosts and saved projects find plugins and
settings by them, so changing one silently breaks every song that uses it. New parameters go at the end of the
list. `IdentityTests` fails if any of this changes. When you add parameters or a plugin, run the tests once
with `CDAW_WRITE_PINNED_IDS=1` to update `tests/pinned-parameter-ids.txt`, and check the diff only adds lines.

**2. Say so when a change alters the sound.** Existing mixes should keep sounding the same unless there's a
good reason. Say in the pull request what changes and for which settings.

To check a change that shouldn't alter the sound, such as an optimisation, use the null test. Set
`CDAW_DSP_GOLDEN_DIR` to an empty folder and run the tests on the code before your change, which writes every engine's
output there. Then run them again on your change: they fail if anything differs by more than -120 dB. Build both runs
the same way.

**3. Nothing that can wait on the audio thread.** No locks, allocation or file access in `process`.
Editors read what they show through atomics and lock-free FIFOs.

## Style

Match the code around it: Allman braces, four spaces, a space before call parentheses, plain comments that
say why. Colours come from `Shared/LookAndFeel/Palette.h`, knobs and panels from `Shared/UI`.

## Adding a plugin

Most plugins need four things; `VST3/CDelay` and `Shared/Delay` are a good model:

1. An engine in `Shared/DSP`, with its own tests in `tests/`.
2. A parameter list in `Shared/<Plugin>/<Plugin>Parameters.*`.
3. A view in `Shared/<Plugin>/<Plugin>View.*`, taking an `EffectModel` and the engine. Display code reads
   the engine's atomics, and doesn't compute anything the audio depends on.
4. `VST3/<Plugin>`: a `CMakeLists.txt` using `cdaw_add_effect_vst3` with a new four-letter `PLUGIN_CODE`
   (start it with a capital), and a processor built on `Common/SharedEffectProcessor.h`.

Then add its folder to the root `CMakeLists.txt`, its identity and parameters to `IdentityTests`, a scene to
`tests/SnapshotTests.cpp`, and a line to the README.

## Screenshots

To regenerate `docs/images`, run the tests with `CDAW_SNAPSHOTS=1` and `CDAW_SNAPSHOT_DIR` set to that folder.
