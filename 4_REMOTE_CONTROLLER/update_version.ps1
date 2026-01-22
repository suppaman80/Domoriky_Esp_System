param (
    [string]$Description = "",
    [string]$Type = "patch"
)

$versionFile = "version.h"
$changelogFile = "CHANGELOG.md"

if (!(Test-Path $versionFile)) {
    Write-Error "File $versionFile non trovato!"
    exit 1
}

# --- 1. LEGGI VERSIONE CORRENTE ---
$content = Get-Content $versionFile
$major = 0
$minor = 0
$patch = 0

foreach ($line in $content) {
    if ($line -match '#define FIRMWARE_VERSION_MAJOR\s+(\d+)') { $major = [int]$matches[1] }
    if ($line -match '#define FIRMWARE_VERSION_MINOR\s+(\d+)') { $minor = [int]$matches[1] }
    if ($line -match '#define FIRMWARE_VERSION_PATCH\s+(\d+)') { $patch = [int]$matches[1] }
}

# --- 2. INCREMENTA VERSIONE ---
switch ($Type.ToLower()) {
    "major" { 
        $major++
        $minor = 0
        $patch = 0
        Write-Host "[INFO] Major update selected"
    }
    "minor" { 
        $minor++
        $patch = 0
        Write-Host "[INFO] Minor update selected"
    }
    "patch" { 
        $patch++
        Write-Host "[INFO] Patch update selected"
    }
    default { 
        $patch++
        Write-Host "[INFO] Defaulting to Patch update"
    }
}

$newVersion = "$major.$minor.$patch"
$today = Get-Date -Format "yyyy-MM-dd"

# --- 3. AGGIORNA CHANGELOG (Se c'è descrizione) ---
if ($Description -ne "") {
    if (Test-Path $changelogFile) {
        $changelogContent = Get-Content $changelogFile
        
        # Prepara la nuova entry
        $newEntry = @"
## [$newVersion] - $today
- $Description

"@
        $newFileContent = @()
        $inserted = $false
        
        foreach ($line in $changelogContent) {
            if (!$inserted -and $line -match "^## \[") {
                $newFileContent += $newEntry.Trim()
                $newFileContent += "" 
                $inserted = $true
            }
            $newFileContent += $line
        }
        
        if (!$inserted) {
            $newFileContent += ""
            $newFileContent += $newEntry.Trim()
        }
        
        $newFileContent | Set-Content $changelogFile
        Write-Host "[OK] Changelog aggiornato con: $Description"
    } else {
        # Crea nuovo changelog
        $newContent = @"
# Changelog

## [$newVersion] - $today
- $Description
"@
        $newContent | Set-Content $changelogFile
        Write-Host "[OK] Creato nuovo file CHANGELOG.md"
    }
} else {
    Write-Host "[INFO] Nessuna descrizione fornita. Changelog non aggiornato."
}

# --- 4. AGGIORNA VERSION.H ---
$newHeaderContent = @(
    "#ifndef VERSION_H"
    "#define VERSION_H"
    ""
    "#define FIRMWARE_VERSION_MAJOR $major"
    "#define FIRMWARE_VERSION_MINOR $minor"
    "#define FIRMWARE_VERSION_PATCH $patch"
    "#define FIRMWARE_VERSION `"$newVersion`""
    ""
    "#endif"
)

$newHeaderContent | Set-Content $versionFile

Write-Host "[OK] Versione incrementata a: $newVersion"
