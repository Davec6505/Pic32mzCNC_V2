#!/usr/bin/env pwsh

# Simple status check to verify system state

Write-Host "===== System Status Check =====" -ForegroundColor Yellow
Write-Host ""

try {
    $port = New-Object System.IO.Ports.SerialPort
    $port.PortName = "COM4" 
    $port.BaudRate = 115200
    $port.Parity = "None"
    $port.DataBits = 8
    $port.StopBits = "One"
    $port.ReadTimeout = 2000
    $port.WriteTimeout = 1000

    $port.Open()
    Write-Host "Connected to COM4" -ForegroundColor Green

    # Clear buffer first
    Start-Sleep -Milliseconds 500
    if ($port.BytesToRead -gt 0) {
        $dummy = $port.ReadExisting()
        Write-Host "Cleared buffer: $dummy" -ForegroundColor Gray
    }

    # Send reset command first
    Write-Host ""
    Write-Host "Sending reset command..." -ForegroundColor Yellow
    $port.WriteLine("$$")  # Reset/clear command
    Start-Sleep -Milliseconds 1000
    
    if ($port.BytesToRead -gt 0) {
        $response = $port.ReadExisting()
        Write-Host "Reset response: $response" -ForegroundColor White
    }

    # Now get status 
    Write-Host ""
    Write-Host "Getting current status..." -ForegroundColor Yellow
    $port.WriteLine("?")
    Start-Sleep -Milliseconds 500
    
    if ($port.BytesToRead -gt 0) {
        $status = $port.ReadExisting()
        Write-Host "Status: $status" -ForegroundColor Cyan
    }

    # Send simple command to test motion planner
    Write-Host ""
    Write-Host "Testing with simple G0Z1 command..." -ForegroundColor Yellow
    $port.WriteLine("G0Z1")
    Start-Sleep -Milliseconds 2000  # Wait 2 seconds for any debug output
    
    $allResponse = ""
    if ($port.BytesToRead -gt 0) {
        $allResponse = $port.ReadExisting()
        Write-Host "G0Z1 Response:" -ForegroundColor White
        Write-Host $allResponse -ForegroundColor Gray
        
        # Check for key debug messages
        if ($allResponse -match "Motion planner got new block") {
            Write-Host "✅ Motion planner is active" -ForegroundColor Green
        } else {
            Write-Host "❌ Motion planner not responding" -ForegroundColor Red
        }
        
        if ($allResponse -match "MotionPlanner_ExecuteBlock") {
            Write-Host "✅ Motion execution called" -ForegroundColor Green
        } else {
            Write-Host "❌ Motion execution not called" -ForegroundColor Red
        }
    }

    # Final status check
    Write-Host ""
    Write-Host "Final status check..." -ForegroundColor Yellow
    $port.WriteLine("?")
    Start-Sleep -Milliseconds 500
    
    if ($port.BytesToRead -gt 0) {
        $finalStatus = $port.ReadExisting()
        Write-Host "Final Status: $finalStatus" -ForegroundColor Cyan
    }
}
catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
}
finally {
    if ($port.IsOpen) {
        $port.Close()
        Write-Host ""
        Write-Host "Serial port closed." -ForegroundColor Gray
    }
}

Write-Host ""
Write-Host "===== Status Check Complete =====" -ForegroundColor Yellow