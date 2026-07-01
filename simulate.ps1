# Ventec AGV Monitor - Simulation depuis PowerShell
# Usage:
#   .\simulate.ps1                    # detecte le port COM et lance les scenarios
#   .\simulate.ps1 -Port COM3         # port explicite
#   .\simulate.ps1 -Upload            # flash avant simulation
#   .\simulate.ps1 -Interactive       # ouvre le moniteur serie PlatformIO

param(
    [string]$Port = "",
    [switch]$Upload,
    [switch]$Interactive,
    [int]$Baud = 115200
)

$ErrorActionPreference = "Stop"
$ProjectDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ProjectDir

function Write-Step([string]$msg) {
    Write-Host "`n==> $msg" -ForegroundColor Cyan
}

function Get-ComPort {
    if ($Port) { return $Port }

    Write-Step "Recherche du port COM ESP32..."
    $pioList = python -m platformio device list 2>&1 | Out-String

    if ($pioList -match '(COM\d+)') {
        $found = $Matches[1]
        Write-Host "Port detecte : $found"
        return $found
    }

    # Fallback : ports serie Windows
    $ports = [System.IO.Ports.SerialPort]::GetPortNames()
    if ($ports.Count -eq 0) {
        throw "Aucun port COM detecte. Branchez l'ESP32 en USB."
    }
    $chosen = $ports[0]
    Write-Host "Port utilise : $chosen"
    return $chosen
}

function Send-SerialCommands {
    param(
        [string]$ComPort,
        [string[]]$Commands,
        [int]$DelayMs = 800
    )

    $serial = New-Object System.IO.Ports.SerialPort $ComPort, $Baud
    $serial.NewLine = "`n"
    $serial.ReadTimeout = 3000
    $serial.WriteTimeout = 3000

    try {
        $serial.Open()
        Start-Sleep -Milliseconds 500
        # Vider le buffer de demarrage
        while ($serial.BytesToRead -gt 0) { $null = $serial.ReadExisting() }

        foreach ($cmd in $Commands) {
            Write-Host "> $cmd" -ForegroundColor Yellow
            $serial.WriteLine($cmd)
            Start-Sleep -Milliseconds $DelayMs

            $response = ""
            $deadline = (Get-Date).AddSeconds(2)
            while ((Get-Date) -lt $deadline) {
                if ($serial.BytesToRead -gt 0) {
                    $response += $serial.ReadExisting()
                } else {
                    Start-Sleep -Milliseconds 50
                }
            }
            if ($response.Trim()) {
                Write-Host $response
            }
        }
    }
    finally {
        if ($serial.IsOpen) { $serial.Close() }
        $serial.Dispose()
    }
}

# --- Main ---

if ($Upload) {
    Write-Step "Compilation et flash..."
    python -m platformio run -t upload
    if ($LASTEXITCODE -ne 0) { throw "Echec upload" }
    Start-Sleep -Seconds 2
}

if ($Interactive) {
    Write-Step "Moniteur serie interactif (Ctrl+C pour quitter)"
    Write-Host "Commandes utiles : LABON | SIM 16 | SIM 22 | SIMTEMP 80 | STATUS | EXPORT | LABOFF"
    python -m platformio device monitor
    exit 0
}

$com = Get-ComPort

Write-Step "Scenario de simulation automatique sur $com"

$scenario = @(
    "HELP"
    "LABON"
    "SIM 5"
    "STATUS"
    "SIM 16"
    "STATUS"
    "SIM 22"
    "STATUS"
    "SIMTEMP 80"
    "STATUS"
    "STORAGE"
    "LABOFF"
    "STATUS"
)

Send-SerialCommands -ComPort $com -Commands $scenario -DelayMs 1200

Write-Step "Simulation terminee"
Write-Host @"

Prochaines commandes manuelles (moniteur interactif) :
  .\simulate.ps1 -Interactive

Ou avec flash :
  .\simulate.ps1 -Upload -Interactive

"@ -ForegroundColor Green
