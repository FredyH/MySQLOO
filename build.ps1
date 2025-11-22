param(
    [string]$Preset = ""
)

# Determine preset based on argument or auto-detect
if ($Preset -eq "") {
    # Auto-detect architecture
    $Arch = $env:PROCESSOR_ARCHITECTURE
    if ($Arch -eq "AMD64" -or $Arch -eq "x64") {
        $Preset = "windows-x64-release"
    }
    else {
        $Preset = "windows-x86-release"
    }
}

Write-Host "Using preset: $Preset"

# Validate preset exists
$presetsContent = Get-Content -Path "CMakePresets.json" -Raw
if ($presetsContent -notmatch "`"name`":\s*`"$Preset`"") {
    Write-Error "ERROR: Preset '$Preset' not found in CMakePresets.json"
    Write-Host "Available Windows presets: windows-x86-release, windows-x64-release"
    exit 1
}

# Install vcpkg if not present
$vcpkgPath = "$env:USERPROFILE\vcpkg"
if (-not (Test-Path $vcpkgPath)) {
    Write-Host "vcpkg not found. Installing..."
    git clone https://github.com/Microsoft/vcpkg.git $vcpkgPath
    & "$vcpkgPath\bootstrap-vcpkg.bat"
}

# Determine build directory from preset
$buildDir = ($presetsContent | Select-String -Pattern "`"name`":\s*`"$Preset`"[\s\S]*?`"binaryDir`":\s*`"`\`${sourceDir}/([^`"]+)`"" |
    ForEach-Object { $_.Matches.Groups[1].Value })

# Clean any partial build artifacts
if ($buildDir -and (Test-Path $buildDir)) {
    Write-Host "Cleaning previous build artifacts in $buildDir..."
    Remove-Item -Path "$buildDir\CMakeCache.txt" -Force -ErrorAction SilentlyContinue
    Remove-Item -Path "$buildDir\CMakeFiles" -Recurse -Force -ErrorAction SilentlyContinue
}

$env:VCPKG_ROOT = $vcpkgPath

Write-Host ""
Write-Host "=== Building MySQLOO with preset: $Preset ==="
Write-Host "This may take several minutes on first run as vcpkg builds dependencies from source..."
Write-Host ""

cmake --preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build --preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "Build complete!"
