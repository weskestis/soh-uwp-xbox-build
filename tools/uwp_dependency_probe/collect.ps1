# Collect PE evidence without loading any bundled code or building a package.
$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot/../..").Path
Set-Location $root
$report = Join-Path $root "scratch/runtime-probe-diagnostics/dependencies"
New-Item -ItemType Directory -Force $report | Out-Null
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$dumpbin = Get-ChildItem "$vs/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe" |
    Sort-Object FullName -Descending | Select-Object -First 1
if ($null -eq $dumpbin) { throw "MSVC dumpbin is unavailable" }
$sdk = "${env:ProgramFiles(x86)}/Microsoft SDKs/Windows Kits/10/ExtensionSDKs/Microsoft.VCLibs/14.0"
$appx = Join-Path $sdk "AppX/Retail/x64/Microsoft.VCLibs.x64.14.00.appx"
if (!(Test-Path $appx)) { throw "Selected retail x64 VCLibs framework is missing" }
Copy-Item "$sdk/SDKManifest.xml" "$report/SDKManifest.xml"
$framework = Join-Path $root "scratch/runtime-probe-framework"
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::ExtractToDirectory($appx, $framework)
Copy-Item "$framework/AppxManifest.xml" "$report/FrameworkAppxManifest.xml"
$inventory = @()
foreach ($group in @(
    @{ Name = "framework"; Root = $framework },
    @{ Name = "depot"; Root = "$root/scratch/uwp-dep/x64/bin" }
)) {
    $destination = Join-Path $report $group.Name
    New-Item -ItemType Directory -Force $destination | Out-Null
    foreach ($dll in Get-ChildItem $group.Root -Filter *.dll -File -Recurse | Sort-Object FullName) {
        foreach ($kind in @("imports", "exports", "headers")) {
            & $dumpbin.FullName "/$kind" $dll.FullName | Set-Content "$destination/$($dll.Name).$kind.txt"
            if ($LASTEXITCODE -ne 0) { throw "Cannot inspect $($dll.Name) $kind" }
        }
        if ($group.Name -eq "depot" -and $dll.Name -in @("libgallium_wgl.dll", "libuwp.dll", "SDL2.dll")) {
            $bytes = [IO.File]::ReadAllBytes($dll.FullName)
            $strings = @()
            foreach ($encoding in @([Text.Encoding]::ASCII, [Text.Encoding]::Unicode)) {
                $text = $encoding.GetString($bytes)
                $matches = [regex]::Matches($text, '[ -~]{0,80}(?:\.dll|DxcCreateInstance|Mesa)[ -~]{0,100}', 'IgnoreCase')
                foreach ($match in $matches) {
                    $strings += [ordered]@{ encoding = $encoding.WebName; index = $match.Index; text = $match.Value }
                }
            }
            $strings | ConvertTo-Json -Depth 3 | Set-Content "$destination/$($dll.Name).loader-strings.json"
        }
        $inventory += [ordered]@{
            group = $group.Name
            name = $dll.Name
            relative_path = [IO.Path]::GetRelativePath($group.Root, $dll.FullName)
            sha256 = (Get-FileHash $dll.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            file_version = $dll.VersionInfo.FileVersion
            product_version = $dll.VersionInfo.ProductVersion
            size = $dll.Length
        }
    }
}
[ordered]@{
    source_commit = $env:GITHUB_SHA
    depot_commit = (git -C scratch/uwp-dep rev-parse HEAD).Trim()
    framework_appx_sha256 = (Get-FileHash $appx -Algorithm SHA256).Hash.ToLowerInvariant()
    runtime_execution_verified = $false
    package_dependency_closure_verified = $false
    files = $inventory
} | ConvertTo-Json -Depth 6 | Set-Content "$report/inventory.json"
Write-Output "Collected $($inventory.Count) DLL inventories; no binaries were executed or packaged"
