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

# Below normal priority, inherited by CMake, Ninja and the compilers: builds use every idle core
# but give way to whatever else you're working on.
(Get-Process -Id $PID).PriorityClass = 'BelowNormal'

if (-not ($env:VSCMD_ARG_TGT_ARCH -eq 'x64' -and (Get-Command cl -ErrorAction SilentlyContinue))) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "vswhere not found: install Visual Studio 2022 or its Build Tools, with C++." }

    $vs = & $vswhere -latest -products * -version "[17.0,19.0)" `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw "No Visual Studio with the C++ x64 tools found." }

    Import-Module (Join-Path $vs 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
    Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
}

foreach ($p in @("$env:ProgramFiles\CMake\bin", "$env:LOCALAPPDATA\Microsoft\WinGet\Links")) {
    if ((Test-Path $p) -and ($env:Path -notlike "*$p*")) { $env:Path = "$p;$env:Path" }
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
