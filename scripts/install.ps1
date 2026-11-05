# Windows: builds every plugin from this checkout and installs the VST3s where Cubase and other hosts scan.
#   .\scripts\install.ps1          (or double-click Install-Plugins.bat)
# Close the hosts first: they keep loaded plugins locked. Exits with 1 if any plugin isn't the build just made.
#
# The folder needs admin rights, so Windows asks once, unless you have made the folder writable. If you decline,
# a plugin never installed there goes to the per-user VST3 folder instead; one with an older copy there is reported
# as not installed, because a second copy would show up twice in Cubase.
param(
    [string] $Preset = 'ninja-msvc-relwithdebinfo',
    [string] $Destination = "$env:ProgramFiles\Common Files\VST3\CDAW",
    [string] $UserDestination = "$env:LOCALAPPDATA\Programs\Common\VST3",
    [string] $CopyFrom = ''     # Only for this script, when it runs itself with admin rights.
)

function Copy-Bundles ([string] $from, [string] $to) {
    New-Item -ItemType Directory -Force $to -ErrorAction Stop | Out-Null
    foreach ($bundle in Get-ChildItem $from -Filter *.vst3) {
        $target = Join-Path $to $bundle.Name
        if (Test-Path $target) { Remove-Item $target -Recurse -Force -ErrorAction Stop }
        Copy-Item $bundle.FullName $to -Recurse -Force -ErrorAction Stop
    }
}

if ($CopyFrom) {
    try { Copy-Bundles $CopyFrom $Destination }
    catch {
        Write-Host "FAILED: $($_.Exception.Message) (close Cubase and CDAW, which keep loaded plugins locked, and try again)"
        Start-Sleep -Seconds 6
    }
    return
}

# Whether every file of the bundle at $from is also at $to, with the same contents.
function Test-SameBundle ([string] $from, [string] $to) {
    $from = (Resolve-Path $from).Path
    foreach ($file in Get-ChildItem $from -Recurse -File) {
        $other = Join-Path $to $file.FullName.Substring($from.Length)
        if (-not (Test-Path $other)) { return $false }
        if ((Get-FileHash $file.FullName).Hash -ne (Get-FileHash $other).Hash) { return $false }
    }
    return $true
}

function Test-Writable ([string] $folder) {
    try {
        New-Item -ItemType Directory -Force $folder -ErrorAction Stop | Out-Null
        $probe = Join-Path $folder '.install-probe'
        [IO.File]::WriteAllText($probe, '')
        Remove-Item $probe -ErrorAction Stop
        return $true
    } catch { return $false }
}

$root = Split-Path -Parent $PSScriptRoot

# Installing a branch to try it is fine, but it should never look like the latest version.
function Get-VersionWarnings {
    $warnings = @()
    $dirty = git -C $root status --porcelain --ignore-submodules
    if ($dirty) { $warnings += 'this checkout has uncommitted changes' }

    git -C $root fetch --quiet origin main 2>$null
    if ($LASTEXITCODE -ne 0) { $warnings += "couldn't reach GitHub to check for a newer version" }

    $head = git -C $root rev-parse HEAD
    $main = git -C $root rev-parse origin/main
    if ($head -ne $main) { $warnings += "this isn't the latest main (installed $($head.Substring(0, 7)), main is $($main.Substring(0, 7)))" }
    return $warnings
}

$warnings = @(Get-VersionWarnings)

# Every folder in VST3 is a plugin, except Common. Stop before installing anything if one doesn't build.
$folders = Get-ChildItem "$root\VST3" -Directory | Where-Object { $_.Name -ne 'Common' } | ForEach-Object { $_.Name }
$built = @()

foreach ($folder in $folders) {
    Write-Host "Building $folder"
    try { $log = & "$PSScriptRoot\build.ps1" -Preset $Preset -Target "${folder}_VST3" *>&1 }
    catch {
        $log | Select-Object -Last 30 | ForEach-Object { Write-Host $_ }
        Write-Host "$folder build FAILED: $($_.Exception.Message). Nothing was installed."
        exit 1
    }

    $bundle = Get-ChildItem "$root\build\$Preset\VST3\$folder\${folder}_artefacts\*\VST3\*.vst3" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime | Select-Object -Last 1
    if (-not $bundle) {
        Write-Host "$folder built, but its VST3 wasn't found. Nothing was installed."
        exit 1
    }
    $built += $bundle
}

# A clean copy of what's about to be installed, which is also what the install is checked against.
$staged = "$root\dist\VST3"
if (Test-Path $staged) { Remove-Item $staged -Recurse -Force }
New-Item -ItemType Directory -Force $staged | Out-Null
foreach ($bundle in $built) { Copy-Item $bundle.FullName $staged -Recurse -Force }
Get-ChildItem $staged -Recurse -Include *.ilk | Remove-Item -Force

if (Test-Writable $Destination) {
    try { Copy-Bundles $staged $Destination }
    catch { Write-Host "FAILED: $($_.Exception.Message)" }
} else {
    try {
        $arguments = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -CopyFrom `"$staged`" -Destination `"$Destination`""
        Start-Process powershell -ArgumentList $arguments -Verb RunAs -Wait -ErrorAction Stop
    } catch { Write-Host "The admin install didn't run: $($_.Exception.Message)" }
}

$stale = 0
foreach ($bundle in Get-ChildItem $staged -Filter *.vst3) {
    $name = $bundle.Name
    if (Test-Path "$Destination\$name") {
        if (-not (Test-SameBundle $bundle.FullName "$Destination\$name")) {
            Write-Host "NOT installed: $name in $Destination is not this build"
            $stale++
            continue
        }
        Write-Host "Installed $name in $Destination"
        # The same plugin in two scanned folders would show up twice in Cubase.
        Remove-Item "$UserDestination\$name" -Recurse -Force -ErrorAction SilentlyContinue
        Remove-Item "$UserDestination\CDAW\$name" -Recurse -Force -ErrorAction SilentlyContinue
    } else {
        New-Item -ItemType Directory -Force "$UserDestination\CDAW" | Out-Null
        Remove-Item "$UserDestination\CDAW\$name" -Recurse -Force -ErrorAction SilentlyContinue
        Copy-Item $bundle.FullName "$UserDestination\CDAW" -Recurse -Force
        Remove-Item "$UserDestination\$name" -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host "System folder not updated; installed $name for this user in $UserDestination\CDAW"
    }
}

foreach ($warning in $warnings) { Write-Host "WARNING: $warning" }

if ($stale -gt 0) {
    Write-Host "$stale plugin(s) not installed: hosts still load the older copies. Close Cubase and CDAW, run this again and approve the admin prompt."
    exit 1
}

Write-Host 'Done. Rescan plugins in your host, or restart it.'
