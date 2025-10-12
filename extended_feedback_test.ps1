# Extended Position Feedback Test for CNC System
# This test sends a sequence of slow motion commands to verify position feedback
param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200,
    [int]$DelayBetweenCommands = 3000  # 3 seconds between commands
)

Write-Host "=== Extended CNC Position Feedback Test ===" -ForegroundColor Green
Write-Host "Port: $Port | Baud: $BaudRate | Command Delay: ${DelayBetweenCommands}ms" -ForegroundColor Yellow

try {
    # Load .NET serial port class
    Add-Type -AssemblyName System.IO.Ports

    # Create and configure serial port
    $serialPort = New-Object System.IO.Ports.SerialPort
    $serialPort.PortName = $Port
    $serialPort.BaudRate = $BaudRate
    $serialPort.DataBits = 8
    $serialPort.Parity = [System.IO.Ports.Parity]::None
    $serialPort.StopBits = [System.IO.Ports.StopBits]::One
    $serialPort.ReadTimeout = 1000
    $serialPort.WriteTimeout = 1000

    Write-Host "Opening $Port..." -ForegroundColor Green
    $serialPort.Open()
    
    if ($serialPort.IsOpen) {
        Write-Host "Connected! Starting extended test sequence..." -ForegroundColor Green
        
        # Test sequence with slow motion commands
        $testCommands = @(
            "$$",                    # Get settings
            "`$`$",                  # Alternative settings command
            "?",                     # Status report
            "G0 X0 Y0 Z0",          # Move to origin (rapid)
            "?",                     # Check position after origin
            "F100",                  # Set very slow feed rate (100 mm/min)
            "G1 X10",               # Slow move X+10mm
            "?",                     # Check position
            "G1 Y5",                # Slow move Y+5mm  
            "?",                     # Check position
            "G1 Z2",                # Slow move Z+2mm
            "?",                     # Check position
            "G1 X20 Y10",           # Diagonal move (slow)
            "?",                     # Check position
            "G1 X0 Y0",             # Return to XY origin (slow)
            "?",                     # Check position
            "G1 Z0",                # Return Z to origin (slow)
            "?",                     # Final position check
            "F500",                  # Restore normal feed rate
            "M2"                     # Program end
        )

        Write-Host "`nSending test sequence (${testCommands.Length} commands):" -ForegroundColor Cyan
        
        $commandIndex = 1
        foreach ($command in $testCommands) {
            Write-Host "`n[$commandIndex/${testCommands.Length}] Sending: '$command'" -ForegroundColor Yellow
            
            # Send command
            $serialPort.Write("$command`r`n")
            
            # Wait for response and display
            $responseStartTime = Get-Date
            $responseReceived = $false
            $responseBuffer = ""
            
            do {
                Start-Sleep -Milliseconds 100
                
                if ($serialPort.BytesToRead -gt 0) {
                    $data = $serialPort.ReadExisting()
                    $responseBuffer += $data
                    Write-Host $data -NoNewline -ForegroundColor Cyan
                    
                    # Check if we got a complete response (ends with ok, error, or status)
                    if ($responseBuffer -match "(ok|error|\>)" -or $responseBuffer.Length -gt 500) {
                        $responseReceived = $true
                    }
                }
                
                $elapsed = (Get-Date) - $responseStartTime
            } while (-not $responseReceived -and $elapsed.TotalMilliseconds -lt 5000)
            
            if (-not $responseReceived) {
                Write-Host "`n  [WARNING] No response received within 5 seconds" -ForegroundColor Red
            }
            
            # Wait between commands (longer for motion commands)
            if ($command -match "^G[01]") {
                Write-Host "`n  [Waiting ${DelayBetweenCommands}ms for motion to complete...]" -ForegroundColor Gray
                Start-Sleep -Milliseconds $DelayBetweenCommands
                
                # Send status request after motion commands
                Write-Host "  [Requesting status after motion...]" -ForegroundColor Gray
                $serialPort.Write("?`r`n")
                Start-Sleep -Milliseconds 500
                
                if ($serialPort.BytesToRead -gt 0) {
                    $statusData = $serialPort.ReadExisting()
                    Write-Host "  Status: " -NoNewline -ForegroundColor Green
                    Write-Host $statusData -NoNewline -ForegroundColor Cyan
                }
            } else {
                Start-Sleep -Milliseconds 500
            }
            
            $commandIndex++
        }
        
        Write-Host "`n`n=== Test Sequence Complete ===" -ForegroundColor Green
        Write-Host "You can now manually send commands. Press ESC to exit, Enter to send commands." -ForegroundColor Yellow
        
        # Continue with interactive mode
        while ($true) {
            try {
                # Read any incoming data
                if ($serialPort.BytesToRead -gt 0) {
                    $data = $serialPort.ReadExisting()
                    Write-Host $data -NoNewline -ForegroundColor Cyan
                }
                
                # Check for keyboard input
                if ([Console]::KeyAvailable) {
                    $key = [Console]::ReadKey($true)
                    if ($key.Key -eq 'Escape') {
                        break
                    }
                    
                    # Send the key to serial port
                    $serialPort.Write($key.KeyChar.ToString())
                    Write-Host $key.KeyChar -NoNewline -ForegroundColor White
                    
                    # Send CR+LF on Enter
                    if ($key.Key -eq 'Enter') {
                        $serialPort.Write("`r`n")
                        Write-Host ""
                    }
                }
                
                Start-Sleep -Milliseconds 50
            }
            catch {
                Write-Host "Error in interactive mode: $_" -ForegroundColor Red
                break
            }
        }
    }
}
catch {
    Write-Host "Error: $_" -ForegroundColor Red
}
finally {
    if ($serialPort -and $serialPort.IsOpen) {
        $serialPort.Close()
        Write-Host "`nSerial port closed." -ForegroundColor Green
    }
}

Write-Host "`nTest completed. Check the position feedback above!" -ForegroundColor Green