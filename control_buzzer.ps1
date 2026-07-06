# Ventec AGV Monitor - Controle du buzzer (GPIO 13) via USB
#
# Menu interactif :
#   .\control_buzzer.ps1 -Port COM4
#
# Commande directe :
#   .\control_buzzer.ps1 -Port COM4 -Action test
#   .\control_buzzer.ps1 -Port COM4 -Action on
#   .\control_buzzer.ps1 -Port COM4 -Action off
#   .\control_buzzer.ps1 -Port COM4 -Action warn
#   .\control_buzzer.ps1 -Port COM4 -Action alarm
#   .\control_buzzer.ps1 -Port COM4 -Action custom -Freq 3000 -DurationMs 800

param(
    [string]$Port = "",
    [ValidateSet("", "test", "on", "off", "warn", "alarm", "custom")]
    [string]$Action = "",
    [int]$Freq = 2000,
    [int]$DurationMs = 500,
    [int]$Baud = 115200
)

$ErrorActionPreference = "Stop"
$ProjectDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ProjectDir

function Get-ComPort {
    if ($Port) { return $Port }
    $pioList = python -m platformio device list 2>&1 | Out-String
    if ($pioList -match '(COM\d+)') { return $Matches[1] }
    $ports = [System.IO.Ports.SerialPort]::GetPortNames()
    if ($ports.Count -eq 0) { throw "Aucun port COM. Branchez la carte ou precisez -Port COMx." }
    return $ports[0]
}

function Send-BuzzerCommand([System.IO.Ports.SerialPort]$serial, [string]$cmd) {
    while ($serial.BytesToRead -gt 0) { $null = $serial.ReadExisting() }
    $serial.WriteLine($cmd)
    Start-Sleep -Milliseconds 350
    $reply = ""
    $deadline = (Get-Date).AddMilliseconds(800)
    while ((Get-Date) -lt $deadline) {
        if ($serial.BytesToRead -gt 0) {
            $reply += $serial.ReadExisting()
        }
        Start-Sleep -Milliseconds 50
    }
    return $reply.Trim()
}

function Get-ActionCommand([string]$choice) {
    switch ($choice.ToLower()) {
        "1" { return "BUZZER" }
        "2" { return "BUZZER ON" }
        "3" { return "BUZZER OFF" }
        "4" { return "BUZZER WARN" }
        "5" { return "BUZZER ALARM" }
        "6" {
            $hz = Read-Host "Frequence Hz (ex: 2500)"
            $ms = Read-Host "Duree ms (ex: 300)"
            return "BUZZER $hz $ms"
        }
        default { return $null }
    }
}

function Resolve-ActionCommand {
    switch ($Action.ToLower()) {
        "test"   { return "BUZZER" }
        "on"     { return "BUZZER ON" }
        "off"    { return "BUZZER OFF" }
        "warn"   { return "BUZZER WARN" }
        "alarm"  { return "BUZZER ALARM" }
        "custom" { return "BUZZER $Freq $DurationMs" }
        default  { return $null }
    }
}

$com = Get-ComPort
$serial = New-Object System.IO.Ports.SerialPort $com, $Baud
$serial.NewLine = "`n"
$serial.ReadTimeout = 500
$serial.Open()
Start-Sleep -Milliseconds 600
while ($serial.BytesToRead -gt 0) { $null = $serial.ReadExisting() }

try {
    $cmd = Resolve-ActionCommand
    if ($cmd) {
        Write-Host "Port $com -> $cmd" -ForegroundColor Cyan
        $reply = Send-BuzzerCommand $serial $cmd
        if ($reply) { Write-Host $reply }
        return
    }

    Write-Host "=== Controle buzzer Ventec (GPIO 13) - $com ===" -ForegroundColor Cyan
    while ($true) {
        Write-Host ""
        Write-Host "  1  Bip test (500 ms)"
        Write-Host "  2  Buzzer ON (continu)"
        Write-Host "  3  Buzzer OFF"
        Write-Host "  4  Motif avertissement"
        Write-Host "  5  Motif alarme critique"
        Write-Host "  6  Bip personnalise (Hz + ms)"
        Write-Host "  Q  Quitter"
        $choice = Read-Host "Choix"
        if ($choice -match '^[Qq]') { break }

        $cmd = Get-ActionCommand $choice
        if (-not $cmd) {
            Write-Host "Choix invalide." -ForegroundColor Yellow
            continue
        }

        Write-Host "Envoi: $cmd" -ForegroundColor DarkGray
        $reply = Send-BuzzerCommand $serial $cmd
        if ($reply) { Write-Host $reply -ForegroundColor Green }
    }
}
finally {
    $serial.Close()
    $serial.Dispose()
}
