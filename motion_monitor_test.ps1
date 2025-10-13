#!/usr/bin/env pwsh

# Test motion execution with extended monitoring
# This will monitor the complete 7-second motion execution

Write-Host "===== Motion Execution Monitor =====" -ForegroundColor Yellow
Write-Host ""

Write-Host "Opening COM4 for extended motion monitoring..." -ForegroundColor Cyan

try {
    $port = New-Object System.IO.Ports.SerialPort
    $port.PortName = "COM4"
    $port.BaudRate = 115200
    $port.Parity = "None"
    $port.DataBits = 8
    $port.StopBits = "One"
    $port.ReadTimeout = 1000
    $port.WriteTimeout = 1000

    $port.Open()
    Write-Host "Connected to COM4 successfully!" -ForegroundColor Green
    
    # Clear any buffered data
    Start-Sleep -Milliseconds 500
    if ($port.BytesToRead -gt 0) {
        $dummy = $port.ReadExisting()
    }

    Write-Host ""
    Write-Host "Sending G0Z5 command..." -ForegroundColor Yellow
    $port.WriteLine("G0Z5")
    
    Write-Host "Monitoring motion execution (up to 10 seconds)..." -ForegroundColor Cyan
    Write-Host "Expected sequence:" -ForegroundColor White
    Write-Host "1. Motion planner gets block" -ForegroundColor Gray
    Write-Host "2. MotionPlanner_ExecuteBlock called" -ForegroundColor Gray
    Write-Host "3. Hardware motion started" -ForegroundColor Gray
    Write-Host "4. Motion timing updates (7000ms duration)" -ForegroundColor Gray
    Write-Host "5. Motion completion and buffer advance" -ForegroundColor Gray
    Write-Host ""

    $startTime = Get-Date
    $responseBuffer = ""
    
    # Monitor for 10 seconds to catch the complete 7-second motion
    while ((Get-Date).Subtract($startTime).TotalSeconds -lt 10) {
        try {
            if ($port.BytesToRead -gt 0) {
                $data = $port.ReadExisting()
                $responseBuffer += $data
                
                # Print data as it comes in with timestamps
                $lines = $data -split "`r`n"
                foreach ($line in $lines) {
                    if ($line.Trim() -ne "") {
                        $timestamp = (Get-Date).ToString("HH:mm:ss.fff")
                        Write-Host "[$timestamp] $line" -ForegroundColor White
                        
                        # Highlight key debug messages
                        if ($line -match "Motion planner got new block") {
                            Write-Host "  ✅ Block received by planner" -ForegroundColor Green
                        }
                        elseif ($line -match "MotionPlanner_ExecuteBlock called") {
                            Write-Host "  ✅ Hardware execution started" -ForegroundColor Green
                        }
                        elseif ($line -match "APP_ExecuteMotionBlock SUCCESS") {
                            Write-Host "  ✅ OCR modules enabled" -ForegroundColor Green
                        }
                        elseif ($line -match "Motion timing.*timer=(\d+).*duration_ms=([0-9.]+)") {
                            $timer = $matches[1]
                            $duration = $matches[2]
                            $progress = ([int]$timer / [float]$duration * 100)
                            Write-Host "  Motion progress: $timer/$duration ms ($progress%)" -ForegroundColor Cyan
                        }
                        elseif ($line -match "Motion block completed") {
                            Write-Host "  Motion execution completed" -ForegroundColor Green
                        }
                        elseif ($line -match "MotionBuffer_Complete.*advancing tail") {
                            Write-Host "  Buffer tail advanced" -ForegroundColor Green
                        }
                        elseif ($line -match "BUFFER_ADD.*head=(\d+).*tail=(\d+)") {
                            $head = $matches[1]
                            $tail = $matches[2]
                            Write-Host "  Buffer state: head=$head, tail=$tail" -ForegroundColor Yellow
                        }
                    }
                }
            }
            Start-Sleep -Milliseconds 50
        }
        catch {
            # Continue on timeout
        }
    }

    Write-Host ""
    Write-Host "Sending status request to check final position..." -ForegroundColor Yellow
    $port.WriteLine("?")
    Start-Sleep -Milliseconds 500
    
    if ($port.BytesToRead -gt 0) {
        $status = $port.ReadExisting()
        Write-Host "Final Status: $status" -ForegroundColor White
        
        if ($status -match "MPos:([0-9.,]+)") {
            $position = $matches[1]
            Write-Host "Position: $position" -ForegroundColor Cyan
            
            if ($position -match "0.000,0.000,5.000") {
                Write-Host "SUCCESS: Position changed to Z=5!" -ForegroundColor Green
            }
            elseif ($position -match "0.000,0.000,0.000") {
                Write-Host "ISSUE: Position still at origin" -ForegroundColor Red
            }
            else {
                Write-Host "Position changed but not as expected: $position" -ForegroundColor Yellow
            }
        }
    }
}
catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
}
finally {
    if ($port.IsOpen) {
        $port.Close()
        Write-Host "Serial port closed." -ForegroundColor Gray
    }
}

Write-Host ""
Write-Host "===== Motion Monitor Complete =====" -ForegroundColor Yellow