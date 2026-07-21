<#
.SYNOPSIS
    Build script for PsyernsCinematicTool (PCT) - Phase 0.
    Packs Scripts\ and GUI\ into PCT_Scripts.pbo / PCT_GUI.pbo, signs them,
    and deploys the result as a ready-to-use @PsyernsCinematicTool mod folder.

.DESCRIPTION
    Pack   : Uses DayZ Tools AddonBuilder.exe (CLI mode) in -packonly mode
             (no binarize - script/xml/layout files must stay as plain text).
             Source trees are staged into folders named after the desired
             PBO ("PCT_Scripts" / "PCT_GUI") because AddonBuilder derives the
             output PBO file name from the leaf name of the source folder.
    Sign   : Uses DSSignFile.exe from DsUtils. Generates a keypair via
             DSCreateKey.exe if the configured private key does not exist yet.
    Deploy : Copies the signed PBOs + public key into -DeployDir in the
             standard @ModName\addons + @ModName\keys layout.

    Windows PowerShell 5.1 compatible: no &&/||, no ternary, no ?./??.

    CLI syntax sources (see task report for full citations):
      - AddonBuilder.exe --help (captured live from this machine's install):
        "AddonBuilder.exe source_path destination_path [-packonly] [-clear]
         [-prefix=<addon prefix>] [-include=<file name>] ..."
      - DSSignFile.exe (no args) -> "Usage: DSSignFile private_key file_to_sign [-v2]"
      - DSCreateKey.exe (no args) -> "Usage: dsCreateKey authority_name"
      - Working -packonly/-prefix/DSSignFile call patterns cross-checked against
        local production scripts:
        C:\Users\Administrator\Desktop\DME_Vehicles\Analysis\_tools\pack_hybrid_all.ps1
        C:\Users\Administrator\Desktop\DME_Vehicles\Analysis\_tools\pack_plain_wheelfix.ps1
        C:\Users\Administrator\Desktop\DME_Vehicles\Analysis\_tools\Retest_ZProxyGuard.ps1

.PARAMETER PrivateKey
    Path to the .biprivatekey used for signing. Default:
    %USERPROFILE%\DayZKeys\Psyern.biprivatekey
    Must NOT point inside this repository (guarded, see Assert-KeyOutsideRepo).

.PARAMETER DeployDir
    Target folder for the built mod. Default:
    C:\Users\Administrator\Desktop\PsyernsCinematicTool\Build\@PsyernsCinematicTool\

.PARAMETER NoSign
    Skip signing entirely (PBOs are packed but left unsigned, no keys are
    touched/generated/copied).

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File Tools\Build_PCT.ps1

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File Tools\Build_PCT.ps1 -NoSign
#>

param(
    [string]$PrivateKey = (Join-Path $env:USERPROFILE "DayZKeys\Psyern.biprivatekey"),
    [string]$DeployDir = "C:\Users\Administrator\Desktop\PsyernsCinematicTool\Build\@PsyernsCinematicTool\",
    [switch]$NoSign
)

$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# Logging helpers
# ---------------------------------------------------------------------------
function Write-Step
{
    param([string]$Message)
    Write-Host ""
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Write-Info
{
    param([string]$Message)
    Write-Host "    $Message"
}

function Fail
{
    param([string]$Message)
    Write-Host ""
    Write-Host "BUILD FAILED: $Message" -ForegroundColor Red
    exit 1
}

# ---------------------------------------------------------------------------
# Fixed tool locations (DayZ Tools install)
# ---------------------------------------------------------------------------
$AddonBuilderExe = "C:\Program Files (x86)\Steam\steamapps\common\DayZ Tools\Bin\AddonBuilder\AddonBuilder.exe"
$DsUtilsDir      = "C:\Program Files (x86)\Steam\steamapps\common\DayZ Tools\Bin\DsUtils"
$DsSignFileExe   = Join-Path $DsUtilsDir "DSSignFile.exe"
$DsCreateKeyExe  = Join-Path $DsUtilsDir "DSCreateKey.exe"

# ---------------------------------------------------------------------------
# Project paths
# ---------------------------------------------------------------------------
$RepoRoot   = Split-Path -Parent $PSScriptRoot
$ModRoot    = Join-Path $RepoRoot "PsyernsCinematicTool"
$ScriptsSrc = Join-Path $ModRoot "Scripts"
$GuiSrc     = Join-Path $ModRoot "GUI"

$StageRoot    = Join-Path $RepoRoot "Build\_stage"
$StageScripts = Join-Path $StageRoot "PCT_Scripts"
$StageGui     = Join-Path $StageRoot "PCT_GUI"
$IncludeList  = Join-Path $StageRoot "addonbuilder_include.lst"

$AddonsOut = Join-Path $DeployDir "addons"
$KeysOut   = Join-Path $DeployDir "keys"

$PboScriptsName = "PCT_Scripts.pbo"
$PboGuiName     = "PCT_GUI.pbo"
$PboScriptsPath = Join-Path $AddonsOut $PboScriptsName
$PboGuiPath     = Join-Path $AddonsOut $PboGuiName

$PrefixScripts = "PsyernsCinematicTool\Scripts"
$PrefixGui     = "PsyernsCinematicTool\GUI"

Write-Host "==================================================================" -ForegroundColor Yellow
Write-Host " PsyernsCinematicTool - Build_PCT.ps1"                                -ForegroundColor Yellow
Write-Host "==================================================================" -ForegroundColor Yellow
Write-Info "RepoRoot   : $RepoRoot"
Write-Info "ModRoot    : $ModRoot"
Write-Info "DeployDir  : $DeployDir"
Write-Info "PrivateKey : $PrivateKey"
Write-Info "NoSign     : $($NoSign.IsPresent)"

# ---------------------------------------------------------------------------
# Preflight checks
# ---------------------------------------------------------------------------
Write-Step "Preflight checks"

if (-not (Test-Path -LiteralPath $AddonBuilderExe))
{
    Fail "AddonBuilder.exe not found at: $AddonBuilderExe (DayZ Tools not installed?)"
}
Write-Info "AddonBuilder.exe found."

if (-not $NoSign)
{
    if (-not (Test-Path -LiteralPath $DsSignFileExe))
    {
        Fail "DSSignFile.exe not found at: $DsSignFileExe (DayZ Tools not installed?)"
    }
    Write-Info "DSSignFile.exe found."

    if (-not (Test-Path -LiteralPath $DsCreateKeyExe))
    {
        Fail "DSCreateKey.exe not found at: $DsCreateKeyExe (DayZ Tools not installed?)"
    }
    Write-Info "DSCreateKey.exe found."
}
else
{
    Write-Info "NoSign set - DsUtils checks skipped."
}

if (-not (Test-Path -LiteralPath $ScriptsSrc))
{
    Fail "Source folder missing: $ScriptsSrc (Task 1/2 package not present or moved)"
}
if (-not (Test-Path -LiteralPath $GuiSrc))
{
    Fail "Source folder missing: $GuiSrc (Task 1/2 package not present or moved)"
}
Write-Info "Source folders OK ($ScriptsSrc, $GuiSrc)."

# ---------------------------------------------------------------------------
# Guard: private key must never live inside the repository
# ---------------------------------------------------------------------------
function Assert-KeyOutsideRepo
{
    param(
        [string]$KeyPath,
        [string]$RepoPath
    )

    $repoFull = [System.IO.Path]::GetFullPath($RepoPath)
    if (-not $repoFull.EndsWith("\"))
    {
        $repoFull = $repoFull + "\"
    }

    $keyFull = [System.IO.Path]::GetFullPath($KeyPath)

    $repoFullLower = $repoFull.ToLowerInvariant()
    $keyFullLower  = $keyFull.ToLowerInvariant()

    if ($keyFullLower.StartsWith($repoFullLower))
    {
        Fail "Guard: -PrivateKey ('$KeyPath') resolves inside the repository ('$RepoPath'). The private key must NEVER be stored in the repo. Aborting."
    }
}

if (-not $NoSign)
{
    Write-Step "Guard: verifying private key path is outside the repository"
    Assert-KeyOutsideRepo -KeyPath $PrivateKey -RepoPath $RepoRoot
    Write-Info "OK - private key path is outside the repo."
}

# ---------------------------------------------------------------------------
# Helpers for staging
# ---------------------------------------------------------------------------
function New-CleanDirectory
{
    param([string]$Path)

    if (Test-Path -LiteralPath $Path)
    {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $Path | Out-Null
}

function Copy-SourceTree
{
    param(
        [string]$Source,
        [string]$Destination
    )

    New-CleanDirectory -Path $Destination
    $robocopyArgs = @($Source, $Destination, "/E", "/NFL", "/NDL", "/NJH", "/NJS", "/NP", "/R:1", "/W:1")
    & robocopy @robocopyArgs | Out-Null
    $rc = $LASTEXITCODE
    if ($rc -ge 8)
    {
        Fail "robocopy failed (exit $rc) copying $Source -> $Destination"
    }
}

# ---------------------------------------------------------------------------
# Stage sources (folder name == desired PBO base name; AddonBuilder derives
# the output PBO file name from the source folder's leaf name)
# ---------------------------------------------------------------------------
Write-Step "Staging source trees"

# Clean the whole stage root ONCE, before staging anything into it.
New-CleanDirectory -Path $StageRoot

Write-Info "Scripts -> $StageScripts"
Copy-SourceTree -Source $ScriptsSrc -Destination $StageScripts
Write-Info "GUI     -> $StageGui"
Copy-SourceTree -Source $GuiSrc -Destination $StageGui

# ---------------------------------------------------------------------------
# Explicit include list, so plain-text/data files are guaranteed to be
# copied into the PBO regardless of AddonBuilder's default whitelist
# handling. (AddonBuilder -include: "Directly copies matched files to PBO.
# Absolute path to file with wildcard patterns, separator is ';' and ','.")
# ---------------------------------------------------------------------------
$includePatterns = "*.c;*.cpp;*.xml;*.layout;*.edds;*.paa;*.json;*.rvmat;*.p3d;*.ogg;*.wav"
Set-Content -LiteralPath $IncludeList -Value $includePatterns -Encoding ASCII
Write-Info "Include-list written: $IncludeList"
Write-Info "Patterns: $includePatterns"

# ---------------------------------------------------------------------------
# Pack via AddonBuilder CLI
#   AddonBuilder.exe source_path destination_path -clear -packonly
#                    -prefix=<addon prefix> -include=<include list file>
# ---------------------------------------------------------------------------
function Invoke-AddonBuilderPack
{
    param(
        [string]$SourceDir,
        [string]$DestDir,
        [string]$Prefix
    )

    if (-not (Test-Path -LiteralPath $DestDir))
    {
        New-Item -ItemType Directory -Force -Path $DestDir | Out-Null
    }

    $prefixArg  = "-prefix=" + $Prefix
    $includeArg = "-include=" + $IncludeList

    Write-Info "AddonBuilder.exe `"$SourceDir`" `"$DestDir`" -clear -packonly $prefixArg $includeArg"
    & $AddonBuilderExe $SourceDir $DestDir -clear -packonly $prefixArg $includeArg
    $rc = $LASTEXITCODE
    if ($rc -ne 0)
    {
        Fail "AddonBuilder.exe failed (exit $rc) for source '$SourceDir'"
    }
}

function Clear-DeployFolder
{
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path))
    {
        Write-Info "Nothing to clean (does not exist yet): $Path"
        return
    }

    Get-ChildItem -LiteralPath $Path -Force | Remove-Item -Recurse -Force -Confirm:$false
    Write-Info "Cleared: $Path"
}

Write-Step "Cleaning deploy target (addons/keys)"
Clear-DeployFolder -Path $AddonsOut
Clear-DeployFolder -Path $KeysOut

Write-Step "Packing PCT_Scripts.pbo"
Invoke-AddonBuilderPack -SourceDir $StageScripts -DestDir $AddonsOut -Prefix $PrefixScripts

Write-Step "Packing PCT_GUI.pbo"
Invoke-AddonBuilderPack -SourceDir $StageGui -DestDir $AddonsOut -Prefix $PrefixGui

if (-not (Test-Path -LiteralPath $PboScriptsPath))
{
    Fail "Expected PBO not found after pack: $PboScriptsPath"
}
if (-not (Test-Path -LiteralPath $PboGuiPath))
{
    Fail "Expected PBO not found after pack: $PboGuiPath"
}
Write-Info "Both PBOs present in $AddonsOut."

# ---------------------------------------------------------------------------
# Content sanity check - PBO headers store file names as plain (null
# terminated) text, so a raw byte/string scan is enough to prove that the
# required non-code assets actually made it into the archive.
# ---------------------------------------------------------------------------
function Test-PboContainsStrings
{
    param(
        [string]$PboPath,
        [string[]]$Needles
    )

    $bytes = [System.IO.File]::ReadAllBytes($PboPath)
    $text = [System.Text.Encoding]::ASCII.GetString($bytes)

    $missing = @()
    foreach ($needle in $Needles)
    {
        if (-not $text.Contains($needle))
        {
            $missing += $needle
        }
    }
    return $missing
}

Write-Step "PBO content sanity check"

$scriptsNeedles = @("config.cpp", "Inputs.xml", "PCT_Constants.c", "PCT_RPC.c", "PCT_ModuleRegistration.c")
$missingScripts = Test-PboContainsStrings -PboPath $PboScriptsPath -Needles $scriptsNeedles
if ($missingScripts.Count -gt 0)
{
    Fail "PCT_Scripts.pbo is missing expected entries: $($missingScripts -join ', ')"
}
Write-Info "PCT_Scripts.pbo contains: $($scriptsNeedles -join ', ')"

$guiNeedles = @("config.cpp", "pct_cinematic_form.layout")
$missingGui = Test-PboContainsStrings -PboPath $PboGuiPath -Needles $guiNeedles
if ($missingGui.Count -gt 0)
{
    Fail "PCT_GUI.pbo is missing expected entries: $($missingGui -join ', ')"
}
Write-Info "PCT_GUI.pbo contains: $($guiNeedles -join ', ')"

# ---------------------------------------------------------------------------
# Sign (unless -NoSign)
# ---------------------------------------------------------------------------
if (-not $NoSign)
{
    Write-Step "Signing"

    $keyDir  = Split-Path -Path $PrivateKey -Parent
    $keyName = [System.IO.Path]::GetFileNameWithoutExtension($PrivateKey)
    $bikeyPath = Join-Path $keyDir ($keyName + ".bikey")

    if (-not (Test-Path -LiteralPath $PrivateKey))
    {
        Write-Info "Private key not found - generating keypair '$keyName' via DSCreateKey.exe"
        if (-not (Test-Path -LiteralPath $keyDir))
        {
            New-Item -ItemType Directory -Force -Path $keyDir | Out-Null
        }

        Push-Location $keyDir
        & $DsCreateKeyExe $keyName
        $keyRc = $LASTEXITCODE
        Pop-Location

        if ($keyRc -ne 0)
        {
            Fail "DSCreateKey.exe failed (exit $keyRc) for authority '$keyName'"
        }
    }
    else
    {
        Write-Info "Existing private key found: $PrivateKey"
    }

    if (-not (Test-Path -LiteralPath $PrivateKey))
    {
        Fail "Private key still missing after generation step: $PrivateKey"
    }
    if (-not (Test-Path -LiteralPath $bikeyPath))
    {
        Fail "Public .bikey still missing after generation step: $bikeyPath"
    }
    Write-Info "Keypair OK: $PrivateKey / $bikeyPath"

    function Invoke-Sign
    {
        param([string]$PboPath)

        Write-Info "DSSignFile.exe `"$PrivateKey`" `"$PboPath`""
        & $DsSignFileExe $PrivateKey $PboPath
        $rc = $LASTEXITCODE
        if ($rc -ne 0)
        {
            Fail "DSSignFile.exe failed (exit $rc) for '$PboPath'"
        }

        $pboDir  = Split-Path -Path $PboPath -Parent
        $pboName = Split-Path -Path $PboPath -Leaf
        $sig = Get-ChildItem -LiteralPath $pboDir -Filter ($pboName + ".*.bisign") -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $sig)
        {
            Fail "No .bisign file found next to '$PboPath' after signing."
        }
        Write-Info "Signature created: $($sig.FullName)"
    }

    Invoke-Sign -PboPath $PboScriptsPath
    Invoke-Sign -PboPath $PboGuiPath

    if (-not (Test-Path -LiteralPath $KeysOut))
    {
        New-Item -ItemType Directory -Force -Path $KeysOut | Out-Null
    }
    Copy-Item -LiteralPath $bikeyPath -Destination (Join-Path $KeysOut ($keyName + ".bikey")) -Force
    Write-Info "Public key deployed: $(Join-Path $KeysOut ($keyName + '.bikey'))"
}
else
{
    Write-Step "Signing skipped (-NoSign)"
}

# ---------------------------------------------------------------------------
# Final report
# ---------------------------------------------------------------------------
Write-Step "Build finished"
Write-Info "Deploy structure at: $DeployDir"
Get-ChildItem -LiteralPath $DeployDir -Recurse | ForEach-Object {
    Write-Info $_.FullName
}

Write-Host ""
Write-Host "BUILD OK" -ForegroundColor Green
exit 0
