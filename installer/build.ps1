param(
    [string]$Version = '1.0',
    [string]$OutputDir = (Join-Path $PSScriptRoot 'output'),
    # reuse the binaries already in AppPackage\bin\x64\Release
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Builds the release binaries, gathers everything the app needs to run without the
# Microsoft Store package, compiles the installer with Inno Setup and zips it.

$repo = Split-Path -Parent $PSScriptRoot
$binaries = Join-Path $repo 'AppPackage\bin\x64\Release'
$stage = Join-Path $PSScriptRoot 'stage'
$stageApp = Join-Path $stage 'app'
$stageFrameworks = Join-Path $stage 'frameworks'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
if (-not $vs) {
    throw 'Visual Studio with MSBuild was not found.'
}

if (-not $SkipBuild) {
    $msbuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
    & $msbuild (Join-Path $repo 'TranslucentTB.slnx') -t:Build -p:Configuration=Release -p:Platform=x64 -p:BuildType=Release -p:AppxBundle=Never -p:UapAppxPackageBuildMode=SideloadOnly -v:minimal -nologo
    if ($LASTEXITCODE -ne 0) {
        throw 'The build failed.'
    }
}

if (Test-Path $stage) {
    Remove-Item $stage -Recurse -Force
}
New-Item -ItemType Directory -Force $stageApp, $stageFrameworks, $OutputDir | Out-Null

# the app itself, without debug symbols or packaging leftovers
Copy-Item (Join-Path $binaries '*') -Destination $stageApp -Recurse -Include @('*.exe', '*.dll', 'resources.pri', 'Assets', 'Fonts')

# the Visual C++ runtime the binaries are linked against, deployed next to the executable
$redist = Get-ChildItem (Join-Path $vs 'VC\Redist\MSVC') -Directory |
    Where-Object { Test-Path (Join-Path $_.FullName 'x64\Microsoft.VC143.CRT') } |
    Sort-Object { [version]$_.Name } -Descending |
    Select-Object -First 1
if (-not $redist) {
    throw 'The Visual C++ redistributable files were not found.'
}
foreach ($dll in @('msvcp140.dll', 'msvcp140_atomic_wait.dll', 'vcruntime140.dll', 'vcruntime140_1.dll')) {
    Copy-Item (Join-Path $redist.FullName "x64\Microsoft.VC143.CRT\$dll") $stageApp
}

# the Windows frameworks the settings UI runs on, installed by the setup when missing
$winui = Get-ChildItem (Join-Path $repo 'packages') -Directory -Filter 'Microsoft.UI.Xaml.2.8.*' |
    Sort-Object Name -Descending |
    Select-Object -First 1
if (-not $winui) {
    throw 'The Microsoft.UI.Xaml NuGet package was not found; build the solution once to restore it.'
}
Copy-Item (Join-Path $winui.FullName 'tools\AppX\x64\Release\Microsoft.UI.Xaml.2.8.appx') $stageFrameworks
Copy-Item (Join-Path ${env:ProgramFiles(x86)} 'Microsoft SDKs\Windows Kits\10\ExtensionSDKs\Microsoft.VCLibs\14.0\Appx\Retail\x64\Microsoft.VCLibs.x64.14.00.appx') $stageFrameworks

$iscc = @(
    (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'),
    (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
    (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe')
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) {
    throw 'Inno Setup 6 was not found. Install it with: winget install JRSoftware.InnoSetup'
}

& $iscc "/DAppVersion=$Version" "/DStageDir=$stageApp" "/DFrameworkDir=$stageFrameworks" "/O$OutputDir" (Join-Path $PSScriptRoot 'TranslucentTB.iss')
if ($LASTEXITCODE -ne 0) {
    throw 'Inno Setup failed.'
}

$setup = Join-Path $OutputDir "TranslucentTB-$Version-Setup.exe"
$zip = Join-Path $OutputDir "TranslucentTB-$Version-x64.zip"
Compress-Archive -LiteralPath $setup -DestinationPath $zip -Force

Remove-Item $stage -Recurse -Force
Get-Item $setup, $zip
