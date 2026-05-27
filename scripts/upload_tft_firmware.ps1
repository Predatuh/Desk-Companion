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

# ── Step 1: Compile only, output binary to a known folder ──
$buildOut = Join-Path $env:TEMP 'desk_companion_fw'
New-Item -ItemType Directory -Force -Path $buildOut | Out-Null

Write-Host "Compiling sketch..."
& $arduinoCli compile --fqbn $fqbn --output-dir $buildOut $sketchPath
if ($LASTEXITCODE -ne 0) { throw "Compile failed (exit $LASTEXITCODE)" }

$binFile = Join-Path $buildOut 'tft_28.ino.bin'
if (-not (Test-Path $binFile)) {
    # arduino-cli sometimes names it differently; grab the first .bin
    $binFile = (Get-ChildItem $buildOut -Filter '*.bin' | Select-Object -First 1).FullName
}
if (-not $binFile) { throw "Compiled .bin not found in $buildOut" }
Write-Host "Binary: $binFile"

# ── Step 2: Prompt user to enter bootloader manually (HWCDC boards) ──
Write-Host ""
Write-Host "*** MANUAL STEP REQUIRED ***"
Write-Host "This board uses Hardware CDC (HWCDC) and does not support auto-reset."
Write-Host ""
Write-Host "Put the device in bootloader mode now:"
Write-Host "  1. Hold the BOOT button"
Write-Host "  2. While holding BOOT, press and release RESET"
Write-Host "  3. Release BOOT"
Write-Host "  (The display will go blank - that means bootloader is active)"
Write-Host ""
Read-Host "Press Enter when the device is in bootloader mode"

# ── Step 3: Flash with esptool using --before no-reset ──
$esptool = 'C:\Users\tanne\AppData\Local\Arduino15\packages\esp32\tools\esptool_py\5.1.0\esptool.exe'
Write-Host "Flashing..."
& $esptool --chip esp32s3 --port $Port --baud 921600 `
    --before no-reset --after hard-reset `
    write-flash -z 0x10000 $binFile
if ($LASTEXITCODE -ne 0) { throw "esptool flash failed (exit $LASTEXITCODE)" }

Write-Host "Upload complete."