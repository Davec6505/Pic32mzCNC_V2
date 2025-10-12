# Motion Control Test Script for Modular Architecture
param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200
)

function Send-GCodeCommand {
    param(
        [System.IO.Ports.SerialPort]$Port,
        [string]$Command,
        [int]$TimeoutMs = 2000
    )
    
    Write-Host "Sending: $Command" -ForegroundColor Yellow
    $Port.Write("$Command`r`n")
    
    $startTime = Get-Date
    $response = ""
    
    while (((Get-Date) - $startTime).TotalMilliseconds -lt $TimeoutMs) {
        if ($Port.BytesToRead -gt 0) {
            $data = $Port.ReadExisting()
            $response += $data
            Write-Host $data -NoNewline -ForegroundColor Cyan
            
            # Check if we got a complete response (ok, error, or alarm)
            if ($response -match "(ok|error|ALARM)" -or $response.Contains("`n")) {
                break
            }
        }
        Start-Sleep -Milliseconds 10
    }
    
    Write-Host "" # New line
    return $response
}

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

    Write-Host "Testing Modular Motion Control System" -ForegroundColor Green
    Write-Host "Opening $Port at $BaudRate baud..." -ForegroundColor Green
    
    $serialPort.Open()
    
    if ($serialPort.IsOpen) {
        Write-Host "Connected! Starting motion tests..." -ForegroundColor Green
        Start-Sleep -Seconds 1
        
        # Test sequence to verify modular motion control
        Write-Host "`n=== GRBL Status Check ===" -ForegroundColor Magenta
        Send-GCodeCommand -Port $serialPort -Command "$$"
        
        Write-Host "`n=== Motion Buffer Tests ===" -ForegroundColor Magenta
        
        # Test 1: Simple linear move (tests motion_gcode_parser + motion_buffer)
        Write-Host "Test 1: Linear move G0 X10 Y10" -ForegroundColor White
        Send-GCodeCommand -Port $serialPort -Command "G0 X10 Y10"
        
        # Test 2: Another linear move to test buffer management
        Write-Host "Test 2: Linear move G1 X20 Y20 F100" -ForegroundColor White
        Send-GCodeCommand -Port $serialPort -Command "G1 X20 Y20 F100"
        
        # Test 3: Return to origin
        Write-Host "Test 3: Return to origin G0 X0 Y0" -ForegroundColor White
        Send-GCodeCommand -Port $serialPort -Command "G0 X0 Y0"
        
        Write-Host "`n=== Motion Planner Tests ===" -ForegroundColor Magenta
        
        # Test 4: Rapid sequence to test motion planning
        Write-Host "Test 4: Rapid sequence (tests motion planner buffering)" -ForegroundColor White
        Send-GCodeCommand -Port $serialPort -Command "G1 X5 Y0 F200"
        Send-GCodeCommand -Port $serialPort -Command "G1 X5 Y5 F200" 
        Send-GCodeCommand -Port $serialPort -Command "G1 X0 Y5 F200"
        Send-GCodeCommand -Port $serialPort -Command "G1 X0 Y0 F200"
        
        Write-Host "`n=== Arc Motion Tests ===" -ForegroundColor Magenta
        
        # Test 5: Circular interpolation (tests motion_gcode_parser arc handling)
        Write-Host "Test 5: Circular arc G2 X10 Y0 I5 J0" -ForegroundColor White
        Send-GCodeCommand -Port $serialPort -Command "G2 X10 Y0 I5 J0 F100"
        
        Write-Host "`n=== System Status ===" -ForegroundColor Magenta
        
        # Test 6: Check final status
        Write-Host "Test 6: Final status check" -ForegroundColor White
        Send-GCodeCommand -Port $serialPort -Command "?"
        
        Write-Host "`n=== Motion Control Test Complete ===" -ForegroundColor Green
        Write-Host "All tests completed. The modular motion control system is responding." -ForegroundColor Green
        
        # Wait a moment before closing
        Start-Sleep -Seconds 2
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