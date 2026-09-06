[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet("FindCore", "ValidateCore", "BuildCore", "VerifyCore", "Package", "CoreDiagnostics", "PackageDiagnostics")]
    [string]$Command,
    [string]$CacheHit = "false"
)

$ErrorActionPreference = "Stop"

function Assert-LastExitCode([string]$Message) {
    if ($LASTEXITCODE -ne 0) { throw $Message }
}

function Assert-EphemeralPackageSignature(
    [string]$SignTool,
    [string]$PackagePath,
    [System.Security.Cryptography.X509Certificates.X509Certificate2]$Certificate
) {
    $signature = Get-AuthenticodeSignature -FilePath $PackagePath
    if ($null -eq $signature.SignerCertificate) {
        throw "Signed package has no readable signer certificate"
    }
    if ($signature.SignerCertificate.Thumbprint -ne $Certificate.Thumbprint) {
        throw "Signed package certificate does not match the ephemeral signing certificate"
    }

    $status = $signature.Status.ToString()
    if ($status -notin @("Valid", "NotTrusted", "UnknownError")) {
        throw "Signed package failed Authenticode integrity audit: $status - $($signature.StatusMessage)"
    }

    $verification = @(& $SignTool verify /pa /v $PackagePath 2>&1)
    $verifyExitCode = $LASTEXITCODE
    $verification | ForEach-Object { Write-Output $_ }
    if ($verifyExitCode -eq 0) { return }

    $verifyText = $verification -join "`n"
    $verifyErrors = [regex]::Matches($verifyText, '(?i)SignTool Error:')
    $verifyComparable = $verifyText -replace '\s+', ' '
    if ($verifyErrors.Count -ne 1 -or
        $verifyComparable -notmatch '(?i)root certificate.+not trusted') {
        throw "Signed AppX verification failed"
    }
    Write-Output "Signature hash and signer audited; the one-run certificate is intentionally not trusted by the runner."
}

function Write-Diagnostics([string]$LogName, [string]$Title, [string]$Pattern) {
    $log = Join-Path $env:RUNNER_TEMP $LogName
    if (!(Test-Path $log)) {
        Write-Output "::warning title=${Title}::The failure happened outside the captured build log."
        return
    }
    $diagnostics = Get-Content $log |
        Where-Object { $_ -match $Pattern } |
        ForEach-Object { $_.Trim() } |
        Select-Object -Unique -First 20
    if (@($diagnostics).Count -eq 0) {
        Write-Output "::warning title=${Title}::No compiler, linker, or AppX diagnostic was found."
    }
    foreach ($diagnostic in $diagnostics) {
        $safe = $diagnostic.Replace('%', '%25').Replace("`r", '%0D').Replace("`n", '%0A')
        Write-Output "::error title=${Title}::$safe"
    }
}

switch ($Command) {
    "FindCore" {
        $headers = @{
            Accept = "application/vnd.github+json"
            Authorization = "Bearer $env:GH_TOKEN"
            "X-GitHub-Api-Version" = "2022-11-28"
        }
        try {
            $response = Invoke-RestMethod `
                -Uri "https://api.github.com/repos/$env:GITHUB_REPOSITORY/actions/artifacts?per_page=100" `
                -Headers $headers
            $candidate = @($response.artifacts |
                Where-Object { !$_.expired -and $_.name -eq $env:CORE_RUNTIME_ARTIFACT } |
                Sort-Object { [DateTimeOffset]$_.created_at } -Descending |
                Select-Object -First 1)
            if ($candidate.Count -eq 0) {
                $candidate = @($response.artifacts |
                    Where-Object { !$_.expired -and $_.name -eq $env:LEGACY_CORE_RUNTIME_ARTIFACT } |
                    Sort-Object { [DateTimeOffset]$_.created_at } -Descending |
                    Select-Object -First 1)
            }
        } catch {
            Write-Output "::warning title=Core reuse lookup failed::$($_.Exception.Message)"
            $candidate = @()
        }
        if ($candidate.Count -eq 0) {
            "found=false" | Out-File $env:GITHUB_OUTPUT -Append -Encoding utf8
            return
        }
        "found=true" | Out-File $env:GITHUB_OUTPUT -Append -Encoding utf8
        "artifact_name=$($candidate[0].name)" | Out-File $env:GITHUB_OUTPUT -Append -Encoding utf8
        "run_id=$($candidate[0].workflow_run.id)" | Out-File $env:GITHUB_OUTPUT -Append -Encoding utf8
        Write-Output "Reusing core artifact from run $($candidate[0].workflow_run.id)."
    }

    "ValidateCore" {
        $archive = "$env:GITHUB_WORKSPACE\source\scratch\windows-core-runtime.zip"
        if ($CacheHit -ne "true") {
            $downloads = @(Get-ChildItem scratch/reused-core -Recurse -File -Filter windows-core-runtime.zip)
            if ($downloads.Count -ne 1) {
                Write-Output "::warning title=Core reuse rejected::Expected one archive, found $($downloads.Count)."
                "ready=false" | Out-File $env:GITHUB_OUTPUT -Append -Encoding utf8
                return
            }
            New-Item -ItemType Directory -Force (Split-Path $archive) | Out-Null
            Copy-Item $downloads[0].FullName $archive -Force
        }
        python tools/uwp_runtime_stage.py verify-core --archive $archive
        if ($LASTEXITCODE -ne 0) {
            Remove-Item $archive -Force -ErrorAction SilentlyContinue
            Write-Output "::warning title=Core reuse rejected::Archive validation failed; rebuilding."
            "ready=false" | Out-File $env:GITHUB_OUTPUT -Append -Encoding utf8
            return
        }
        "ready=true" | Out-File $env:GITHUB_OUTPUT -Append -Encoding utf8
    }

    "BuildCore" {
        & "$env:GITHUB_WORKSPACE\source\scratch\vcpkg\bootstrap-vcpkg.bat" -disableMetrics
        Assert-LastExitCode "vcpkg bootstrap failed"
        $vcpkg = "$env:GITHUB_WORKSPACE\source\scratch\vcpkg\vcpkg.exe"
        & $vcpkg install --triplet x64-windows-static `
            libzip nlohmann-json tinyxml2 spdlog freetype `
            libogg libvorbis opus opusfile zlib bzip2 glew
        Assert-LastExitCode "vcpkg core dependency install failed"

        cmake -S . -B scratch/build-uwp-core -G "Visual Studio 17 2022" -A x64 `
            "-DCMAKE_SYSTEM_VERSION=10.0.19041.0" `
            "-DCMAKE_TOOLCHAIN_FILE=$env:GITHUB_WORKSPACE\source\scratch\vcpkg\scripts\buildsystems\vcpkg.cmake" `
            -DVCPKG_TARGET_TRIPLET=x64-windows-static `
            -DZELDA3D_UWP_CORE=ON `
            "-DZELDA3D_UWP_DEPS_ROOT=$env:GITHUB_WORKSPACE\source\scratch\uwp-dep" `
            -DZELDA3D_SDL2_OPENGL=ON `
            -DZELDA3D_BUILD_MM=OFF `
            -DZELDA3D_BUILD_LAUNCHER=OFF `
            -DZELDA3D_ENABLE_ROM_EXTRACTION=OFF `
            -DNON_PORTABLE=ON `
            -DLUS_BUILD_TESTS=OFF
        Assert-LastExitCode "Windows core configure failed"

        $coreLog = Join-Path $env:RUNNER_TEMP "windows-core-build.log"
        cmake --build scratch/build-uwp-core --config Release --target soh_core --parallel 2 2>&1 |
            Tee-Object -FilePath $coreLog
        Assert-LastExitCode "Windows core compilation failed"

        python tools/uwp_runtime_stage.py stage-core `
            --source-root . `
            --stage-dir scratch/windows-core-runtime `
            --archive scratch/windows-core-runtime.zip
        Assert-LastExitCode "Windows core runtime staging failed"
    }

    "VerifyCore" {
        python tools/uwp_runtime_stage.py verify-core --archive scratch/windows-core-runtime.zip
        Assert-LastExitCode "Windows core archive validation failed"
    }

    "Package" {
        python tools/uwp_runtime_stage.py assemble `
            --archive scratch/core-artifact/windows-core-runtime.zip `
            --port-archive scratch/host-assets/soh.o2r `
            --deps-root scratch/uwp-dep `
            --runtime-dir scratch/uwp-runtime
        Assert-LastExitCode "UWP wrapper runtime assembly failed"

        cmake -S uwp -B scratch/build-uwp-wrapper -G "Visual Studio 17 2022" -A x64 `
            "-DZELDA3D_UWP_RUNTIME_DIR=$env:GITHUB_WORKSPACE\source\scratch\uwp-runtime" `
            "-DZELDA3D_UWP_DEPS_ROOT=$env:GITHUB_WORKSPACE\source\scratch\uwp-dep" `
            -DZELDA3D_UWP_ALLOW_INCOMPLETE_RENDERER=ON `
            -DZELDA3D_UWP_REQUIRE_SIGNING=OFF `
            "-DZELDA3D_UWP_VERSION=$env:UWP_VERSION"
        Assert-LastExitCode "UWP wrapper configure failed"

        $wrapperLog = Join-Path $env:RUNNER_TEMP "uwp-wrapper-build.log"
        cmake --build scratch/build-uwp-wrapper --config Release --target zelda3d-uwp --parallel 2 -- `
            /p:GenerateAppxPackageOnBuild=true `
            /p:AppxPackageSigningEnabled=false `
            /p:UapAppxPackageBuildMode=SideloadOnly `
            /p:AppxBundle=Never 2>&1 | Tee-Object -FilePath $wrapperLog
        Assert-LastExitCode "Unsigned AppX build failed"

        $wrapper = "$env:GITHUB_WORKSPACE\source\scratch\build-uwp-wrapper"
        $dist = "$env:GITHUB_WORKSPACE\source\scratch\dist\$env:PACKAGE_NAME"
        New-Item -ItemType Directory -Force "$dist\Dependencies" | Out-Null
        $allCandidates = @(Get-ChildItem $wrapper -Recurse -File |
            Where-Object {
                $_.Extension -in ".appx", ".msix" -and
                $_.FullName -notmatch '\\Dependencies\\'
            })
        $candidates = @($allCandidates |
            Where-Object { $_.FullName -match '\\AppPackages\\' } |
            Sort-Object Length -Descending)
        if ($candidates.Count -eq 0) { $candidates = @($allCandidates | Sort-Object Length -Descending) }
        if ($candidates.Count -eq 0) { throw "Visual Studio produced no AppX/MSIX" }
        $packageExtension = $candidates[0].Extension.ToLowerInvariant()
        $appPackage = "$dist\SOH-CURSOR-FPS-V3-OOT3D-FULL-x64$packageExtension"
        Copy-Item $candidates[0].FullName $appPackage
        Get-ChildItem $wrapper -Recurse -File |
            Where-Object {
                $_.Extension -in ".appx", ".msix" -and
                $_.FullName -match '\\Dependencies\\'
            } |
            ForEach-Object { Copy-Item $_.FullName "$dist\Dependencies" -Force }

        $sdkRoots = @(Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Directory |
            Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
            Sort-Object { [version]$_.Name } -Descending)
        $sdkBin = $sdkRoots | Where-Object {
            (Test-Path (Join-Path $_.FullName "x64\signtool.exe")) -and
            (Test-Path (Join-Path $_.FullName "x64\makeappx.exe"))
        } | Select-Object -First 1
        if ($null -eq $sdkBin) { throw "Windows SDK AppX tools are missing" }
        $signTool = Join-Path $sdkBin.FullName "x64\signtool.exe"
        $makeAppx = Join-Path $sdkBin.FullName "x64\makeappx.exe"

        $pfx = "$env:GITHUB_WORKSPACE\source\scratch\ephemeral-signing.pfx"
        $cer = "$dist\SOH-CURSOR-FPS-V3-OOT3D-FULL.cer"
        $passwordText = [guid]::NewGuid().ToString("N")
        $password = ConvertTo-SecureString $passwordText -AsPlainText -Force
        $cert = $null
        try {
            $cert = New-SelfSignedCertificate `
                -Type Custom `
                -Subject $env:PACKAGE_PUBLISHER `
                -KeyAlgorithm RSA `
                -KeyLength 2048 `
                -HashAlgorithm SHA256 `
                -KeyUsage DigitalSignature `
                -KeyExportPolicy Exportable `
                -CertStoreLocation Cert:\CurrentUser\My `
                -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3")
            Export-PfxCertificate -Cert $cert -FilePath $pfx -Password $password | Out-Null
            Export-Certificate -Cert $cert -FilePath $cer -Type CERT | Out-Null
            & $signTool sign /fd SHA256 /f $pfx /p $passwordText $appPackage
            Assert-LastExitCode "SignTool failed"
            Assert-EphemeralPackageSignature $signTool $appPackage $cert
            foreach ($dependencyAppx in Get-ChildItem "$dist\Dependencies" -File |
                Where-Object { $_.Extension -in ".appx", ".msix" }) {
                & $signTool verify /pa /v $dependencyAppx.FullName
                Assert-LastExitCode "Dependency AppX signature verification failed: $($dependencyAppx.Name)"
            }
            $unpacked = "$env:GITHUB_WORKSPACE\source\scratch\appx-audit"
            Remove-Item $unpacked -Recurse -Force -ErrorAction SilentlyContinue
            & $makeAppx unpack /p $appPackage /d $unpacked /o
            Assert-LastExitCode "Could not audit packed AppX"
            python tools/uwp_runtime_stage.py audit-appx `
                --unpacked-dir $unpacked `
                --publisher $env:PACKAGE_PUBLISHER
            Assert-LastExitCode "Packed AppX runtime audit failed"
        } finally {
            Remove-Item $pfx -Force -ErrorAction SilentlyContinue
            if ($null -ne $cert) {
                Remove-Item "Cert:\CurrentUser\My\$($cert.Thumbprint)" -Force -ErrorAction SilentlyContinue
            }
        }

        Copy-Item uwp\ANDROID-INSTALL.txt $dist
        Copy-Item uwp\PRIVATE-DATA-NOT-INCLUDED.txt $dist
        New-Item -ItemType Directory -Force "$dist\ThirdPartyNotices" | Out-Null
        if (Test-Path scratch\uwp-dep\lic) {
            Copy-Item scratch\uwp-dep\lic "$dist\ThirdPartyNotices\uwp-dep" -Recurse
        } elseif (Test-Path scratch\uwp-dep\License) {
            Copy-Item scratch\uwp-dep\License "$dist\ThirdPartyNotices\uwp-dep" -Recurse
        } elseif (Test-Path scratch\uwp-dep\LICENSE) {
            Copy-Item scratch\uwp-dep\LICENSE "$dist\ThirdPartyNotices\uwp-dep-LICENSE.txt"
        } else {
            throw "Pinned UWP dependency notices are missing"
        }
        if (Get-ChildItem $dist -Recurse -File -Include *.pfx,*.p12) {
            throw "A private signing key entered the Device Portal artifact"
        }
        Get-ChildItem $dist -Recurse -File |
            Where-Object { $_.Extension -in ".appx", ".msix", ".cer" } |
            Sort-Object FullName |
            ForEach-Object {
                $hash = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
                $relative = [IO.Path]::GetRelativePath($dist, $_.FullName).Replace('\', '/')
                "$hash  $relative"
            } | Set-Content "$dist\SHA256SUMS.txt"
        $sourceCommit = (git rev-parse HEAD).Trim()
        "source_commit=$sourceCommit" | Set-Content "$dist\BUILD.txt"
        "workflow_commit=$env:GITHUB_SHA" | Add-Content "$dist\BUILD.txt"
        "workflow_run=$env:GITHUB_RUN_ID" | Add-Content "$dist\BUILD.txt"
        "workflow_attempt=$env:GITHUB_RUN_ATTEMPT" | Add-Content "$dist\BUILD.txt"
    }

    "CoreDiagnostics" {
        Write-Diagnostics "windows-core-build.log" "Windows core diagnostic" `
            '(?i)(fatal error|:\s*error\s+(C|LNK|MSB)\d+|\berror\s+LNK\d+|\bLNK\d{4}\b|CMake Error)'
    }

    "PackageDiagnostics" {
        Write-Diagnostics "uwp-wrapper-build.log" "UWP package diagnostic" `
            '(?i)(fatal error|:\s*error\s+(C|LNK|MSB|APPX)\d+|\berror\s+LNK\d+|\bLNK\d{4}\b|CMake Error)'
    }
}
