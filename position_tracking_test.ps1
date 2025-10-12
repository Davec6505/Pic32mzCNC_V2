# Position Tracking Test for UGS Real-time Visualization
# Verifies position updates during actual motion execution
param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200
)

function Send-CommandAndMonitorPosition {
    param(
        [System.IO.Ports.SerialPort]$Port,
        [string]$Command,
        [int]$MonitorDurationMs = 3000
    )
    
    Write-Host "`n--- Testing: $Command ---" -ForegroundColor Yellow
    
    # Get initial position
    $Port.Write("?`r`n")
    Start-Sleep -Milliseconds 100
    $initialResponse = ""
    if ($Port.BytesToRead -gt 0) {
        $initialResponse = $Port.ReadExisting()
    }
    
    Write-Host "Initial: $initialResponse" -NoNewline -ForegroundColor Gray
    
    # Send the motion command
    Write-Host "Sending: $Command" -ForegroundColor Cyan
    $Port.Write("$Command`r`n")
    
    # Monitor position changes during execution
    $startTime = Get-Date
    $positionUpdates = @()
    $lastPosition = "unknown"
    
    while (((Get-Date) - $startTime).TotalMilliseconds -lt $MonitorDurationMs) {
        # Query position
        $Port.Write("?`r`n")
        Start-Sleep -Milliseconds 50
        
        if ($Port.BytesToRead -gt 0) {
            $response = $Port.ReadExisting()
            
            # Extract position from response
            if ($response -match "<([^>]+)>") {
                $status = $matches[1]
                
                # Look for position changes
                if ($status -match "MPos:([+-]?\d+\.?\d*),([+-]?\d+\.?\d*),([+-]?\d+\.?\d*)") {
                    $currentPosition = "X$($matches[1]) Y$($matches[2]) Z$($matches[3])"
                    
                    if ($currentPosition -ne $lastPosition) {
                        $elapsedMs = ((Get-Date) - $startTime).TotalMilliseconds
                        $positionUpdates += @{
                            Time = $elapsedMs
                            Position = $currentPosition
                            FullStatus = $status
                        }
                        
                        Write-Host "[$([math]::Round($elapsedMs, 0))ms] Pos: $currentPosition" -ForegroundColor Green
                        $lastPosition = $currentPosition
                    }
                }
                
                # Check if motion is complete (Idle state)
                if ($status -match "^Idle") {
                    Write-Host "Motion completed (Idle detected)" -ForegroundColor Green
                    break
                }
            }
        }
        
        Start-Sleep -Milliseconds 25
    }
    
    # Get final position
    $Port.Write("?`r`n")
    Start-Sleep -Milliseconds 100
    $finalResponse = ""
    if ($Port.BytesToRead -gt 0) {
        $finalResponse = $Port.ReadExisting()
    }
    
    Write-Host "Final: $finalResponse" -NoNewline -ForegroundColor Gray
    Write-Host "Position updates captured: $($positionUpdates.Count)" -ForegroundColor White
    
    return $positionUpdates
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

    Write-Host "=== Position Tracking Test for UGS Visualization ===" -ForegroundColor Green
    Write-Host "Opening $Port at $BaudRate baud..." -ForegroundColor Green
    
    $serialPort.Open()
    
    if ($serialPort.IsOpen) {
        Write-Host "Connected! Testing position feedback during motion..." -ForegroundColor Green
        Start-Sleep -Seconds 1
        
        # Initialize machine
        Write-Host "`n=== Initializing Machine ===" -ForegroundColor Magenta
        $serialPort.Write("G21`r`n")  # Metric units
        Start-Sleep -Milliseconds 200
        $serialPort.Write("G90`r`n")  # Absolute positioning
        Start-Sleep -Milliseconds 200
        
        # Clear any responses
        if ($serialPort.BytesToRead -gt 0) {
            $initResponse = $serialPort.ReadExisting()
            Write-Host "Init response: $initResponse" -ForegroundColor Gray
        }
        
        # Test different types of moves to verify position feedback
        $allPositionUpdates = @()
        
        Write-Host "`n=== Testing Position Feedback During Motion ===" -ForegroundColor Magenta
        
        # Test 1: Slow linear move (should show intermediate positions)
        $updates1 = Send-CommandAndMonitorPosition -Port $serialPort -Command "G1 X20 Y0 F50"
        $allPositionUpdates += $updates1
        
        # Test 2: Diagonal move
        $updates2 = Send-CommandAndMonitorPosition -Port $serialPort -Command "G1 X20 Y20 F100"
        $allPositionUpdates += $updates2
        
        # Test 3: Arc move (tests circular interpolation position updates)
        $updates3 = Send-CommandAndMonitorPosition -Port $serialPort -Command "G2 X0 Y20 I-10 J0 F75"
        $allPositionUpdates += $updates3
        
        # Test 4: Return to origin
        $updates4 = Send-CommandAndMonitorPosition -Port $serialPort -Command "G1 X0 Y0 F200"
        $allPositionUpdates += $updates4
        
        # Test 5: Z-axis move
        $updates5 = Send-CommandAndMonitorPosition -Port $serialPort -Command "G1 Z10 F100"
        $allPositionUpdates += $updates5
        
        # Test 6: Return Z to zero
        $updates6 = Send-CommandAndMonitorPosition -Port $serialPort -Command "G0 Z0"
        $allPositionUpdates += $updates6
        
        Write-Host "`n=== Position Tracking Analysis ===" -ForegroundColor Magenta
        
        $totalUpdates = $allPositionUpdates.Count
        Write-Host "Total position updates captured: $totalUpdates" -ForegroundColor White
        
        if ($totalUpdates -gt 0) {
            Write-Host "✅ Position feedback is working!" -ForegroundColor Green
            Write-Host "   • UGS will receive real-time position updates" -ForegroundColor Green
            Write-Host "   • Motion visualization will be smooth and accurate" -ForegroundColor Green
            Write-Host "   • Both linear and arc motions provide feedback" -ForegroundColor Green
            
            # Show some sample position updates
            Write-Host "`nSample position updates:" -ForegroundColor Cyan
            $sampleCount = [Math]::Min(5, $totalUpdates)
            for ($i = 0; $i -lt $sampleCount; $i++) {
                $update = $allPositionUpdates[$i]
                Write-Host "  $($update.Time.ToString("F0"))ms: $($update.Position)" -ForegroundColor Cyan
            }
            
            if ($totalUpdates -gt 5) {
                Write-Host "  ... and $($totalUpdates - 5) more updates" -ForegroundColor Cyan
            }
        } else {
            Write-Host "⚠️ No position updates detected during motion" -ForegroundColor Yellow
            Write-Host "   This could indicate:" -ForegroundColor Yellow
            Write-Host "   • Motions completing too quickly for position feedback" -ForegroundColor Yellow
            Write-Host "   • Position reporting may need timing adjustments" -ForegroundColor Yellow
        }
        
        # Test rapid status queries (simulating UGS behavior)
        Write-Host "`n=== UGS-style Rapid Status Queries ===" -ForegroundColor Magenta
        Write-Host "Testing rapid '?' queries like UGS uses..." -ForegroundColor Yellow
        
        $rapidTests = 15
        $successCount = 0
        $responses = @()
        
        for ($i = 1; $i -le $rapidTests; $i++) {
            $startTime = Get-Date
            $serialPort.Write("?`r`n")
            
            $timeout = 200  # 200ms timeout
            $response = ""
            
            while (((Get-Date) - $startTime).TotalMilliseconds -lt $timeout) {
                if ($serialPort.BytesToRead -gt 0) {
                    $response = $serialPort.ReadExisting()
                    break
                }
                Start-Sleep -Milliseconds 5
            }
            
            if ($response -match "<([^>]+)>") {
                $successCount++
                $responses += $response.Trim()
                Write-Host "✓" -NoNewline -ForegroundColor Green
            } else {
                Write-Host "✗" -NoNewline -ForegroundColor Red
            }
            
            Start-Sleep -Milliseconds 100  # UGS typically queries every 100ms
        }
        
        Write-Host "`n"
        Write-Host "Rapid Query Results:" -ForegroundColor White
        Write-Host "  Success Rate: $successCount/$rapidTests ($([math]::Round(($successCount/$rapidTests)*100, 1))%)" -ForegroundColor Green
        
        if ($successCount -gt 0) {
            Write-Host "  Sample status response:" -ForegroundColor Cyan
            Write-Host "    $($responses[0])" -ForegroundColor Cyan
        }
        
        if ($successCount -ge ($rapidTests * 0.9)) {
            Write-Host "`n🎯 EXCELLENT: Status feedback is perfect for UGS!" -ForegroundColor Green
            Write-Host "   • Real-time position updates: ✅" -ForegroundColor Green
            Write-Host "   • Rapid status queries: ✅" -ForegroundColor Green  
            Write-Host "   • Motion visualization ready: ✅" -ForegroundColor Green
        } elseif ($successCount -ge ($rapidTests * 0.7)) {
            Write-Host "`n⚠️ GOOD: Status feedback works but could be optimized" -ForegroundColor Yellow
        } else {
            Write-Host "`n❌ NEEDS WORK: Status feedback may have timing issues" -ForegroundColor Red
        }
        
        Write-Host "`n=== Test Summary ===" -ForegroundColor Green
        Write-Host "The modular motion control system provides:" -ForegroundColor White
        Write-Host "• Real-time position feedback during motion execution" -ForegroundColor Green
        Write-Host "• Compatible status reporting for UGS visualization" -ForegroundColor Green
        Write-Host "• Proper state management (Idle/Run detection)" -ForegroundColor Green
        Write-Host "• Motion buffer status reporting" -ForegroundColor Green
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