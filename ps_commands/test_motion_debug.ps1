# Test Motion System with Full Debug Output
# Shows all serial output including TMR9 ISR debug messages
# October 19, 2025 - Diagnose why only first block executes

param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200,
    [int]$DelayMs = 100  # Delay between commands
)

Write-Host "===================================================" -ForegroundColor Cyan
Write-Host "PIC32MZ CNC - Motion Debug Test" -ForegroundColor Cyan
Write-Host "Port: $Port @ $BaudRate baud" -ForegroundColor Cyan
Write-Host "===================================================" -ForegroundColor Cyan
Write-Host ""

# Open serial port
$port_obj = New-Object System.IO.Ports.SerialPort $Port, $BaudRate, None, 8, One
$port_obj.ReadTimeout = 1000
$port_obj.WriteTimeout = 1000

try {
    $port_obj.Open()
    Write-Host "✓ Serial port opened" -ForegroundColor Green
    Start-Sleep -Milliseconds 500
    
    # Clear any startup messages
    try {
        while ($port_obj.BytesToRead -gt 0) {
            $line = $port_obj.ReadLine()
            Write-Host "[STARTUP] $line" -ForegroundColor DarkGray
        }
    } catch { }
    
    Write-Host ""
    Write-Host "Sending initialization commands..." -ForegroundColor Yellow
    Write-Host ""
    
    # Initialize commands
    $init_commands = @(
        "G21",      # mm mode
        "G90",      # absolute mode
        "G17",      # XY plane
        "G94",      # units/min feedrate
        "M3S1000"   # spindle on
    )
    
    foreach ($cmd in $init_commands) {
        Write-Host "> $cmd" -ForegroundColor Cyan
        $port_obj.WriteLine($cmd)
        Start-Sleep -Milliseconds $DelayMs
        
        # Read response
        try {
            while ($port_obj.BytesToRead -gt 0) {
                $line = $port_obj.ReadLine()
                Write-Host "  $line" -ForegroundColor White
            }
        } catch { }
    }
    
    Write-Host ""
    Write-Host "===================================================" -ForegroundColor Cyan
    Write-Host "STARTING MOTION TEST - Watch for [TMR9-ISR] messages!" -ForegroundColor Yellow
    Write-Host "===================================================" -ForegroundColor Cyan
    Write-Host ""
    
    # Motion test commands
    $motion_commands = @(
        "G0Z5",       # Move Z to 5mm (should see [TMR9-ISR] and [GRBL-BLOCK])
        "G0Z0",       # Move Z back to 0 (should see SECOND [TMR9-ISR])
        "G1Y10F1000", # Move Y to 10mm (should see THIRD [TMR9-ISR])
        "G1X10",      # Move X to 10mm (should see FOURTH [TMR9-ISR])
        "G1Y0",       # Move Y to 0 (should see FIFTH [TMR9-ISR])
        "G1X0"        # Move X to 0 (should see SIXTH [TMR9-ISR])
    )
    
    foreach ($cmd in $motion_commands) {
        Write-Host ""
        Write-Host ">>> $cmd" -ForegroundColor Green
        $port_obj.WriteLine($cmd)
        
        # Wait for "ok" response before collecting motion output
        $ok_received = $false
        $timeout_ok = 2000  # 2 seconds to wait for "ok"
        $start_time_ok = Get-Date
        
        while (((Get-Date) - $start_time_ok).TotalMilliseconds -lt $timeout_ok -and !$ok_received) {
            try {
                if ($port_obj.BytesToRead -gt 0) {
                    $line = $port_obj.ReadLine()
                    Write-Host "  $line" -ForegroundColor White
                    if ($line -match "^ok") {
                        $ok_received = $true
                    }
                }
            } catch { }
            Start-Sleep -Milliseconds 10
        }
        
        if (!$ok_received) {
            Write-Host "  WARNING: No 'ok' received for $cmd!" -ForegroundColor Red
        }
        
        # Now wait for motion to complete and collect ALL output
        $timeout = 5000  # 5 seconds max per move
        $start_time = Get-Date
        
        while (((Get-Date) - $start_time).TotalMilliseconds -lt $timeout) {
            try {
                if ($port_obj.BytesToRead -gt 0) {
                    $line = $port_obj.ReadLine()
                    
                    # Highlight critical debug messages
                    if ($line -match "\[TMR9-ISR\]") {
                        Write-Host "  $line" -ForegroundColor Magenta
                    }
                    elseif ($line -match "\[GRBL-BLOCK\]") {
                        Write-Host "  $line" -ForegroundColor Yellow
                    }
                    elseif ($line -match "\[COORD\]") {
                        Write-Host "  $line" -ForegroundColor Cyan
                    }
                    elseif ($line -match "\[TMR9\]") {
                        Write-Host "  $line" -ForegroundColor Green
                    }
                    elseif ($line -match "<Idle\|") {
                        Write-Host "  $line" -ForegroundColor Green
                        break  # Motion complete
                    }
                    elseif ($line -match "<Run\|") {
                        Write-Host "  $line" -ForegroundColor Yellow
                    }
                    else {
                        Write-Host "  $line" -ForegroundColor White
                    }
                }
            } catch {
                # Timeout is OK, keep waiting
            }
            Start-Sleep -Milliseconds 50
        }
    }
    
    Write-Host ""
    Write-Host "===================================================" -ForegroundColor Cyan
    Write-Host "Waiting for final motion to complete..." -ForegroundColor Yellow
    Write-Host "===================================================" -ForegroundColor Cyan
    Write-Host ""
    
    # Wait for final idle state and collect remaining output
    $final_timeout = 10000  # 10 seconds
    $start_time = Get-Date
    $idle_count = 0
    
    while (((Get-Date) - $start_time).TotalMilliseconds -lt $final_timeout) {
        try {
            if ($port_obj.BytesToRead -gt 0) {
                $line = $port_obj.ReadLine()
                
                # Highlight critical messages
                if ($line -match "\[TMR9-ISR\]") {
                    Write-Host "$line" -ForegroundColor Magenta
                }
                elseif ($line -match "\[GRBL-BLOCK\]") {
                    Write-Host "$line" -ForegroundColor Yellow
                }
                elseif ($line -match "\[TMR9\]") {
                    Write-Host "$line" -ForegroundColor Green
                }
                elseif ($line -match "<Idle\|") {
                    Write-Host "$line" -ForegroundColor Green
                    $idle_count++
                    if ($idle_count -ge 3) {
                        break  # Confirmed idle
                    }
                }
                else {
                    Write-Host "$line" -ForegroundColor White
                }
            }
        } catch { }
        Start-Sleep -Milliseconds 100
    }
    
    Write-Host ""
    Write-Host "===================================================" -ForegroundColor Cyan
    Write-Host "Test Complete!" -ForegroundColor Green
    Write-Host "===================================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "EXPECTED OUTPUT:" -ForegroundColor Yellow
    Write-Host "  - 6 separate [GRBL-BLOCK] messages (one per move)" -ForegroundColor White
    Write-Host "  - 6 separate [TMR9] Started: messages" -ForegroundColor White
    Write-Host "  - Multiple [TMR9-ISR] Entry: messages showing busy transitions" -ForegroundColor White
    Write-Host "  - Final position: <Idle|MPos:0.000,0.000,0.000|...>" -ForegroundColor White
    Write-Host ""
    Write-Host "IF YOU ONLY SAW 1 [GRBL-BLOCK]:" -ForegroundColor Red
    Write-Host "  → TMR9 ISR is stuck after first block!" -ForegroundColor Red
    Write-Host "  → Check if MultiAxis_IsBusy() stays true forever" -ForegroundColor Red
    Write-Host "  → Check if block_discarded flag gets stuck at false" -ForegroundColor Red
    Write-Host ""
    
} catch {
    Write-Host "ERROR: $_" -ForegroundColor Red
} finally {
    if ($port_obj.IsOpen) {
        $port_obj.Close()
    }
    Write-Host "Serial port closed" -ForegroundColor Gray
}
