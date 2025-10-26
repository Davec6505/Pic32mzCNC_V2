# Direct Serial Communication Test Script
# Bypasses UGS to see ALL debug output including [MSG:...] messages
#
# Usage: .\test_direct_serial.ps1 -Port COM4 -BaudRate 115200

param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200,
    [int]$ReadTimeout = 2000,
    [int]$WriteTimeout = 1000
)

# Color coding for output
function Write-Sent { param($msg) Write-Host ">> $msg" -ForegroundColor Cyan }
function Write-Received { param($msg) Write-Host "<< $msg" -ForegroundColor Green }
function Write-Debug { param($msg) Write-Host "[DEBUG] $msg" -ForegroundColor Yellow }
function Write-Error { param($msg) Write-Host "[ERROR] $msg" -ForegroundColor Red }

# Open serial port
Write-Host "`n=== Opening Serial Port ===" -ForegroundColor White
Write-Host "Port: $Port @ $BaudRate baud" -ForegroundColor White

try {
    $serialPort = New-Object System.IO.Ports.SerialPort
    $serialPort.PortName = $Port
    $serialPort.BaudRate = $BaudRate
    $serialPort.DataBits = 8
    $serialPort.Parity = [System.IO.Ports.Parity]::None
    $serialPort.StopBits = [System.IO.Ports.StopBits]::One
    $serialPort.Handshake = [System.IO.Ports.Handshake]::None
    $serialPort.ReadTimeout = $ReadTimeout
    $serialPort.WriteTimeout = $WriteTimeout
    $serialPort.NewLine = "`n"
    
    $serialPort.Open()
    Write-Host "Serial port opened successfully!`n" -ForegroundColor Green
    
    # Wait for boot messages
    Start-Sleep -Milliseconds 500
    
    # Flush any startup messages
    while ($serialPort.BytesToRead -gt 0) {
        $line = $serialPort.ReadLine()
        Write-Received $line
    }
    
    Write-Host "`n=== Starting Command Stream ===" -ForegroundColor White
    Write-Host "Watching for [MSG:...] debug output`n" -ForegroundColor Yellow
    
    # Test commands - simple sequence to trigger debug output
    $commands = @(
        "?",              # Status query
        "`$I",            # Build info
        "G21",            # Units mm
        "G90",            # Absolute mode
        "G0 Z5 F1500",   # Rapid to Z5
        "G0 X0 Y0",       # Rapid to origin
        "G0 Z0",          # Rapid to Z0
        "G2 X10 Y0 I5 J0 F1000",  # ARC COMMAND - should trigger our debug!
        "G0 Z5",          # Rapid back up
        "G0 X0 Y0",       # Return to origin
        "?"               # Final status
    )
    
    foreach ($cmd in $commands) {
        # Send command
        Write-Sent $cmd
        $serialPort.WriteLine($cmd)
        
        # Read response with timeout
        $responseComplete = $false
        $startTime = Get-Date
        $timeout = 3.0  # 3 second timeout per command
        
        while (-not $responseComplete -and ((Get-Date) - $startTime).TotalSeconds -lt $timeout) {
            if ($serialPort.BytesToRead -gt 0) {
                try {
                    $line = $serialPort.ReadLine().Trim()
                    if ($line -ne "") {
                        Write-Received $line
                        
                        # Highlight our debug messages
                        if ($line -match "\[MSG:") {
                            Write-Host "    ^^^ DEBUG MESSAGE FOUND! ^^^" -ForegroundColor Magenta
                        }
                        
                        # Check for command completion
                        if ($line -eq "ok" -or $line -match "^error:") {
                            $responseComplete = $true
                        }
                    }
                }
                catch {
                    # Timeout on ReadLine, continue
                }
            }
            Start-Sleep -Milliseconds 10
        }
        
        if (-not $responseComplete) {
            Write-Error "Timeout waiting for response to: $cmd"
        }
        
        # Small delay between commands
        Start-Sleep -Milliseconds 100
    }
    
    Write-Host "`n=== Command Stream Complete ===" -ForegroundColor White
    Write-Host "Waiting 2 seconds for any final debug output...`n" -ForegroundColor Yellow
    
    # Wait for any final output
    $finalWait = 2.0
    $startTime = Get-Date
    while (((Get-Date) - $startTime).TotalSeconds -lt $finalWait) {
        if ($serialPort.BytesToRead -gt 0) {
            try {
                $line = $serialPort.ReadLine().Trim()
                if ($line -ne "") {
                    Write-Received $line
                    
                    if ($line -match "\[MSG:") {
                        Write-Host "    ^^^ DEBUG MESSAGE FOUND! ^^^" -ForegroundColor Magenta
                    }
                }
            }
            catch {
                # Ignore timeout
            }
        }
        Start-Sleep -Milliseconds 50
    }
    
    Write-Host "`n=== Test Complete ===" -ForegroundColor White
    
} catch {
    Write-Error "Exception: $_"
} finally {
    if ($serialPort -and $serialPort.IsOpen) {
        $serialPort.Close()
        Write-Host "Serial port closed.`n" -ForegroundColor White
    }
}

Write-Host @"

=== Summary ===
If you saw [MSG:...] messages above, our debug code IS working!
If you did NOT see [MSG:...] messages, the code paths are not executing.

Expected debug messages:
- [MSG:MAINLOOP ...] - Should print every ~50 seconds during idle
- [MSG:SERIAL ...] - Should print when commands are tokenized
- [MSG:CMDBUF ...] - Should print when command buffer is checked
- [MSG:PARSE ...] - Should print for each parsed command

"@ -ForegroundColor Cyan
