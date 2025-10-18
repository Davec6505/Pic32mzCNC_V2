#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Debug script to test GRBL responses without UGS
    
.DESCRIPTION
    Opens serial port, sends commands, shows RAW responses
    Helps debug what UGS is receiving that it doesn't like
    
.PARAMETER Port
    COM port (default: COM4)
    
.PARAMETER BaudRate
    Baud rate (default: 115200)
    
.EXAMPLE
    .\ugs_debug.ps1 -Port COM4 -BaudRate 115200
#>

param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200
)

# Create serial port object
$serialPort = New-Object System.IO.Ports.SerialPort
$serialPort.PortName = $Port
$serialPort.BaudRate = $BaudRate
$serialPort.DataBits = 8
$serialPort.Parity = [System.IO.Ports.Parity]::None
$serialPort.StopBits = [System.IO.Ports.StopBits]::One
$serialPort.Handshake = [System.IO.Ports.Handshake]::None
$serialPort.ReadTimeout = 2000
$serialPort.WriteTimeout = 2000
$serialPort.NewLine = "`n"

# Open port
try {
    Write-Host "=== Opening $Port @ $BaudRate baud ===" -ForegroundColor Cyan
    $serialPort.Open()
    Write-Host "=== Port opened successfully ===" -ForegroundColor Green
    Start-Sleep -Milliseconds 500
    
    # Discard any startup messages
    if ($serialPort.BytesToRead -gt 0) {
        $startup = $serialPort.ReadExisting()
        Write-Host "`n=== STARTUP MESSAGES ===" -ForegroundColor Yellow
        Write-Host $startup -ForegroundColor Gray
    }
    
    # Test 1: Send '?' (status request)
    Write-Host "`n=== TEST 1: Status Request (?) ===" -ForegroundColor Cyan
    Write-Host ">>> Sending: '?'" -ForegroundColor White
    $serialPort.WriteLine("?")
    Start-Sleep -Milliseconds 200
    
    if ($serialPort.BytesToRead -gt 0) {
        $response = $serialPort.ReadExisting()
        Write-Host "<<< Response ($($response.Length) bytes):" -ForegroundColor Green
        Write-Host $response -ForegroundColor White
        
        # Show hex dump of response
        Write-Host "<<< HEX DUMP:" -ForegroundColor Magenta
        $bytes = [System.Text.Encoding]::ASCII.GetBytes($response)
        $hexString = ($bytes | ForEach-Object { $_.ToString("X2") }) -join " "
        Write-Host $hexString -ForegroundColor Gray
    }
    else {
        Write-Host "<<< NO RESPONSE!" -ForegroundColor Red
    }
    
    # Test 2: Send '$I' (build info)
    Write-Host "`n=== TEST 2: Build Info (`$I) ===" -ForegroundColor Cyan
    Write-Host ">>> Sending: '`$I'" -ForegroundColor White
    $serialPort.WriteLine("`$I")
    Start-Sleep -Milliseconds 200
    
    if ($serialPort.BytesToRead -gt 0) {
        $response = $serialPort.ReadExisting()
        Write-Host "<<< Response ($($response.Length) bytes):" -ForegroundColor Green
        Write-Host $response -ForegroundColor White
        
        # Show hex dump of response
        Write-Host "<<< HEX DUMP:" -ForegroundColor Magenta
        $bytes = [System.Text.Encoding]::ASCII.GetBytes($response)
        $hexString = ($bytes | ForEach-Object { $_.ToString("X2") }) -join " "
        Write-Host $hexString -ForegroundColor Gray
        
        # Check for expected GRBL format
        if ($response -match "GRBL|Grbl") {
            Write-Host "✓ Contains 'Grbl' identifier" -ForegroundColor Green
        }
        else {
            Write-Host "✗ Missing 'Grbl' identifier" -ForegroundColor Red
        }
        
        if ($response -match "ok") {
            Write-Host "✓ Contains 'ok' response" -ForegroundColor Green
        }
        else {
            Write-Host "✗ Missing 'ok' response" -ForegroundColor Red
        }
    }
    else {
        Write-Host "<<< NO RESPONSE!" -ForegroundColor Red
    }
    
    # Test 3: Send '$G' (parser state)
    Write-Host "`n=== TEST 3: Parser State (`$G) ===" -ForegroundColor Cyan
    Write-Host ">>> Sending: '`$G'" -ForegroundColor White
    $serialPort.WriteLine("`$G")
    Start-Sleep -Milliseconds 200
    
    if ($serialPort.BytesToRead -gt 0) {
        $response = $serialPort.ReadExisting()
        Write-Host "<<< Response ($($response.Length) bytes):" -ForegroundColor Green
        Write-Host $response -ForegroundColor White
        
        # Show hex dump of response
        Write-Host "<<< HEX DUMP:" -ForegroundColor Magenta
        $bytes = [System.Text.Encoding]::ASCII.GetBytes($response)
        $hexString = ($bytes | ForEach-Object { $_.ToString("X2") }) -join " "
        Write-Host $hexString -ForegroundColor Gray
    }
    else {
        Write-Host "<<< NO RESPONSE!" -ForegroundColor Red
    }
    
    # Test 4: Send '$$' (view settings)
    Write-Host "`n=== TEST 4: View Settings (`$`$) ===" -ForegroundColor Cyan
    Write-Host ">>> Sending: '`$`$'" -ForegroundColor White
    $serialPort.WriteLine("`$`$")
    Start-Sleep -Milliseconds 500  # Settings take longer
    
    if ($serialPort.BytesToRead -gt 0) {
        $response = $serialPort.ReadExisting()
        Write-Host "<<< Response ($($response.Length) bytes):" -ForegroundColor Green
        Write-Host $response -ForegroundColor White
    }
    else {
        Write-Host "<<< NO RESPONSE!" -ForegroundColor Red
    }
    
    # Test 5: Mirror UGS G-code sequence (Square test pattern)
    Write-Host "`n=== TEST 5: G-code Square Pattern (Mirror UGS) ===" -ForegroundColor Cyan
    
    # Setup commands
    $setupCommands = @("G21", "G90", "G17", "G94", "M3S1000")
    foreach ($cmd in $setupCommands) {
        Write-Host ">>> Sending: '$cmd'" -ForegroundColor White
        $serialPort.WriteLine($cmd)
        Start-Sleep -Milliseconds 100
        if ($serialPort.BytesToRead -gt 0) {
            $response = $serialPort.ReadExisting()
            Write-Host "<<< $response" -ForegroundColor Gray
        }
    }
    
    # G0 commands (rapids)
    Write-Host "`n--- Rapid Moves ---" -ForegroundColor Yellow
    $rapidCommands = @("G0Z5", "G0X0Y0", "G0Z0")
    foreach ($cmd in $rapidCommands) {
        Write-Host ">>> Sending: '$cmd'" -ForegroundColor White
        $serialPort.WriteLine($cmd)
        Start-Sleep -Milliseconds 100
        if ($serialPort.BytesToRead -gt 0) {
            $response = $serialPort.ReadExisting()
            Write-Host "<<< $response" -ForegroundColor Gray
        }
    }
    
    # Square pattern (G1 linear moves)
    Write-Host "`n--- Square Pattern (4 moves) ---" -ForegroundColor Yellow
    $squareCommands = @("G1X0Y0F1000", "G1Y10", "G1X10", "G1Y0", "G1X0")
    
    foreach ($cmd in $squareCommands) {
        Write-Host ">>> Sending: '$cmd'" -ForegroundColor White
        $serialPort.WriteLine($cmd)
        Start-Sleep -Milliseconds 100
        
        # Read any immediate response (ok, debug messages)
        if ($serialPort.BytesToRead -gt 0) {
            $response = $serialPort.ReadExisting()
            Write-Host "<<< $response" -ForegroundColor Gray
        }
        
        # Poll status during motion (send '?' every 200ms for 2 seconds)
        Write-Host "    Polling status..." -ForegroundColor DarkGray
        for ($i = 0; $i -lt 10; $i++) {
            $serialPort.Write("?")  # Don't use WriteLine for '?'
            Start-Sleep -Milliseconds 200
            
            if ($serialPort.BytesToRead -gt 0) {
                $status = $serialPort.ReadExisting()
                # Parse position from status
                if ($status -match "<(.+?)\|MPos:([^>]+)>") {
                    $state = $matches[1]
                    $pos = $matches[2]
                    Write-Host "    [$state] MPos: $pos" -ForegroundColor Cyan
                }
            }
        }
    }
    
    # Spindle off
    Write-Host "`n--- Cleanup ---" -ForegroundColor Yellow
    Write-Host ">>> Sending: 'M5'" -ForegroundColor White
    $serialPort.WriteLine("M5")
    Start-Sleep -Milliseconds 100
    if ($serialPort.BytesToRead -gt 0) {
        $response = $serialPort.ReadExisting()
        Write-Host "<<< $response" -ForegroundColor Gray
    }
    
    # Final status check
    Write-Host "`n--- Final Position ---" -ForegroundColor Yellow
    for ($i = 0; $i -lt 3; $i++) {
        $serialPort.Write("?")
        Start-Sleep -Milliseconds 200
        if ($serialPort.BytesToRead -gt 0) {
            $status = $serialPort.ReadExisting()
            Write-Host "<<< $status" -ForegroundColor Green
        }
    }
    
    Write-Host "`n=== Tests Complete ===" -ForegroundColor Green
}
catch {
    Write-Host "ERROR: $_" -ForegroundColor Red
}
finally {
    if ($serialPort.IsOpen) {
        $serialPort.Close()
        Write-Host "`n=== Port closed ===" -ForegroundColor Cyan
    }
}
