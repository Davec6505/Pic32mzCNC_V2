# Test optimized position tracking using step counters
$port = "COM4"
$baudRate = 115200

# Open serial connection
try {
    $serialPort = New-Object System.IO.Ports.SerialPort
    $serialPort.PortName = $port
    $serialPort.BaudRate = $baudRate
    $serialPort.DataBits = 8
    $serialPort.Parity = "None"
    $serialPort.StopBits = "One"
    $serialPort.Handshake = "None"
    $serialPort.ReadTimeout = 2000
    $serialPort.WriteTimeout = 2000
    
    $serialPort.Open()
    Write-Host "Connected to $port at $baudRate baud" -ForegroundColor Green
    
    # Wait for system to initialize
    Start-Sleep -Milliseconds 500
    
    # Clear any pending data
    $serialPort.DiscardInBuffer()
    $serialPort.DiscardOutBuffer()
    
    Write-Host "`n=== Testing Optimized Position Tracking ===" -ForegroundColor Yellow
    
    # Test sequence with position reporting
    $testSequence = @(
        @{ cmd = "G0 X0 Y0 Z0"; desc = "Move to origin" },
        @{ cmd = "?"; desc = "Get initial position" },
        @{ cmd = "G0 X10 Y10"; desc = "Move to X10 Y10" },
        @{ cmd = "?"; desc = "Get position after move" },
        @{ cmd = "G1 X20 Y20 F100"; desc = "Linear move to X20 Y20" },
        @{ cmd = "?"; desc = "Get position after linear move" },
        @{ cmd = "`$`$"; desc = "Get detailed status with step counts" },
        @{ cmd = "G0 X0 Y0"; desc = "Return to origin" },
        @{ cmd = "?"; desc = "Final position check" }
    )
    
    foreach ($test in $testSequence) {
        Write-Host "`nSending: '$($test.cmd)' - $($test.desc)" -ForegroundColor Cyan
        $serialPort.WriteLine($test.cmd)
        
        # Wait for response
        $startTime = Get-Date
        $response = ""
        
        while (((Get-Date) - $startTime).TotalMilliseconds -lt 3000) {
            if ($serialPort.BytesToRead -gt 0) {
                $data = $serialPort.ReadExisting()
                $response += $data
                Write-Host $data -NoNewline -ForegroundColor White
                
                # Check if we got complete response
                if ($response -match "(ok|>)" -and $response -match "\r?\n") {
                    break
                }
            }
            Start-Sleep -Milliseconds 50
        }
        
        if ($response -eq "") {
            Write-Host "No response received!" -ForegroundColor Red
        }
        
        Start-Sleep -Milliseconds 300
    }
    
    Write-Host "`n`n=== Position Tracking Test Complete ===" -ForegroundColor Yellow
    Write-Host "Check the position values in status reports:" -ForegroundColor Green
    Write-Host "- MPos shows real-world coordinates (mm)" -ForegroundColor Green
    Write-Host "- Steps shows raw step counts" -ForegroundColor Green
    Write-Host "- StepPos shows converted step-to-coordinate values" -ForegroundColor Green
    
} catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
} finally {
    if ($serialPort -and $serialPort.IsOpen) {
        $serialPort.Close()
        Write-Host "`nSerial connection closed." -ForegroundColor Green
    }
}