[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet("Dependencies", "Configure", "Compile", "Audit")]
    [string]$Command,
    [ValidateSet("mm", "soh")]
    [string]$Core = "mm"
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path "$PSScriptRoot/../..").Path
Set-Location $repoRoot
$diagnostics = Join-Path $repoRoot "scratch/fusion-$Core-diagnostics"
$build = Join-Path $repoRoot "scratch/build-fusion-$Core"
New-Item -ItemType Directory -Force $diagnostics | Out-Null

function Assert-Exit([string]$Message) {
    if ($LASTEXITCODE -ne 0) { throw $Message }
}

function Find-MsvcTool([string]$Name) {
    $vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $tool = Get-ChildItem "$vs/VC/Tools/MSVC/*/bin/Hostx64/x64/$Name.exe" |
        Sort-Object FullName -Descending | Select-Object -First 1
    if ($null -eq $tool) { throw "MSVC $Name is unavailable" }
    return $tool.FullName
}

switch ($Command) {
    "Dependencies" {
        $cache = Join-Path $repoRoot "scratch/vcpkg-cache"
        New-Item -ItemType Directory -Force $cache | Out-Null
        & "$repoRoot/scratch/vcpkg/bootstrap-vcpkg.bat" -disableMetrics
        Assert-Exit "vcpkg bootstrap failed"
        & "$repoRoot/scratch/vcpkg/vcpkg.exe" install --triplet x64-uwp-static-md `
            "--overlay-ports=$repoRoot/cmake/vcpkg-uwp-ports" `
            "--binarysource=clear;files,$cache,readwrite" `
            libzip nlohmann-json tinyxml2 spdlog freetype `
            libogg libvorbis opus opusfile zlib bzip2 spdlog:x64-windows-static-md 2>&1 |
            Tee-Object "$diagnostics/dependencies.log"
        Assert-Exit "Store dependency installation failed"
        $fontOptions = "$repoRoot/scratch/vcpkg/installed/x64-uwp-static-md/include/freetype/config/ftoption.h"
        if (!(Test-Path $fontOptions) -or
            (Get-Content $fontOptions | Select-String '^\s*#\s*define\s+FT_CONFIG_OPTION_ENVIRONMENT_PROPERTIES\b')) {
            throw "FreeType Store configuration still enables unsupported CRT environment access"
        }
        $dumpbin = Find-MsvcTool "dumpbin"
        $lib = Find-MsvcTool "lib"
        & $dumpbin /exports "$repoRoot/scratch/uwp-dep/x64/bin/opengl32.dll" |
            Set-Content "$diagnostics/mesa-exports.txt"
        Assert-Exit "Cannot inspect pinned Mesa exports"
        python tools/uwp_store_build.py mesa-def "$diagnostics/mesa-exports.txt" "$diagnostics/mesa.def"
        Assert-Exit "Pinned Mesa export validation failed"
        & $lib /nologo /machine:x64 "/def:$diagnostics/mesa.def" `
            "/out:$repoRoot/scratch/uwp-dep/x64/lib/fusion-mesa-opengl32.lib"
        Assert-Exit "Cannot generate pinned Mesa import library"
    }
    "Configure" {
        cmake -S . -B $build -G "Visual Studio 17 2022" -A "x64,version=10.0.19041.0" `
            -DCMAKE_SYSTEM_NAME=Windows "-DCMAKE_SYSTEM_VERSION=10.0.19041.0" `
            "-DCMAKE_TOOLCHAIN_FILE=$repoRoot/scratch/vcpkg/scripts/buildsystems/vcpkg.cmake" `
            -DVCPKG_TARGET_TRIPLET=x64-uwp-static-md -DVCPKG_APPLOCAL_DEPS=OFF `
            "-DZELDA3D_UWP_DEPS_ROOT=$repoRoot/scratch/uwp-dep" `
            -DZELDA3D_UWP_CORE=ON -DZELDA3D_BUILD_MM=ON `
            -DZELDA3D_SDL2_OPENGL=ON -DZELDA3D_BUILD_LAUNCHER=OFF `
            -DZELDA3D_ENABLE_ROM_EXTRACTION=OFF -DNON_PORTABLE=ON `
            -DLUS_BUILD_TESTS=OFF 2>&1 | Tee-Object "$diagnostics/configure.log"
        Assert-Exit "Fusion Store core configure failed"
        Copy-Item "$build/uwp-store-targets.txt" "$diagnostics/"
        # Preserve small generated project files, not private assets or objects.
        foreach ($project in Get-Content "$build/uwp-store-targets.txt") {
            Copy-Item $project "$diagnostics/"
        }
        python tools/uwp_store_build.py projects $build 2>&1 | Tee-Object "$diagnostics/project-audit.txt"
        Assert-Exit "Generated projects did not consistently select the native Store runtime"
    }
    "Compile" {
        cmake -S tools/uwp_logger_host -B scratch/build-logger-host -G "Visual Studio 17 2022" -A x64 `
            "-DCMAKE_TOOLCHAIN_FILE=$repoRoot/scratch/vcpkg/scripts/buildsystems/vcpkg.cmake" `
            -DVCPKG_TARGET_TRIPLET=x64-windows-static-md 2>&1 | Tee-Object "$diagnostics/logger-build.log"
        Assert-Exit "Logger host configure failed"
        cmake --build scratch/build-logger-host --config Release --parallel 2 2>&1 |
            Tee-Object -Append "$diagnostics/logger-build.log"
        Assert-Exit "Logger host compilation failed"
        & "$repoRoot/scratch/build-logger-host/Release/uwp_release_logger_test.exe" "$diagnostics/early-error.log"
        if ($LASTEXITCODE -ne 23) { throw "UWP logger did not deliver the error synchronously" }
        if ((Get-Content "$diagnostics/early-error.log" -Raw) -notmatch '\[2ship\] cannot boot: missing archive') {
            throw "UWP boot error was lost on immediate process exit"
        }
        cmake --build $build --config Release --target "${Core}_core" --parallel 2 2>&1 |
            Tee-Object "$diagnostics/build.log"
        Assert-Exit "$Core Store core compilation failed"
    }
    "Audit" {
        $corePath = if ($Core -eq "mm") { "$build/mm/Release/mm_core.dll" } else { "$repoRoot/x64/Release/soh_core.dll" }
        if (!(Test-Path $corePath) -or (Get-Item $corePath).Length -eq 0) {
            throw "The expected $Core core DLL is missing or empty"
        }
        $dumpbin = Find-MsvcTool "dumpbin"
        foreach ($report in @("exports", "imports", "headers", "dependents")) {
            & $dumpbin "/$report" $corePath | Set-Content "$diagnostics/$report.txt"
            Assert-Exit "Cannot inspect $Core $report"
        }
        python tools/uwp_store_build.py core $diagnostics
        Assert-Exit "$Core Store runtime/import audit failed"
        $sha = (git rev-parse HEAD).Trim()
        @("source_commit=$sha", "artifact=experimental $Core Store core DLL", "xbox_runtime_verified=false", `
          "shared_engine_handoff=false", "not_an_installable_package=true", `
          "uwp_api_certified=false", "store_crt_link_verified=true") |
            Set-Content "$diagnostics/BUILD.txt"
        Get-FileHash $corePath -Algorithm SHA256 | Format-List | Out-String |
            Set-Content "$diagnostics/SHA256.txt"
        Copy-Item $corePath "$diagnostics/${Core}_core.dll"
    }
}
