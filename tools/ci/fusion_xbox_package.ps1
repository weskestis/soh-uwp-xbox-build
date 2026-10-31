# Build and sign only the Fusion wrapper; core binaries were separately verified.
$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot/../..").Path
Set-Location $root
$build = "$root/scratch/build-fusion-package"
$dist = "$root/scratch/dist/ZELDA-FUSION-XBOX"
$evidence = "$root/scratch/fusion-package-diagnostics"
New-Item -ItemType Directory -Force $evidence, "$dist/Dependencies/x64" | Out-Null

function Assert-Exit([string]$Message) {
    if ($LASTEXITCODE -ne 0) { throw $Message }
}

function Assert-Signature([string]$Tool, [string]$Package, $Certificate) {
    $signature = Get-AuthenticodeSignature -FilePath $Package
    if ($null -eq $signature.SignerCertificate -or
        $signature.SignerCertificate.Thumbprint -ne $Certificate.Thumbprint -or
        $signature.Status.ToString() -notin @("Valid", "NotTrusted", "UnknownError")) {
        throw "Fusion signature integrity or signer mismatch"
    }
    $verification = @(& $Tool verify /pa /v $Package 2>&1)
    $code = $LASTEXITCODE
    $verification | ForEach-Object { Write-Output $_ }
    if ($code -eq 0) { return }
    $text = $verification -join "`n"
    if ([regex]::Matches($text, '(?i)SignTool Error:').Count -ne 1 -or
        ($text -replace '\s+', ' ') -notmatch '(?i)root certificate.+not trusted') {
        throw "Signed Fusion package verification failed"
    }
    Write-Output "Package signature verified; self-signed root is intentionally not installed on the runner."
}

$sdk = "${env:ProgramFiles(x86)}/Windows Kits/10/bin/10.0.19041.0/x64"
$makeAppx = "$sdk/makeappx.exe"
$signTool = "$sdk/signtool.exe"
if (!(Test-Path $makeAppx) -or !(Test-Path $signTool)) { throw "Pinned SDK tools are missing" }
$framework = "${env:ProgramFiles(x86)}/Microsoft SDKs/Windows Kits/10/ExtensionSDKs/Microsoft.VCLibs/14.0/AppX/Retail/x64/Microsoft.VCLibs.x64.14.00.appx"
python tools/fusion_package_audit.py framework $framework
Assert-Exit "Store framework provenance failed"
& $signTool verify /pa /v $framework
Assert-Exit "Microsoft framework signature failed"
Copy-Item $framework "$dist/Dependencies/x64/Microsoft.VCLibs.x64.14.00.appx"

cmake -S uwp -B $build -G "Visual Studio 17 2022" -A "x64,version=10.0.19041.0" `
    "-DZELDA3D_UWP_RUNTIME_DIR=$root/scratch/fusion-runtime" `
    "-DZELDA3D_UWP_DEPS_ROOT=$root/scratch/uwp-dep" `
    -DZELDA3D_UWP_ALLOW_INCOMPLETE_RENDERER=ON `
    -DZELDA3D_UWP_REQUIRE_SIGNING=OFF `
    "-DZELDA3D_UWP_VERSION=$env:UWP_VERSION" 2>&1 | Tee-Object "$evidence/configure.log"
Assert-Exit "Fusion wrapper configuration failed"
cmake --build $build --config Release --target zelda3d-uwp --parallel 2 -- `
    /p:GenerateAppxPackageOnBuild=true /p:AppxPackageSigningEnabled=false `
    /p:UapAppxPackageBuildMode=SideloadOnly /p:AppxBundle=Never 2>&1 |
    Tee-Object "$evidence/build.log"
Assert-Exit "Fusion wrapper build failed"
$candidate = (& python tools/fusion_package_audit.py select $build).Trim()
Assert-Exit "No exact Fusion package was produced"
$unsigned = "$root/scratch/fusion-unsigned"
& $makeAppx unpack /p $candidate /d $unsigned /o
Assert-Exit "Cannot unpack generated Fusion AppX"
python tools/fusion_package_audit.py audit $unsigned
Assert-Exit "Unsigned Fusion payload audit failed"
$package = "$dist/ZeldaFusion_$($env:UWP_VERSION)_x64.msix"
# Preserve the SDK-generated package container and its resource/block metadata.
# Signing the existing container avoids treating footprint files as new payload.
Copy-Item $candidate $package

$pfx = "$root/scratch/fusion-signing.pfx"
$passwordText = [guid]::NewGuid().ToString("N")
$password = ConvertTo-SecureString $passwordText -AsPlainText -Force
$certificate = $null
try {
    $certificate = New-SelfSignedCertificate -Type Custom -Subject "CN=WesKestisZeldaFusion" `
        -KeyAlgorithm RSA -KeyLength 2048 -HashAlgorithm SHA256 -KeyUsage DigitalSignature `
        -KeyExportPolicy Exportable -CertStoreLocation Cert:\CurrentUser\My `
        -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3")
    Export-PfxCertificate -Cert $certificate -FilePath $pfx -Password $password | Out-Null
    Export-Certificate -Cert $certificate -FilePath "$dist/ZELDA-FUSION.cer" -Type CERT | Out-Null
    & $signTool sign /fd SHA256 /f $pfx /p $passwordText $package
    Assert-Exit "Fusion signing failed"
    Assert-Signature $signTool $package $certificate
    $signed = "$root/scratch/fusion-signed"
    & $makeAppx unpack /p $package /d $signed /o
    Assert-Exit "Cannot unpack signed Fusion MSIX"
    python tools/fusion_package_audit.py audit $signed
    Assert-Exit "Signed Fusion payload audit failed"
} finally {
    Remove-Item $pfx -Force -ErrorAction SilentlyContinue
    if ($null -ne $certificate) {
        Remove-Item "Cert:\CurrentUser\My\$($certificate.Thumbprint)" -Force -ErrorAction SilentlyContinue
    }
}
Copy-Item uwp/ANDROID-INSTALL.txt $dist
Copy-Item uwp/PRIVATE-DATA-NOT-INCLUDED.txt $dist
Copy-Item uwp/StartupSelections "$dist/StartupSelections" -Recurse
New-Item -ItemType Directory -Force "$dist/ThirdPartyNotices" | Out-Null
$notices = @("lic", "License", "LICENSE") | Where-Object { Test-Path "$root/scratch/uwp-dep/$_" }
if ($notices.Count -eq 0) { throw "Pinned Xbox dependency notices are missing" }
foreach ($name in $notices) {
    Copy-Item "$root/scratch/uwp-dep/$name" "$dist/ThirdPartyNotices/uwp-dep-$name" -Recurse
}
python tools/fusion_package_audit.py finish $dist
Assert-Exit "Fusion delivery manifest failed"
Write-Output "Signed diagnostic Fusion MSIX and required dependency ready; physical Xbox boot remains unverified."
