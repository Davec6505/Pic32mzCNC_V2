#!/usr/bin/env pwsh
# test_segment_debug.ps1 - Capture segment debug output without UGS interference
# October 19, 2025

param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200
)

Write-Host "=== Segment Debug Test ===" -ForegroundColor Cyan
Write-Host "Port: $Port @ $BaudRate baud" -ForegroundColor Yellow
Write-Host "Commands: G0 Z5 (expect 3 segments: 2mm + 2mm + 1mm)" -ForegroundColor Yellow
Write-Host ""

# Open serial port
$serial = New-Object System.IO.Ports.SerialPort
$serial.PortName = $Port
$serial.BaudRate = $BaudRate
$serial.DataBits = 8
$serial.Parity = [System.IO.Ports.Parity]::None
$serial.StopBits = [System.IO.Ports.StopBits]::One
$serial.Handshake = [System.IO.Ports.Handshake]::None
$serial.ReadTimeout = 1000
$serial.WriteTimeout = 1000

try {
    $serial.Open()
    Write-Host "[CONNECTED]" -ForegroundColor Green
    Start-Sleep -Milliseconds 500
    
    # Flush any startup messages
    while ($serial.BytesToRead -gt 0) {
        $null = $serial.ReadExisting()
    }
    
    # Send wake-up character
    Write-Host "`n[SEND] <wake-up>" -ForegroundColor Cyan
    $serial.WriteLine("")
    Start-Sleep -Milliseconds 200
    
    # Read response
    $response = ""
    $timeout = [DateTime]::Now.AddSeconds(2)
    while ([DateTime]::Now -lt $timeout -and $serial.BytesToRead -gt 0) {
        $response += $serial.ReadExisting()
        Start-Sleep -Milliseconds 50
    }
    if ($response) {
        Write-Host "[RECV] $response" -ForegroundColor Gray
    }
    
    # Send G0 Z5 command
    Write-Host "`n[SEND] G0 Z5" -ForegroundColor Cyan
    $serial.WriteLine("G0 Z5")
    
    # Capture ALL output for 5 seconds (motion + debug)
    Write-Host "`n=== DEBUG OUTPUT (5 seconds) ===" -ForegroundColor Yellow
    $startTime = [DateTime]::Now
    $segmentCount = 0
    $blockDiscardCount = 0
    $prepFinalCount = 0
    
    while (([DateTime]::Now - $startTime).TotalSeconds -lt 5) {
        if ($serial.BytesToRead -gt 0) {
            $line = $serial.ReadLine()
            
            # Count debug events
            if ($line -match '\[SEG_COMPLETE\]') {
                $segmentCount++
                Write-Host $line -ForegroundColor Green
            }
            elseif ($line -match '\[BLOCK_DISCARD\]') {
                $blockDiscardCount++
                Write-Host $line -ForegroundColor Magenta
            }
            elseif ($line -match '\[PREP_FINAL_SEG\]') {
                $prepFinalCount++
                Write-Host $line -ForegroundColor Yellow
            }
            elseif ($line -match '<Run\|') {
                # Position updates (only show every 5th to reduce spam)
                if ($segmentCount % 5 -eq 0) {
                    Write-Host $line -ForegroundColor DarkGray
                }
            }
            else {
                Write-Host $line -ForegroundColor Gray
            }
        }
        Start-Sleep -Milliseconds 50
    }
    
    # Summary
    Write-Host "`n=== SUMMARY ===" -ForegroundColor Cyan
    Write-Host "Segments executed: $segmentCount" -ForegroundColor $(if ($segmentCount -eq 3) { "Green" } else { "Red" })
    Write-Host "Blocks discarded: $blockDiscardCount" -ForegroundColor $(if ($blockDiscardCount -eq 1) { "Green" } else { "Red" })
    Write-Host "Final segments prepped: $prepFinalCount" -ForegroundColor $(if ($prepFinalCount -eq 1) { "Green" } else { "Red" })
    
    Write-Host "`n=== EXPECTED vs ACTUAL ===" -ForegroundColor Yellow
    Write-Host "Expected: 3 segments, 1 block discard, 1 final segment prep" -ForegroundColor White
    Write-Host "Actual:   $segmentCount segments, $blockDiscardCount block discards, $prepFinalCount final segment preps" -ForegroundColor White
    
    if ($segmentCount -gt 3) {
        Write-Host "`n⚠️  ROCKET SHIP BUG CONFIRMED: Too many segments executed!" -ForegroundColor Red
        Write-Host "Theory: Blocks are being re-added to planner or segments repeating" -ForegroundColor Yellow
    }
    elseif ($segmentCount -eq 3) {
        Write-Host "`n✅ MOTION CORRECT: Exactly 3 segments executed as expected!" -ForegroundColor Green
    }
    else {
        Write-Host "`n⚠️  TOO FEW SEGMENTS: Motion may have stopped early" -ForegroundColor Yellow
    }
    
    # Send soft reset to stop motion
    Write-Host "`n[SEND] Ctrl-X (soft reset)" -ForegroundColor Cyan
    $serial.Write([char]0x18)
    Start-Sleep -Milliseconds 500
    
    # Flush remaining output
    if ($serial.BytesToRead -gt 0) {
        $remaining = $serial.ReadExisting()
        Write-Host "[REMAINING OUTPUT]" -ForegroundColor DarkGray
        Write-Host $remaining -ForegroundColor DarkGray
    }
}
catch {
    Write-Host "ERROR: $_" -ForegroundColor Red
}
finally {
    if ($serial.IsOpen) {
        $serial.Close()
        Write-Host "`n[DISCONNECTED]" -ForegroundColor Yellow
    }
}

Write-Host "`n=== Test Complete ===" -ForegroundColor Cyan
