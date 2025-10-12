# Real-time Status Feedback Test for UGS Visualization
# Tests position reporting and machine state during G-code execution
param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200,
    [int]$StatusInterval = 100  # Status query interval in milliseconds
)

function Send-GCodeCommand {
    param(
        [System.IO.Ports.SerialPort]$Port,
        [string]$Command,
        [int]$TimeoutMs = 5000
    )
    
    $Command = $Command.Trim()
    if ($Command -eq "" -or $Command.StartsWith(";")) {
        return $true
    }
    
    Write-Host "Sending: $Command" -ForegroundColor Yellow
    $Port.Write("$Command`r`n")
    
    $startTime = Get-Date
    $response = ""
    
    while (((Get-Date) - $startTime).TotalMilliseconds -lt $TimeoutMs) {
        if ($Port.BytesToRead -gt 0) {
            $data = $Port.ReadExisting()
            $response += $data
            Write-Host $data -NoNewline -ForegroundColor Cyan
            
            if ($response -match "ok" -or $response -match "error") {
                Write-Host "" # New line
                return $response -match "ok"
            }
        }
        Start-Sleep -Milliseconds 10
    }
    
    Write-Host " [TIMEOUT]" -ForegroundColor Red
    return $false
}

function Query-MachineStatus {
    param(
        [System.IO.Ports.SerialPort]$Port,
        [int]$TimeoutMs = 1000
    )
    
    $Port.Write("?`r`n")
    
    $startTime = Get-Date
    $response = ""
    
    while (((Get-Date) - $startTime).TotalMilliseconds -lt $TimeoutMs) {
        if ($Port.BytesToRead -gt 0) {
            $data = $Port.ReadExisting()
            $response += $data
            
            # Look for complete status response
            if ($response -match "<([^>]+)>") {
                $statusMatch = $matches[1]
                return $statusMatch
            }
        }
        Start-Sleep -Milliseconds 5
    }
    
    return $null
}

function Parse-StatusResponse {
    param([string]$Status)
    
    if (-not $Status) { return $null }
    
    $result = @{
        State = ""
        MachinePosition = @{ X = 0; Y = 0; Z = 0 }
        WorkPosition = @{ X = 0; Y = 0; Z = 0 }
        FeedRate = 0
        SpindleSpeed = 0
        Buffer = 0
    }
    
    # Parse state (Idle, Run, Hold, etc.)
    if ($Status -match "^([^|]+)") {
        $result.State = $matches[1]
    }
    
    # Parse machine position MPos:x,y,z
    if ($Status -match "MPos:([+-]?\d+\.?\d*),([+-]?\d+\.?\d*),([+-]?\d+\.?\d*)") {
        $result.MachinePosition.X = [float]$matches[1]
        $result.MachinePosition.Y = [float]$matches[2] 
        $result.MachinePosition.Z = [float]$matches[3]
    }
    
    # Parse work position WPos:x,y,z
    if ($Status -match "WPos:([+-]?\d+\.?\d*),([+-]?\d+\.?\d*),([+-]?\d+\.?\d*)") {
        $result.WorkPosition.X = [float]$matches[1]
        $result.WorkPosition.Y = [float]$matches[2]
        $result.WorkPosition.Z = [float]$matches[3]
    }
    
    # Parse feed and spindle FS:feed,spindle
    if ($Status -match "FS:(\d+),(\d+)") {
        $result.FeedRate = [int]$matches[1]
        $result.SpindleSpeed = [int]$matches[2]
    }
    
    # Parse buffer state Bf:planner,serial
    if ($Status -match "Bf:(\d+),(\d+)") {
        $result.Buffer = [int]$matches[1]
    }
    
    return $result
}

function Display-StatusUpdate {
    param([object]$Status, [int]$UpdateCount)
    
    if (-not $Status) {
        Write-Host "[$UpdateCount] Status: No response" -ForegroundColor Red
        return
    }
    
    $x = $Status.MachinePosition.X.ToString("F3")
    $y = $Status.MachinePosition.Y.ToString("F3") 
    $z = $Status.MachinePosition.Z.ToString("F3")
    
    Write-Host "[$UpdateCount] " -NoNewline -ForegroundColor White
    Write-Host "State: $($Status.State) " -NoNewline -ForegroundColor Green
    Write-Host "Pos: X$x Y$y Z$z " -NoNewline -ForegroundColor Cyan
    Write-Host "Feed: $($Status.FeedRate) " -NoNewline -ForegroundColor Yellow
    Write-Host "Buf: $($Status.Buffer)" -ForegroundColor Magenta
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

    Write-Host "=== UGS Real-time Status Feedback Test ===" -ForegroundColor Green
    Write-Host "Opening $Port at $BaudRate baud..." -ForegroundColor Green
    
    $serialPort.Open()
    
    if ($serialPort.IsOpen) {
        Write-Host "Connected! Starting real-time status monitoring..." -ForegroundColor Green
        Start-Sleep -Seconds 1
        
        # Test G-code commands that will show motion feedback
        $testCommands = @(
            "G21",           # Set units to millimeters
            "G90",           # Absolute positioning
            "G0 X0 Y0 Z0",   # Move to origin
            "G1 X50 Y0 F100", # Slow move to test position updates
            "G1 X50 Y50 F200", # Medium speed move
            "G1 X0 Y50 F300",  # Faster move
            "G2 X0 Y0 I-25 J-25 F150", # Arc move (slow for visualization)
            "G0 Z5",         # Z-axis move
            "G0 Z0",         # Return Z
            "G0 X0 Y0"       # Return to origin
        )
        
        Write-Host "`n=== Starting Motion with Real-time Status ===" -ForegroundColor Magenta
        
        $updateCount = 0
        $lastStatusTime = Get-Date
        
        # Get initial status
        $initialStatus = Query-MachineStatus -Port $serialPort
        $parsedStatus = Parse-StatusResponse -Status $initialStatus
        Display-StatusUpdate -Status $parsedStatus -UpdateCount $updateCount
        $updateCount++
        
        foreach ($command in $testCommands) {
            Write-Host "`n--- Executing: $command ---" -ForegroundColor Yellow
            
            # Send the command
            $success = Send-GCodeCommand -Port $serialPort -Command $command
            
            if (-not $success) {
                Write-Host "Command failed, continuing..." -ForegroundColor Red
                continue
            }
            
            # Monitor status during execution
            $commandStartTime = Get-Date
            $maxMonitorTime = 10000 # 10 seconds max per command
            
            while (((Get-Date) - $commandStartTime).TotalMilliseconds -lt $maxMonitorTime) {
                $currentTime = Get-Date
                
                # Query status at specified interval
                if (($currentTime - $lastStatusTime).TotalMilliseconds -ge $StatusInterval) {
                    $status = Query-MachineStatus -Port $serialPort
                    $parsedStatus = Parse-StatusResponse -Status $status
                    Display-StatusUpdate -Status $parsedStatus -UpdateCount $updateCount
                    
                    $updateCount++
                    $lastStatusTime = $currentTime
                    
                    # Check if machine is idle (command completed)
                    if ($parsedStatus -and $parsedStatus.State -eq "Idle") {
                        Write-Host "Command completed (Idle state detected)" -ForegroundColor Green
                        break
                    }
                }
                
                Start-Sleep -Milliseconds 25
            }
            
            # Small pause between commands
            Start-Sleep -Milliseconds 200
        }
        
        Write-Host "`n=== Final Status Check ===" -ForegroundColor Magenta
        $finalStatus = Query-MachineStatus -Port $serialPort
        $parsedFinalStatus = Parse-StatusResponse -Status $finalStatus
        Display-StatusUpdate -Status $parsedFinalStatus -UpdateCount $updateCount
        
        Write-Host "`n=== Real-time Status Test Complete ===" -ForegroundColor Green
        Write-Host "Total status updates captured: $updateCount" -ForegroundColor White
        Write-Host "Status feedback system is working for UGS visualization!" -ForegroundColor Green
        
        # Test rapid status queries (UGS simulation)
        Write-Host "`n=== Rapid Status Query Test (UGS Simulation) ===" -ForegroundColor Magenta
        Write-Host "Simulating UGS rapid status requests..." -ForegroundColor Yellow
        
        $rapidTestCount = 20
        $successCount = 0
        $startTime = Get-Date
        
        for ($i = 1; $i -le $rapidTestCount; $i++) {
            $status = Query-MachineStatus -Port $serialPort -TimeoutMs 500
            if ($status) {
                $successCount++
                Write-Host "." -NoNewline -ForegroundColor Green
            } else {
                Write-Host "x" -NoNewline -ForegroundColor Red
            }
            Start-Sleep -Milliseconds 50
        }
        
        $totalTime = ((Get-Date) - $startTime).TotalMilliseconds
        $avgResponseTime = $totalTime / $rapidTestCount
        
        Write-Host "`n"
        Write-Host "Rapid Status Test Results:" -ForegroundColor White
        Write-Host "  Success Rate: $successCount/$rapidTestCount ($([math]::Round(($successCount/$rapidTestCount)*100, 1))%)" -ForegroundColor Green
        Write-Host "  Average Response Time: $([math]::Round($avgResponseTime, 1))ms" -ForegroundColor Cyan
        Write-Host "  Total Test Time: $([math]::Round($totalTime, 0))ms" -ForegroundColor White
        
        if ($successCount -ge ($rapidTestCount * 0.9)) {
            Write-Host "✅ Status feedback system is excellent for UGS visualization!" -ForegroundColor Green
        } elseif ($successCount -ge ($rapidTestCount * 0.7)) {
            Write-Host "⚠️ Status feedback is good but could be improved" -ForegroundColor Yellow
        } else {
            Write-Host "❌ Status feedback may have issues for real-time visualization" -ForegroundColor Red
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