# Working in this repo

Read [CONTRIBUTING.md](CONTRIBUTING.md) before changing code. Its three rules (never change a plugin code or
parameter ID, say when the sound changes, nothing that can wait on the audio thread) matter more than anything here.

## How a change goes in

1. Branch from an up-to-date `main`: `git switch main; git pull; git switch -c <short-name>`. Never commit to `main`:
   it only takes pull requests, which need the windows, macos, linux and DCO checks to pass.
2. Commit with `git commit -s`. The DCO check fails without the `Signed-off-by` line. Messages follow the history:
   `<Plugin or area>: what changed, in plain words`, such as `C-Verb: compare the freeze value in the decay line's check`.
3. Build and run the tests (below) before pushing. A change that could alter the sound gets the null test from
   CONTRIBUTING, run against `main`.
4. Open the pull request with `gh pr create`, filling in every part of
   [the template](.github/pull_request_template.md). Answer "does it alter the sound?" honestly, and say what
   wasn't tested. Listening in a host is for a person to do, so ask for it rather than claim it.
5. Commit, push, open or merge a pull request, and tag releases only when asked.

## Build and test (Windows)

```
.\scripts\build.ps1                              # everything, Ninja + MSVC, into build/ninja-msvc-relwithdebinfo
.\scripts\build.ps1 -Target cdaw_plugins_tests   # just the tests
build\ninja-msvc-relwithdebinfo\tests\cdaw_plugins_tests_artefacts\RelWithDebInfo\cdaw_plugins_tests.exe
```

`build.ps1` sets up the Visual Studio environment itself, so run it rather than calling `cmake` directly. The first
full build takes about 20 minutes. Other systems use the presets in the README.

When adding parameters or a plugin, run the tests once with `CDAW_WRITE_PINNED_IDS=1` and check that the diff of
`tests/pinned-parameter-ids.txt` only adds lines.

## Code

- Match the code around it: Allman braces, four spaces, a space before call parentheses, comments that say why.
- Sound lives in `Shared/DSP`, with no UI. Views read the engine's atomics and compute nothing the audio depends on.
- CDAW uses this repo as a submodule and builds `Shared` into its own effects, so changes there reach it too.
- Docs and comments are short, plain sentences, like the README and CONTRIBUTING.
