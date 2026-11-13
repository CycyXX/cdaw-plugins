# Windows, for development: configures and builds inside a Visual Studio developer environment, with Ninja.
#   .\scripts\build.ps1                         # everything
#   .\scripts\build.ps1 -Target CLead_VST3
# (Or without Ninja: cmake --preset windows; cmake --build --preset windows)
param(
    [string] $Preset = 'ninja-msvc-relwithdebinfo',
    [string] $Target = ''
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

# Visual Studio's C++ build tools, with its CMake and Ninja, in one command.
$toolsHint = 'Install them with: winget install Microsoft.VisualStudio.2022.BuildTools --override ' +
    '"--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.CMake.Project --includeRecommended"'

# A clone without --recurse-submodules has an empty JUCE folder.
if (-not (Test-Path "$root\third_party\JUCE\CMakeLists.txt")) {
    $ErrorActionPreference = 'Continue'   # git reports progress on stderr, which install.ps1 captures
    git -C $root submodule update --init --depth 1
    $ErrorActionPreference = 'Stop'
    if (-not (Test-Path "$root\third_party\JUCE\CMakeLists.txt")) {
        throw "JUCE is missing and couldn't be downloaded: run 'git submodule update --init' in $root."
    }
}

# Below normal priority, inherited by CMake, Ninja and the compilers: builds use every idle core
# but give way to whatever else you're working on.
(Get-Process -Id $PID).PriorityClass = 'BelowNormal'

if (-not ($env:VSCMD_ARG_TGT_ARCH -eq 'x64' -and (Get-Command cl -ErrorAction SilentlyContinue))) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "Visual Studio's C++ build tools aren't installed. $toolsHint" }

    $vs = & $vswhere -latest -products * -version "[17.0,19.0)" `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw "No Visual Studio with the C++ x64 tools found. $toolsHint" }

    # Visual Studio's own setup scripts call vswhere by name, and print an error if it isn't on PATH.
    $env:Path = "$(Split-Path $vswhere);$env:Path"
    Import-Module (Join-Path $vs 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
    Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
}

foreach ($p in @("$env:ProgramFiles\CMake\bin", "$env:LOCALAPPDATA\Microsoft\WinGet\Links")) {
    if ((Test-Path $p) -and ($env:Path -notlike "*$p*")) { $env:Path = "$p;$env:Path" }
}
$tools = @('cmake')
if ($Preset -like 'ninja*') { $tools += 'ninja' }
foreach ($tool in $tools) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "$tool not found: add 'C++ CMake tools for Windows' in the Visual Studio Installer. $toolsHint"
    }
}

Push-Location $root
# Native tools write warnings to stderr; judge success by exit codes only.
$ErrorActionPreference = 'Continue'
try {
    cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)" }

    $buildArgs = @('--build', '--preset', $Preset)
    if ($Target) { $buildArgs += @('--target', $Target) }
    $buildArgs += @('--', '-k', '0')   # keep going: report every error in one pass
    cmake @buildArgs
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }
}
finally {
    Pop-Location
}
