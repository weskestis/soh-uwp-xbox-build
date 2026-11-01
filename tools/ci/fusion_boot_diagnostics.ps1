$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot/../..").Path
Set-Location $root
$build = "$root/scratch/build-boot-diagnostics"
$evidence = "$root/scratch/boot-diagnostics-evidence"
New-Item -ItemType Directory -Force $evidence | Out-Null

cmake -S tools/uwp_boot_diagnostics_host -B $build -G "Visual Studio 17 2022" `
    -A "x64,version=10.0.19041.0" 2>&1 | Tee-Object "$evidence/configure.log"
if ($LASTEXITCODE -ne 0) { throw "Native diagnostic test configuration failed" }
cmake --build $build --config Release --parallel 2 2>&1 | Tee-Object "$evidence/build.log"
if ($LASTEXITCODE -ne 0) { throw "Native diagnostic test build failed" }
$framework = "${env:ProgramFiles(x86)}/Microsoft SDKs/Windows Kits/10/ExtensionSDKs/Microsoft.VCLibs/14.0/AppX/Retail/x64/Microsoft.VCLibs.x64.14.00.appx"
python tools/fusion_package_audit.py framework $framework
if ($LASTEXITCODE -ne 0) { throw "Native diagnostic framework validation failed" }
python tools/uwp_boot_diagnostics_host/verify.py "$build/Release/diagnostics_host.exe" `
    $framework "$evidence/cases" 2>&1 | Tee-Object "$evidence/test.log"
if ($LASTEXITCODE -ne 0) { throw "Native SEH and Store-CRT stream verification failed" }
