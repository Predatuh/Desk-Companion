param(
    [string]$Port
)

$ErrorActionPreference = 'Stop'

$arduinoCli = 'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
$repoRoot = Split-Path -Parent $PSScriptRoot
$sketchPath = Join-Path $repoRoot 'esp32_firmware\tft_28\tft_28.ino'
$fqbn = 'esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc'

if (-not (Test-Path $arduinoCli)) {
    throw "arduino-cli not found at $arduinoCli"
}

if (-not (Test-Path $sketchPath)) {
    throw "Sketch not found at $sketchPath"
}

if (-not $Port) {
    $ports = Get-CimInstance Win32_SerialPort |
        Where-Object {
            $_.Name -match 'USB|CP210|CH340|ESP32|Silicon Labs|UART' -or
            $_.Description -match 'USB|CP210|CH340|ESP32|Silicon Labs|UART'
        }

    if ($ports.Count -eq 1) {
        $Port = $ports[0].DeviceID
    } elseif ($ports.Count -gt 1) {
        $portList = $ports | Select-Object DeviceID, Name | Format-Table -AutoSize | Out-String
        throw "Multiple serial ports detected. Re-run with -Port COMx.`n$portList"
    } else {
        throw 'No ESP32 serial port detected. Plug the board in over USB and re-run.'
    }
}

Write-Host "Using port: $Port"
Write-Host "Compiling and uploading $sketchPath"

& $arduinoCli compile --upload --port $Port --fqbn $fqbn $sketchPath