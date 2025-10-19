#!/usr/bin/env pwsh
# test_position_reset.ps1 - Reset position and verify 5mm move
# October 19, 2025

param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200
)

Write-Host "=== Position Reset Test ===" -ForegroundColor Cyan
Write-Host "Commands: G92 X0 Y0 Z0 → G0 X5 → Verify position = 5.000mm" -ForegroundColor Yellow
Write-Host ""

# Open serial port
$serial = New-Object System.IO.Ports.SerialPort
$serial.PortName = $Port
$serial.BaudRate = $BaudRate
$serial.DataBits = 8
$serial.Parity = [System.IO.Ports.Parity]::None
$serial.StopBits = [System.IO.Ports.StopBits]::One
$serial.Handshake = [System.IO.Ports.Handshake]::None
$serial.ReadTimeout = 2000
$serial.WriteTimeout = 1000

function Send-Command {
    param([string]$cmd)
    
    Write-Host "[SEND] $cmd" -ForegroundColor Cyan
    $serial.WriteLine($cmd)
    Start-Sleep -Milliseconds 100
    
    # Read response
    $response = ""
    $timeout = [DateTime]::Now.AddSeconds(2)
    while ([DateTime]::Now -lt $timeout) {
        if ($serial.BytesToRead -gt 0) {
            $line = $serial.ReadLine()
            $response += $line + "`n"
            Write-Host "[RECV] $line" -ForegroundColor Gray
            if ($line -match "^ok") {
                break
            }
        }
        Start-Sleep -Milliseconds 50
    }
    
    return $response
}

try {
    $serial.Open()
    Write-Host "[CONNECTED]" -ForegroundColor Green
    Start-Sleep -Milliseconds 500
    
    # Flush startup
    while ($serial.BytesToRead -gt 0) {
        $null = $serial.ReadExisting()
    }
    
    # Reset work coordinates
    Send-Command "G92 X0 Y0 Z0"
    Start-Sleep -Milliseconds 200
    
    # Query position (should be 0,0,0)
    Write-Host "`n[QUERY] Current position after G92..." -ForegroundColor Yellow
    $serial.Write("?")
    Start-Sleep -Milliseconds 200
    if ($serial.BytesToRead -gt 0) {
        $pos = $serial.ReadLine()
        Write-Host $pos -ForegroundColor White
        if ($pos -match "MPos:(\d+\.\d+),(\d+\.\d+),(\d+\.\d+)") {
            $x = [float]$matches[1]
            $y = [float]$matches[2]
            $z = [float]$matches[3]
            Write-Host "  Position: X=$x, Y=$y, Z=$z" -ForegroundColor White
        }
    }
    
    # Send G0 X5
    Write-Host "`n[TEST] Sending G0 X5 (expect 5mm move)..." -ForegroundColor Yellow
    Send-Command "G0 X5"
    
    # Wait for motion
    Start-Sleep -Seconds 2
    
    # Query final position
    Write-Host "`n[QUERY] Final position..." -ForegroundColor Yellow
    $serial.Write("?")
    Start-Sleep -Milliseconds 200
    if ($serial.BytesToRead -gt 0) {
        $pos = $serial.ReadLine()
        Write-Host $pos -ForegroundColor White
        if ($pos -match "MPos:(\d+\.\d+),(\d+\.\d+),(\d+\.\d+)") {
            $x = [float]$matches[1]
            $y = [float]$matches[2]
            $z = [float]$matches[3]
            Write-Host "  Position: X=$x, Y=$y, Z=$z" -ForegroundColor White
            
            # Verify
            Write-Host "`n=== RESULT ===" -ForegroundColor Cyan
            if ([Math]::Abs($x - 5.0) < 0.1) {
                Write-Host "✅ SUCCESS: X-axis at $x mm (expected 5.0mm)" -ForegroundColor Green
            } else {
                Write-Host "❌ FAIL: X-axis at $x mm (expected 5.0mm)" -ForegroundColor Red
            }
        }
    }
}
catch {
    Write-Host "ERROR: $_" -ForegroundColor Red
}
finally {
    if ($serial.IsOpen) {
        $serial.Close()
        Write-Host "`n[DISCONNECTED]" -ForegroundColor Yellow
    }
}
