[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet("Dependencies", "Configure", "Compile", "Audit")]
    [string]$Command
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path "$PSScriptRoot/../..").Path
Set-Location $repoRoot
$diagnostics = Join-Path $repoRoot "scratch/fusion-mm-diagnostics"
New-Item -ItemType Directory -Force $diagnostics | Out-Null

function Assert-Exit([string]$Message) {
    if ($LASTEXITCODE -ne 0) { throw $Message }
}

switch ($Command) {
    "Dependencies" {
        $cache = Join-Path $repoRoot "scratch/vcpkg-cache"
        New-Item -ItemType Directory -Force $cache | Out-Null
        & "$repoRoot/scratch/vcpkg/bootstrap-vcpkg.bat" -disableMetrics
        Assert-Exit "vcpkg bootstrap failed"
        & "$repoRoot/scratch/vcpkg/vcpkg.exe" install --triplet x64-windows-static `
            "--binarysource=clear;files,$cache,readwrite" `
            libzip nlohmann-json tinyxml2 spdlog freetype `
            libogg libvorbis opus opusfile zlib bzip2 glew 2>&1 |
            Tee-Object "$diagnostics/dependencies.log"
        Assert-Exit "MM dependency installation failed"
    }
    "Configure" {
        cmake -S . -B scratch/build-fusion-mm -G "Visual Studio 17 2022" -A x64 `
            "-DCMAKE_SYSTEM_VERSION=10.0.19041.0" `
            "-DCMAKE_TOOLCHAIN_FILE=$repoRoot/scratch/vcpkg/scripts/buildsystems/vcpkg.cmake" `
            -DVCPKG_TARGET_TRIPLET=x64-windows-static `
            "-DZELDA3D_UWP_DEPS_ROOT=$repoRoot/scratch/uwp-dep" `
            -DZELDA3D_UWP_CORE=ON -DZELDA3D_BUILD_MM=ON `
            -DZELDA3D_SDL2_OPENGL=ON -DZELDA3D_BUILD_LAUNCHER=OFF `
            -DZELDA3D_ENABLE_ROM_EXTRACTION=OFF -DNON_PORTABLE=ON `
            -DLUS_BUILD_TESTS=OFF 2>&1 | Tee-Object "$diagnostics/configure.log"
        Assert-Exit "MM Xbox profile configure failed"
    }
    "Compile" {
        cmake --build scratch/build-fusion-mm --config Release --target uwp_release_logger_test --parallel 2 2>&1 |
            Tee-Object "$diagnostics/logger-build.log"
        Assert-Exit "UWP logger regression test compilation failed"
        & "$repoRoot/scratch/build-fusion-mm/uwp-checks/Release/uwp_release_logger_test.exe" "$diagnostics/early-error.log"
        if ($LASTEXITCODE -ne 23) { throw "UWP logger did not deliver the error synchronously" }
        if ((Get-Content "$diagnostics/early-error.log" -Raw) -notmatch '\[2ship\] cannot boot: missing archive') {
            throw "UWP boot error was lost on immediate process exit"
        }
        # Compile this revision. The old OoT runtime ZIP cannot prove MM builds.
        cmake --build scratch/build-fusion-mm --config Release --target mm_core --parallel 2 2>&1 |
            Tee-Object "$diagnostics/build.log"
        Assert-Exit "MM core compilation failed"
    }
    "Audit" {
        $core = Join-Path $repoRoot "scratch/build-fusion-mm/mm/Release/mm_core.dll"
        if (!(Test-Path $core) -or (Get-Item $core).Length -eq 0) {
            throw "The expected MM core DLL is missing or empty"
        }
        $vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
        $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        $dumpbin = Get-ChildItem "$vs/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe" |
            Sort-Object FullName -Descending | Select-Object -First 1
        if ($null -eq $dumpbin) { throw "MSVC dumpbin is unavailable" }
        $exports = & $dumpbin.FullName /exports $core
        Assert-Exit "Cannot inspect MM exports"
        $exports | Set-Content "$diagnostics/exports.txt"
        if (($exports -join "`n") -notmatch '\bZelda3D_CoreEntry\b') {
            throw "MM DLL does not export Zelda3D_CoreEntry"
        }
        $imports = & $dumpbin.FullName /dependents $core
        Assert-Exit "Cannot inspect MM dependencies"
        $imports | Set-Content "$diagnostics/dependents.txt"
        if (($imports -join "`n") -match '(?i)\bSDL3\.dll\b') {
            throw "MM Xbox profile still imports SDL3"
        }
        & $dumpbin.FullName /imports $core | Set-Content "$diagnostics/imports.txt"
        Assert-Exit "Cannot inspect MM imports"
        python tools/uwp_core_imports.py "$diagnostics/imports.txt" --output "$diagnostics/import-audit.json"
        Assert-Exit "MM DLL still contains known desktop callbacks"
        $sha = (git rev-parse HEAD).Trim()
        @("source_commit=$sha", "artifact=experimental MM core DLL", "xbox_runtime_verified=false", `
          "shared_engine_handoff=false", "not_an_installable_package=true", `
          "uwp_api_certified=false", "crt_compatibility_verified=false") |
            Set-Content "$diagnostics/BUILD.txt"
        Get-FileHash $core -Algorithm SHA256 | Format-List | Out-String |
            Set-Content "$diagnostics/SHA256.txt"
    }
}
