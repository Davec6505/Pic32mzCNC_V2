# Test script to verify OCR period scaling fix for motion accuracy
# Tests the diagonal motion bug: firmware should execute moves with exact step counts
#
# BEFORE FIX: Hardware overshot (13.038mm instead of 10mm = 130.4%)
# AFTER FIX: Should achieve exactly 10.000mm (800 steps @ 80 steps/mm)

param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200
)

function Send-GCodeCommand {
    param(
        [System.IO.Ports.SerialPort]$Port,
        [string]$Command,
        [int]$TimeoutMs = 5000
    )
    
    Write-Host ">>> $Command" -ForegroundColor Cyan
    $Port.WriteLine($Command)
    
    $startTime = Get-Date
    while (((Get-Date) - $startTime).TotalMilliseconds -lt $TimeoutMs) {
        try {
            $response = $Port.ReadLine()
            if ($response -match "ok") {
                Write-Host "<<< ok" -ForegroundColor Green
                return $true
            } elseif ($response -match "error") {
                Write-Host "<<< $response" -ForegroundColor Red
                return $false
            } else {
                Write-Host "<<< $response" -ForegroundColor Gray
            }
        } catch {
            Start-Sleep -Milliseconds 10
        }
    }
    
    Write-Host "!!! TIMEOUT waiting for 'ok'" -ForegroundColor Red
    return $false
}

function Get-Position {
    param([System.IO.Ports.SerialPort]$Port)
    
    # Send RAW '?' (no line terminators)
    $Port.Write("?")
    Start-Sleep -Milliseconds 50
    
    try {
        $response = $Port.ReadLine()
        if ($response -match "MPos:([-\d.]+),([-\d.]+),([-\d.]+)") {
            return @{
                X = [float]$matches[1]
                Y = [float]$matches[2]
                Z = [float]$matches[3]
                Raw = $response
            }
        }
    } catch {
        Write-Host "Error reading position: $_" -ForegroundColor Red
    }
    return $null
}

function Wait-ForMotionComplete {
    param(
        [System.IO.Ports.SerialPort]$Port,
        [int]$TimeoutSeconds = 30
    )
    
    $startTime = Get-Date
    while (((Get-Date) - $startTime).TotalSeconds -lt $TimeoutSeconds) {
        $Port.Write("?")
        Start-Sleep -Milliseconds 50
        
        try {
            $response = $Port.ReadLine()
            if ($response -match "<Idle") {
                return $true
            }
            Write-Host "  Status: $response" -ForegroundColor DarkGray
        } catch {
            # Timeout on ReadLine, keep waiting
        }
        
        Start-Sleep -Milliseconds 100
    }
    
    Write-Host "!!! TIMEOUT waiting for motion to complete" -ForegroundColor Red
    return $false
}

# Open serial port
$serialPort = New-Object System.IO.Ports.SerialPort
$serialPort.PortName = $Port
$serialPort.BaudRate = $BaudRate
$serialPort.DataBits = 8
$serialPort.Parity = [System.IO.Ports.Parity]::None
$serialPort.StopBits = [System.IO.Ports.StopBits]::One
$serialPort.ReadTimeout = 1000
$serialPort.WriteTimeout = 1000

try {
    $serialPort.Open()
    Write-Host "`n============================================================" -ForegroundColor Cyan
    Write-Host "  OCR PERIOD SCALING - MOTION ACCURACY TEST" -ForegroundColor Cyan
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host "Connected to $Port @ $BaudRate" -ForegroundColor Green
    
    # Wait for startup message
    Start-Sleep -Milliseconds 1000
    $serialPort.ReadExisting() | Out-Null
    
    Write-Host "`nTest Configuration:" -ForegroundColor Yellow
    Write-Host "  X/Y Steps/mm: 80 (GT2 belt, 1/16 microstepping)" -ForegroundColor Gray
    Write-Host "  Target: 10.000mm = 800 steps exactly" -ForegroundColor Gray
    Write-Host "  Expected: Position reports show 10.000mm (not 13.038mm!)" -ForegroundColor Gray
    
    # Initialize - set to metric and absolute mode
    Write-Host "`n--- Initialization ---" -ForegroundColor Yellow
    Send-GCodeCommand -Port $serialPort -Command "G21" | Out-Null  # Metric
    Send-GCodeCommand -Port $serialPort -Command "G90" | Out-Null  # Absolute
    Send-GCodeCommand -Port $serialPort -Command "G92 X0 Y0 Z0" | Out-Null  # Set current as origin
    
    Start-Sleep -Milliseconds 500
    
    # Get initial position
    Write-Host "`n--- Initial Position ---" -ForegroundColor Yellow
    $pos = Get-Position -Port $serialPort
    if ($pos) {
        Write-Host "Position: X=$($pos.X) Y=$($pos.Y) Z=$($pos.Z)" -ForegroundColor White
    }
    
    # Test 1: Y-axis 10mm move
    Write-Host "`n--- TEST 1: Y-axis 10mm @ 1000mm/min ---" -ForegroundColor Yellow
    Write-Host "Expected: 800 steps → 10.000mm EXACTLY" -ForegroundColor Gray
    
    Send-GCodeCommand -Port $serialPort -Command "G1 Y10 F1000" | Out-Null
    Write-Host "Waiting for motion to complete..." -ForegroundColor Gray
    
    if (Wait-ForMotionComplete -Port $serialPort -TimeoutSeconds 30) {
        $pos = Get-Position -Port $serialPort
        if ($pos) {
            Write-Host "`nRESULT:" -ForegroundColor Cyan
            Write-Host "  Y Position: $($pos.Y) mm" -ForegroundColor White
            
            $error = [Math]::Abs($pos.Y - 10.0)
            if ($error -lt 0.01) {
                Write-Host "  ✓ PASS: Position accurate within 0.01mm!" -ForegroundColor Green
            } elseif ($error -lt 0.1) {
                Write-Host "  ⚠ MARGINAL: Position within 0.1mm (error: $error mm)" -ForegroundColor Yellow
            } else {
                Write-Host "  ✗ FAIL: Position error: $error mm" -ForegroundColor Red
                if ($pos.Y -gt 12) {
                    Write-Host "  (Still overshooting - OCR scaling not working!)" -ForegroundColor Red
                }
            }
        }
    }
    
    # Test 2: X-axis 10mm move
    Write-Host "`n--- TEST 2: X-axis 10mm @ 1000mm/min ---" -ForegroundColor Yellow
    Write-Host "Expected: 800 steps → 10.000mm EXACTLY" -ForegroundColor Gray
    
    Send-GCodeCommand -Port $serialPort -Command "G1 X10 F1000" | Out-Null
    Write-Host "Waiting for motion to complete..." -ForegroundColor Gray
    
    if (Wait-ForMotionComplete -Port $serialPort -TimeoutSeconds 30) {
        $pos = Get-Position -Port $serialPort
        if ($pos) {
            Write-Host "`nRESULT:" -ForegroundColor Cyan
            Write-Host "  X Position: $($pos.X) mm" -ForegroundColor White
            Write-Host "  Y Position: $($pos.Y) mm (should still be 10.000)" -ForegroundColor White
            
            $errorX = [Math]::Abs($pos.X - 10.0)
            $errorY = [Math]::Abs($pos.Y - 10.0)
            
            if ($errorX -lt 0.01 -and $errorY -lt 0.01) {
                Write-Host "  ✓ PASS: Both axes accurate!" -ForegroundColor Green
            } else {
                Write-Host "  ✗ Position errors: X=$errorX mm, Y=$errorY mm" -ForegroundColor Red
            }
        }
    }
    
    # Test 3: Return to Y=0
    Write-Host "`n--- TEST 3: Return Y to origin (Y=0) ---" -ForegroundColor Yellow
    
    Send-GCodeCommand -Port $serialPort -Command "G1 Y0 F1000" | Out-Null
    Write-Host "Waiting for motion to complete..." -ForegroundColor Gray
    
    if (Wait-ForMotionComplete -Port $serialPort -TimeoutSeconds 30) {
        $pos = Get-Position -Port $serialPort
        if ($pos) {
            Write-Host "`nRESULT:" -ForegroundColor Cyan
            Write-Host "  Position: X=$($pos.X) Y=$($pos.Y)" -ForegroundColor White
            
            $errorY = [Math]::Abs($pos.Y)
            if ($errorY -lt 0.01) {
                Write-Host "  ✓ PASS: Y returned to origin!" -ForegroundColor Green
            } else {
                Write-Host "  ✗ FAIL: Y position error: $errorY mm" -ForegroundColor Red
            }
        }
    }
    
    # Test 4: Return X to origin - complete the square
    Write-Host "`n--- TEST 4: Return X to origin (X=0) - Complete Square ---" -ForegroundColor Yellow
    
    Send-GCodeCommand -Port $serialPort -Command "G1 X0 F1000" | Out-Null
    Write-Host "Waiting for motion to complete..." -ForegroundColor Gray
    
    if (Wait-ForMotionComplete -Port $serialPort -TimeoutSeconds 30) {
        $pos = Get-Position -Port $serialPort
        if ($pos) {
            Write-Host "`nFINAL RESULT:" -ForegroundColor Cyan
            Write-Host "  Position: X=$($pos.X) Y=$($pos.Y) Z=$($pos.Z)" -ForegroundColor White
            
            $errorX = [Math]::Abs($pos.X)
            $errorY = [Math]::Abs($pos.Y)
            $errorZ = [Math]::Abs($pos.Z)
            $totalError = [Math]::Sqrt($errorX*$errorX + $errorY*$errorY + $errorZ*$errorZ)
            
            Write-Host "`n============================================================" -ForegroundColor Cyan
            if ($totalError -lt 0.01) {
                Write-Host "  ✓✓✓ PERFECT! Returned to origin within 0.01mm!" -ForegroundColor Green
                Write-Host "  OCR PERIOD SCALING FIX VERIFIED!" -ForegroundColor Green
            } elseif ($totalError -lt 0.1) {
                Write-Host "  ✓ GOOD: Returned to origin within 0.1mm" -ForegroundColor Yellow
                Write-Host "  Total position error: $totalError mm" -ForegroundColor Yellow
            } else {
                Write-Host "  ✗ FAIL: Did not return to origin" -ForegroundColor Red
                Write-Host "  Total position error: $totalError mm" -ForegroundColor Red
            }
            Write-Host "============================================================" -ForegroundColor Cyan
        }
    }
    
} catch {
    Write-Host "`nERROR: $_" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
} finally {
    if ($serialPort.IsOpen) {
        $serialPort.Close()
    }
    Write-Host "`nDisconnected from $Port" -ForegroundColor Gray
}
