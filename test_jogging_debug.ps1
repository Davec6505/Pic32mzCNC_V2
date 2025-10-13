#!/usr/bin/env pwsh
# Test script for jogging debug output
param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200
)

Write-Host "=== Jogging Debug Test ===" -ForegroundColor Green
Write-Host "Testing motion debug output with jog commands..."

try {
    # Open serial port
    $serial = New-Object System.IO.Ports.SerialPort
    $serial.PortName = $Port
    $serial.BaudRate = $BaudRate
    $serial.DataBits = 8
    $serial.Parity = [System.IO.Ports.Parity]::None
    $serial.StopBits = [System.IO.Ports.StopBits]::One
    $serial.ReadTimeout = 1000
    $serial.WriteTimeout = 1000

    Write-Host "Opening $Port at $BaudRate baud..." -ForegroundColor Yellow
    $serial.Open()
    Write-Host "Connected!" -ForegroundColor Green
    
    # Wait for system startup
    Start-Sleep -Milliseconds 2000
    
    # Clear any startup messages
    if ($serial.BytesToRead -gt 0) {
        $startup = $serial.ReadExisting()
        Write-Host "Startup messages:" -ForegroundColor Gray
        Write-Host $startup -ForegroundColor Gray
    }
    
    # Test 1: Simple jog command
    Write-Host "`n=== Test 1: Simple X-axis jog ===" -ForegroundColor Cyan
    $command1 = '$J=G21G91X-1F99'
    Write-Host "Sending: $command1" -ForegroundColor Yellow
    $serial.WriteLine($command1)
    
    # Wait and collect responses for 3 seconds
    Write-Host "Collecting debug output..." -ForegroundColor Gray
    for ($i = 0; $i -lt 30; $i++) {
        Start-Sleep -Milliseconds 100
        if ($serial.BytesToRead -gt 0) {
            $response = $serial.ReadExisting()
            Write-Host $response -NoNewline -ForegroundColor White
        }
    }
    
    # Test 2: Y-axis jog
    Write-Host "`n`n=== Test 2: Y-axis jog ===" -ForegroundColor Cyan
    $command2 = '$J=G21G91Y1F99'
    Write-Host "Sending: $command2" -ForegroundColor Yellow
    $serial.WriteLine($command2)
    
    # Wait and collect responses
    Write-Host "Collecting debug output..." -ForegroundColor Gray
    for ($i = 0; $i -lt 30; $i++) {
        Start-Sleep -Milliseconds 100
        if ($serial.BytesToRead -gt 0) {
            $response = $serial.ReadExisting()
            Write-Host $response -NoNewline -ForegroundColor White
        }
    }
    
    # Test 3: Status query to check position
    Write-Host "`n`n=== Test 3: Status Query ===" -ForegroundColor Cyan
    Write-Host "Sending status query..." -ForegroundColor Yellow
    $serial.WriteLine("?")
    
    # Wait for status response
    for ($i = 0; $i -lt 10; $i++) {
        Start-Sleep -Milliseconds 100
        if ($serial.BytesToRead -gt 0) {
            $response = $serial.ReadExisting()
            Write-Host $response -NoNewline -ForegroundColor Magenta
        }
    }
    
    Write-Host "`n`nTest completed!" -ForegroundColor Green
    
} catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
} finally {
    if ($serial -and $serial.IsOpen) {
        $serial.Close()
        Write-Host "Serial port closed." -ForegroundColor Yellow
    }
}

Write-Host "`nLook for these debug messages:" -ForegroundColor Cyan
Write-Host "  [DEBUG: Motion added to buffer]" -ForegroundColor White
Write-Host "  [DEBUG: Planner state = X]" -ForegroundColor White  
Write-Host "  [DEBUG: Target X=... Y=... Z=...]" -ForegroundColor White
Write-Host "  [BUFFER_ADD] head=X tail=Y" -ForegroundColor White
Write-Host "  [DEBUG: Motion planner got new block]" -ForegroundColor White
Write-Host "  [DEBUG: Block target X=... Y=... Z=...]" -ForegroundColor White