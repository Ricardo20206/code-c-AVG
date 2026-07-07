# Ventec AGV Monitor - Affichage capteurs dans PowerShell
#
# Sans carte (simulation PC) :
#   .\monitor_sensors.ps1 -Simulate
#
# Avec carte USB :
#   .\monitor_sensors.ps1 -Port COM4
#   .\monitor_sensors.ps1 -Port COM4 -Upload

param(
    [string]$Port = "",
    [switch]$Simulate,
    [switch]$Upload,
    [int]$IntervalSec = 1,
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
    if ($ports.Count -eq 0) { throw "Aucun port COM. Utilisez -Simulate ou branchez la carte." }
    return $ports[0]
}

function Format-SensorTable([hashtable]$d) {
    Clear-Host
    Write-Host "=== Ventec AGV Monitor - Capteurs ===" -ForegroundColor Cyan
    Write-Host ("Heure : {0:HH:mm:ss}" -f (Get-Date))
    if ($d.LAB -eq 1) { Write-Host "Mode  : SIMULATION (labo)" -ForegroundColor Yellow }
    else { Write-Host "Mode  : Carte reelle" -ForegroundColor Green }
    Write-Host ""
    Write-Host ("  Courant 36V     : {0,8:N2} A" -f $d.I)
    if ($d.VSHUNT -ge 0) {
        Write-Host ("  Tension shunt   : {0,8:N4} mV" -f $d.VSHUNT)
    } else {
        Write-Host "  Tension shunt   :      N/A"
    }
    Write-Host ("  Temperature PCB1: {0,8:N1} C" -f $d.T1)
    Write-Host ("  Temperature PCB2: {0,8:N1} C" -f $d.T2)
    Write-Host ("  Temperature amb.: {0,8:N1} C" -f $d.AMB)
    $hum = if ($d.HUM -lt 0) { "N/A" } else { "{0:N1} %" -f $d.HUM }
    Write-Host ("  Humidite        : {0,9}" -f $hum)
    $vib = if ($d.VIB -lt 0) { "N/A" } else { "{0:N0} mg" -f $d.VIB }
    Write-Host ("  Vibration       : {0,9}" -f $vib)
    $alm = switch ($d.ALM) { 1 { "WARNING" } 2 { "CRITIQUE" } default { "OK" } }
    Write-Host ("  Alarme          : {0,9}" -f $alm)
    Write-Host ("  Source alim.    : {0,9}" -f $d.SRC)
    Write-Host ""
    Write-Host "  Bus / modules :"
    Write-Host ("    INA237 courant : {0}" -f ($(if ($d.INA -eq 1) { "OK" } else { "ABSENT" })))
    Write-Host ("    TMP126 ambiant : {0}" -f ($(if ($d.TMP -eq 1) { "OK" } else { "ABSENT" })))
    Write-Host ("    Grove vibration: {0}" -f ($(if ($d.VIB_S -eq 1) { "PRESENT" } else { "ABSENT" })))
    Write-Host ("    Grove humidite : {0}" -f ($(if ($d.HUM_S -eq 1) { "PRESENT" } else { "ABSENT" })))
    Write-Host ""
    Write-Host "Ctrl+C pour quitter" -ForegroundColor DarkGray
}

function Parse-SensorLine([string]$line) {
    if ($line -notmatch '\[SENSORS\]') { return $null }
    $d = @{}
    foreach ($pair in ([regex]::Matches($line, '(\w+)=([^\s]+)'))) {
        $d[$pair.Groups[1].Value] = $pair.Groups[2].Value
    }
    return [pscustomobject]@{
        I    = [double]$d.I
        VSHUNT = if ($d.VSHUNT) { [double]$d.VSHUNT } else { -1.0 }
        T1   = [double]$d.T1
        T2   = [double]$d.T2
        AMB  = [double]$d.AMB
        HUM  = [double]$d.HUM
        VIB  = [double]$d.VIB
        ALM  = [int]$d.ALM
        SRC  = $d.SRC
        INA  = [int]$d.INA
        TMP  = [int]$d.TMP
        VIB_S = [int]$d.VIB_S
        HUM_S = [int]$d.HUM_S
        LAB  = [int]$d.LAB
    }
}

function Run-Simulate {
    Write-Host "Mode simulation PC (carte non branchée)" -ForegroundColor Yellow
    $t = 0
    while ($true) {
        $t++
        $current = 5.0 + [Math]::Sin($t * 0.1) * 2
        $alm = if ($current -gt 20) { 2 } elseif ($current -gt 15) { 1 } else { 0 }
        $d = @{
            I = $current
            T1 = 35.0 + ($t % 10) * 0.5
            T2 = 38.0 + ($t % 8) * 0.3
            AMB = 25.0 + ($t % 5) * 0.2
            HUM = 48.0 + [Math]::Sin($t * 0.05) * 5
            VIB = 980 + ($t % 20) * 10
            ALM = $alm
            SRC = "HT-36V"
            INA = 1; TMP = 1; VIB_S = 1; HUM_S = 1; LAB = 1
        }
        Format-SensorTable $d
        Start-Sleep -Seconds $IntervalSec
    }
}

function Run-Serial {
    if ($Upload) {
        Write-Host "Flash firmware..." -ForegroundColor Cyan
        python -m platformio run -t upload --upload-port $Port
        Start-Sleep -Seconds 2
    }

    $com = Get-ComPort
    Write-Host "Connexion $com - LABON + MONITOR ON" -ForegroundColor Cyan

    $serial = New-Object System.IO.Ports.SerialPort $com, $Baud
    $serial.NewLine = "`n"
    $serial.ReadTimeout = 500
    $serial.Open()
    Start-Sleep -Milliseconds 800
    while ($serial.BytesToRead -gt 0) { $null = $serial.ReadExisting() }

    foreach ($cmd in @("LABON", "MONITOR ON")) {
        $serial.WriteLine($cmd)
        Start-Sleep -Milliseconds 400
        while ($serial.BytesToRead -gt 0) { $null = $serial.ReadExisting() }
    }

    try {
        while ($true) {
            Start-Sleep -Milliseconds 200
            while ($serial.BytesToRead -gt 0) {
                $line = $serial.ReadLine()
                $parsed = Parse-SensorLine $line
                if ($parsed) {
                    Format-SensorTable @{
                        I=$parsed.I; T1=$parsed.T1; T2=$parsed.T2; AMB=$parsed.AMB
                        HUM=$parsed.HUM; VIB=$parsed.VIB; ALM=$parsed.ALM; SRC=$parsed.SRC
                        INA=$parsed.INA; TMP=$parsed.TMP; VIB_S=$parsed.VIB_S; HUM_S=$parsed.HUM_S; LAB=$parsed.LAB
                    }
                }
            }
        }
    }
    finally {
        try { $serial.WriteLine("MONITOR OFF"); $serial.WriteLine("LABOFF") } catch {}
        $serial.Close()
        $serial.Dispose()
    }
}

if ($Simulate) {
    Run-Simulate
} else {
    Run-Serial
}
