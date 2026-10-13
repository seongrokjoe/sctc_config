param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [ValidateSet('v141', 'v142', 'v143')][string]$Toolset = 'v141',
    [ValidatePattern('^\d+\.\d+\.\d+\.\d+$')][string]$Sdk = '10.0.17134.0',
    [switch]$Test
)
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio 2017 or newer with Desktop development with C++.' }
$instances = & $vswhere -all -products * -format json | ConvertFrom-Json
$compilerPrefix = @{ v141 = '14.1'; v142 = '14.2'; v143 = '14.4' }[$Toolset]
$msbuild = $null
foreach ($instance in ($instances | Sort-Object { [Version]$_.installationVersion } -Descending)) {
    $compilerRoot = Join-Path $instance.installationPath 'VC\Tools\MSVC'
    if (!(Test-Path -LiteralPath $compilerRoot)) { continue }
    $compilers = @(Get-ChildItem -LiteralPath $compilerRoot -Directory | Where-Object {
        $_.Name.StartsWith($compilerPrefix) -or ($Toolset -eq 'v143' -and $_.Name.StartsWith('14.3'))
    })
    if (!$compilers.Count) { continue }
    foreach ($relative in @('MSBuild\Current\Bin\MSBuild.exe', 'MSBuild\15.0\Bin\MSBuild.exe')) {
        $candidate = Join-Path $instance.installationPath $relative
        if (Test-Path -LiteralPath $candidate) { $msbuild = $candidate; break }
    }
    if ($msbuild) { break }
}
if (!$msbuild) {
    throw "The $Toolset x64/x86 C++ build tools are not installed. Add the v141 (VS 2017) tools using Visual Studio Installer. No fallback toolset will be used."
}
$sdkRoots = @((Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Windows Kits\10'))
foreach ($key in @('HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots', 'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows Kits\Installed Roots')) {
    $item = Get-ItemProperty -LiteralPath $key -ErrorAction SilentlyContinue
    if ($item -and $item.KitsRoot10) { $sdkRoots += $item.KitsRoot10 }
}
$hasSdk = $false
foreach ($sdkRoot in $sdkRoots) {
    if ((Test-Path -LiteralPath (Join-Path $sdkRoot "Include\$Sdk\um\Windows.h")) -and
        (Test-Path -LiteralPath (Join-Path $sdkRoot "Lib\$Sdk\um\x64\kernel32.lib"))) { $hasSdk = $true; break }
}
if (!$hasSdk) {
    throw "Windows SDK $Sdk is not installed. Install the Windows 10 SDK 1803 (10.0.17134.0) from the Microsoft SDK archive. No fallback SDK will be used."
}
Write-Host "Building independent AlarmCsvMerge: $Toolset / SDK $Sdk / x64 / MT"
& $msbuild (Join-Path $PSScriptRoot 'AlarmCsvMerge.sln') /m /t:Rebuild "/p:Configuration=$Configuration" /p:Platform=x64 "/p:PlatformToolset=$Toolset" "/p:WindowsTargetPlatformVersion=$Sdk" /verbosity:minimal
if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE). No executable was published to dist." }
$buildDir = Join-Path $PSScriptRoot ("build\{0}_{1}\x64\{2}" -f $Toolset, $Sdk, $Configuration)
if ($Test) {
    & (Join-Path $buildDir 'AlarmCsvMerge.Tests.exe')
    if ($LASTEXITCODE -ne 0) { throw "Core tests failed ($LASTEXITCODE). No executable was published." }
    & (Join-Path $PSScriptRoot 'tests\Integration.ps1') -ExePath (Join-Path $buildDir 'AlarmCsvMerge.exe')
}
if ($Toolset -eq 'v141' -and $Sdk -eq '10.0.17134.0' -and $Configuration -eq 'Release') {
    $dist = Join-Path $PSScriptRoot 'dist'
    New-Item -ItemType Directory -Force -Path $dist | Out-Null
    Copy-Item -LiteralPath (Join-Path $buildDir 'AlarmCsvMerge.exe') -Destination (Join-Path $dist 'AlarmCsvMerge.exe') -Force
    Write-Host ""
    Write-Host "SUCCESS: $dist\AlarmCsvMerge.exe"
    Write-Host 'Deploy only AlarmCsvMerge.exe. The existing SCTC_CONFIG project was not built or modified.'
} else {
    Write-Warning "Development validation only: $buildDir\AlarmCsvMerge.exe"
    Write-Warning 'The requested v141 / SDK 10.0.17134.0 release artifact was not published.'
}
