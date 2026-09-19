#requires -Version 5.1
<#
.SYNOPSIS
Interactively builds PiTime with Visual Studio and optionally runs it.

.DESCRIPTION
Uses the native x64 solution and automatically locates Visual Studio's build tools.
GMP is restored by the project. Tests and benchmarks are not part of this build.
Use -NonInteractive to accept parameter values without showing the menus.

.PARAMETER Configuration
Release enables performance optimizations. Debug enables normal debugging.

.PARAMETER Action
Build updates changed files, Rebuild recompiles the application, and BuildAndRun
builds successfully before launching PiTime.

.PARAMETER Digits
Decimal places to calculate when using BuildAndRun.

.PARAMETER Threads
Worker limit when using BuildAndRun. Zero selects the automatic limit.

.PARAMETER Quiet
Suppresses printed digits when using BuildAndRun. Calculation timing is still shown.

.PARAMETER NonInteractive
Skips the menus, using the supplied parameters or their defaults.

.EXAMPLE
.\Build.ps1

.EXAMPLE
.\Build.ps1 -NonInteractive -Configuration Release -Action BuildAndRun -Digits 1000000 -Quiet
#>

[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',

    [ValidateSet('Build', 'Rebuild', 'BuildAndRun')]
    [string]$Action = 'Build',

    [ValidateRange(0, 100000000)]
    [int]$Digits = 10000,

    [ValidateRange(0, 256)]
    [int]$Threads = 0,

    [switch]$Quiet,
    [switch]$NonInteractive
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$nativeErrorPreference = Get-Variable -Name PSNativeCommandUseErrorActionPreference `
    -ErrorAction SilentlyContinue
if ($null -ne $nativeErrorPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

function Read-MenuChoice {
    param(
        [Parameter(Mandatory)][string]$Title,
        [Parameter(Mandatory)][string[]]$Choices,
        [Parameter(Mandatory)][string]$DefaultChoice
    )

    Write-Host "`n$Title" -ForegroundColor Cyan
    $defaultIndex = 1
    for ($index = 0; $index -lt $Choices.Count; $index++) {
        Write-Host ('  {0}. {1}' -f ($index + 1), $Choices[$index])
        if ($Choices[$index] -eq $DefaultChoice) {
            $defaultIndex = $index + 1
        }
    }
    Write-Host '  Q. Quit'

    while ($true) {
        $prompt = "Select [$defaultIndex]"
        if ([Console]::IsInputRedirected) {
            Write-Host "${prompt}: " -NoNewline
            $answer = [Console]::ReadLine()
        }
        else {
            $answer = Read-Host $prompt
        }

        # End-of-input cancels instead of repeatedly accepting empty defaults.
        if ($null -eq $answer -or $answer.Trim() -eq 'q') {
            return $null
        }
        $answer = $answer.Trim()
        if ($answer.Length -eq 0) {
            return $Choices[$defaultIndex - 1]
        }

        $selection = 0
        if ([int]::TryParse($answer, [ref]$selection) -and
            $selection -ge 1 -and $selection -le $Choices.Count) {
            return $Choices[$selection - 1]
        }
        Write-Host "Enter a number from 1 to $($Choices.Count), or Q to quit."
    }
}

function Find-MSBuild {
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
        throw 'This script builds the Windows solution. Use the CMake presets on other platforms.'
    }

    $installerDirectory = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer'
    $vswherePath = Join-Path $installerDirectory 'vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswherePath -PathType Leaf)) {
        throw 'Visual Studio Installer was not found. Install Visual Studio with its C++ workload.'
    }

    $locatorArguments = @(
        '-products', '*'
        '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64'
        '-version', '[17.6,)'
        '-sort'
        '-property', 'installationPath'
    )
    $installations = @(& $vswherePath @locatorArguments)
    if ($LASTEXITCODE -ne 0) {
        throw "Visual Studio discovery failed with exit code $LASTEXITCODE."
    }

    foreach ($installation in $installations) {
        if ([string]::IsNullOrWhiteSpace($installation)) {
            continue
        }
        $buildTool = Join-Path $installation 'MSBuild\Current\Bin\MSBuild.exe'
        $vcpkgPath = Join-Path $installation 'VC\vcpkg\vcpkg.exe'
        if ((Test-Path -LiteralPath $buildTool -PathType Leaf) -and
            (Test-Path -LiteralPath $vcpkgPath -PathType Leaf)) {
            return $buildTool
        }
    }

    throw ('Visual Studio 2022 (17.6+) or 2026 with Desktop development with C++ and ' +
        'its bundled vcpkg component is required. Add these in Visual Studio Installer.')
}

try {
    if (-not $NonInteractive) {
        Write-Host 'PiTime application build' -ForegroundColor Cyan
        $selectedConfiguration = Read-MenuChoice -Title 'Configuration' `
            -Choices @('Release', 'Debug') -DefaultChoice $Configuration
        if (-not $selectedConfiguration) {
            Write-Host 'Build cancelled.'
            exit 0
        }
        $Configuration = $selectedConfiguration

        $selectedAction = Read-MenuChoice -Title 'Action' `
            -Choices @('Build', 'Rebuild', 'BuildAndRun') -DefaultChoice $Action
        if (-not $selectedAction) {
            Write-Host 'Build cancelled.'
            exit 0
        }
        $Action = $selectedAction
    }

    $repositoryRoot = $PSScriptRoot
    $solutionPath = Join-Path $repositoryRoot 'PiTime.sln'
    if (-not (Test-Path -LiteralPath $solutionPath -PathType Leaf)) {
        throw ("The solution was not found at '$solutionPath'. " +
            'Keep this script in the repository root.')
    }
    $buildTool = Find-MSBuild
    $target = 'Build'
    if ($Action -eq 'Rebuild') {
        $target = 'Rebuild'
    }
    $buildArguments = @(
        $solutionPath
        '/nologo'
        '/m'
        '/verbosity:minimal'
        "/t:$target"
        "/p:Configuration=$Configuration"
        '/p:Platform=x64'
    )

    Write-Host "`n$target PiTime ($Configuration | x64)" -ForegroundColor Cyan
    Write-Host 'The first build restores GMP automatically and may take several minutes.'
    Push-Location $repositoryRoot
    try {
        & $buildTool @buildArguments
        if ($LASTEXITCODE -ne 0) {
            throw "MSBuild failed with exit code $LASTEXITCODE."
        }

        $application = Join-Path $repositoryRoot "build\visual-studio\x64\$Configuration\PiTime.exe"
        if (-not (Test-Path -LiteralPath $application -PathType Leaf)) {
            throw "MSBuild completed without producing '$application'."
        }
        Write-Host "`nBuilt '$application'." -ForegroundColor Green

        if ($Action -eq 'BuildAndRun') {
            $runArguments = @('--digits', "$Digits", '--threads', "$Threads")
            if ($Quiet) {
                $runArguments += '--quiet'
            }
            Write-Host "`nCalculating $Digits decimal places..." -ForegroundColor Cyan
            & $application @runArguments
            if ($LASTEXITCODE -ne 0) {
                throw "PiTime failed with exit code $LASTEXITCODE."
            }
        }
    }
    finally {
        Pop-Location
    }
}
catch {
    Write-Host "Build script failed: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

exit 0
