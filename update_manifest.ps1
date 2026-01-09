param (
    [string]$RepoUser = "suppaman80",
    [string]$RepoName = "Domoriky_Esp_System"
)

function Get-VersionFromHeader {
    param ([string]$Path)
    if (Test-Path $Path) {
        $content = Get-Content $Path
        foreach ($line in $content) {
            if ($line -match '#define FIRMWARE_VERSION\s+"([^"]+)"') {
                return $matches[1]
            }
        }
    }
    return $null
}

# 1. Read Versions from Code
$DashVer = Get-VersionFromHeader "ESP32_Dashboard_Controller/version.h"
$GwVer = Get-VersionFromHeader "ESP8266_Gateway_mqtt/version.h"
$NodeVer = Get-VersionFromHeader "4_RELAY_CONTROLLER/version.h"

if (!$DashVer -or !$GwVer -or !$NodeVer) {
    Write-Error "Impossibile leggere le versioni dai file .h"
    exit 1
}

Write-Host "Versioni rilevate:"
Write-Host "  - Dashboard: $DashVer"
Write-Host "  - Gateway:   $GwVer"
Write-Host "  - Node:      $NodeVer"

# 2. Update versions.json
$jsonPath = "versions.json"
if (Test-Path $jsonPath) {
    $json = Get-Content $jsonPath | ConvertFrom-Json

    # Dashboard
    $json.dashboard.version = $DashVer
    $json.dashboard.url = "https://raw.githubusercontent.com/$RepoUser/$RepoName/master/bin/ESP32_Dashboard_Controller.ino.bin"

    # Gateway
    $json.gateway.version = $GwVer
    $json.gateway.url = "https://raw.githubusercontent.com/$RepoUser/$RepoName/master/bin/ESP8266_Gateway_mqtt.ino.bin"

    # Node
    if ($json.nodes."4_RELAY_CONTROLLER") {
        $json.nodes."4_RELAY_CONTROLLER".version = $NodeVer
        $json.nodes."4_RELAY_CONTROLLER".url = "https://raw.githubusercontent.com/$RepoUser/$RepoName/master/bin/4_RELAY_CONTROLLER.ino.bin"
    }

    $json | ConvertTo-Json -Depth 4 | Set-Content $jsonPath
    Write-Host "Manifesto versions.json aggiornato."
    
    # Restituisce le versioni separate da punto e virgola per il Batch
    return "$DashVer;$GwVer;$NodeVer"
} else {
    Write-Error "File versions.json non trovato!"
    exit 1
}
