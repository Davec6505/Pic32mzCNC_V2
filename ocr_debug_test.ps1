#!/usr/bin/env pwsh
# Test with very slow motion to catch OCR settings

Write-Host "=== OCR Debug Test ===" -ForegroundColor Cyan
Write-Host "Testing with very slow motion..."

$port = "COM4"
$baud = 115200

# Open serial port
$serial = New-Object System.IO.Ports.SerialPort
$serial.PortName = $port
$serial.BaudRate = $baud
$serial.DataBits = 8
$serial.Parity = [System.IO.Ports.Parity]::None
$serial.StopBits = [System.IO.Ports.StopBits]::One
$serial.ReadTimeout = 1000
$serial.WriteTimeout = 1000

try {
    Write-Host "Opening $port..."
    $serial.Open()
    Write-Host "Connected!" -ForegroundColor Green
    
    Start-Sleep -Milliseconds 1000
    
    Write-Host "`nSending very slow motion command..." -ForegroundColor Yellow
    $serial.WriteLine('G1 X10 F10')  # Very slow: 10mm at 10mm/min
    Start-Sleep -Milliseconds 100
    
    # Read any immediate responses
    for ($i = 0; $i -lt 10; $i++) {
        try {
            $response = $serial.ReadLine().Trim()
            if ($response) {
                Write-Host "Response $i`: $response" -ForegroundColor Green
            }
        }
        catch {
            break
        }
    }
    
    Write-Host "`nWaiting 2 seconds then checking status during motion..." -ForegroundColor Gray
    Start-Sleep -Milliseconds 2000
    
    $serial.WriteLine('DEBUG')
    Start-Sleep -Milliseconds 100
    
    # Read debug response
    for ($i = 0; $i -lt 5; $i++) {
        try {
            $response = $serial.ReadLine().Trim()
            if ($response) {
                Write-Host "Debug $i`: $response" -ForegroundColor Cyan
            }
        }
        catch {
            break
        }
    }
    
} catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
} finally {
    if ($serial.IsOpen) {
        $serial.Close()
        Write-Host "`nSerial port closed." -ForegroundColor Yellow
    }
}

Write-Host "`nLook for [OCR_SET] messages above!" -ForegroundColor Cyan