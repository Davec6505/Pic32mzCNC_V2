###############################################################################
# test_coordinates.ps1
# Test G92 and G91 coordinate system functionality
#
# This script tests:
# 1. G92 coordinate offset (set work zero)
# 2. G91 relative positioning mode
# 3. Position tracking across moves
#
# Usage: .\test_coordinates.ps1 -Port COM4
###############################################################################

param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200
)

# Helper function to send G-code and capture response
function Send-GCode {
    param(
        [System.IO.Ports.SerialPort]$Port,
        [string]$Command,
        [int]$TimeoutMs = 3000
    )
    
    Write-Host ">> $Command" -ForegroundColor Cyan
    
    # Real-time commands (?, !, ~, ^X) must be sent WITHOUT line terminators
    if ($Command -eq "?" -or $Command -eq "!" -or $Command -eq "~") {
        $Port.Write($Command)
        
        # For status query, wait for response
        $startTime = Get-Date
        while (((Get-Date) - $startTime).TotalMilliseconds -lt 500) {
            try {
                $line = $Port.ReadLine()
                if ($line) {
                    Write-Host "<< $line" -ForegroundColor Gray
                    return @($line)
                }
            }
            catch {
                Start-Sleep -Milliseconds 10
            }
        }
        return @()
    }
    else {
        # Regular G-code - must wait for "ok" response
        $Port.WriteLine($Command)
        
        $startTime = Get-Date
        $responses = @()
        $gotOk = $false
        
        while (((Get-Date) - $startTime).TotalMilliseconds -lt $TimeoutMs) {
            try {
                $line = $Port.ReadLine()
                if ($line) {
                    Write-Host "<< $line" -ForegroundColor Gray
                    $responses += $line
                    
                    # Stop if we get "ok" or "error"
                    if ($line -match "^ok" -or $line -match "^error") {
                        $gotOk = $true
                        break
                    }
                }
            }
            catch {
                Start-Sleep -Milliseconds 10
            }
        }
        
        if (-not $gotOk) {
            Write-Host "<< [WARN] No 'ok' received within timeout" -ForegroundColor Yellow
        }
        
        # Delay matching mikroC DMA timing (allows firmware processing)
        # 200ms provides safety margin for:
        # - UART ISR to copy data from 512-byte ring buffer
        # - G-code parser to tokenize and execute command
        # - Motion system to accept command into motion buffer
        # - Serial buffers to fully drain before next command
        Start-Sleep -Milliseconds 200
        
        return $responses
    }
}

Write-Host "╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  COORDINATE SYSTEM TEST (G92, G91)                       ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Initialize serial port
Write-Host "Connecting to $Port @ $BaudRate baud..." -ForegroundColor Yellow
$serialPort = New-Object System.IO.Ports.SerialPort
$serialPort.PortName = $Port
$serialPort.BaudRate = $BaudRate
$serialPort.DataBits = 8
$serialPort.Parity = [System.IO.Ports.Parity]::None
$serialPort.StopBits = [System.IO.Ports.StopBits]::One
$serialPort.ReadTimeout = 1000
$serialPort.WriteTimeout = 1000
$serialPort.NewLine = "`n"

try {
    $serialPort.Open()
    Write-Host "✓ Connected!" -ForegroundColor Green
    Write-Host ""
    
    Start-Sleep -Milliseconds 500
    $serialPort.DiscardInBuffer()
    $serialPort.DiscardOutBuffer()
    
    # Test 1: G92 Basic Functionality
    Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Yellow
    Write-Host " TEST 1: G92 Coordinate Offset" -ForegroundColor Yellow
    Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Yellow
    Write-Host ""
    
    Write-Host "Step 1: Clear any existing offsets" -ForegroundColor White
    Send-GCode -Port $serialPort -Command "G92.1"
    
    Write-Host "`nStep 2: Check initial position" -ForegroundColor White
    Send-GCode -Port $serialPort -Command "?"
    
    Write-Host "`nStep 3: Set current position as work zero (G92 X0 Y0)" -ForegroundColor White
    Send-GCode -Port $serialPort -Command "G92 X0 Y0"
    
    Write-Host "`nStep 4: Check position after G92" -ForegroundColor White
    Send-GCode -Port $serialPort -Command "?"
    
    Write-Host "`nStep 5: Move to Y=10mm in work coordinates" -ForegroundColor White
    Send-GCode -Port $serialPort -Command "G90"
    Send-GCode -Port $serialPort -Command "G1 Y10 F1000"
    
    Write-Host "`nStep 6: Check position after move" -ForegroundColor White
    Start-Sleep -Milliseconds 1500  # Wait for motion to complete
    Send-GCode -Port $serialPort -Command "?"
    
    Write-Host "`nStep 7: Return to work zero (G1 Y0)" -ForegroundColor White
    Send-GCode -Port $serialPort -Command "G1 Y0"
    
    Write-Host "`nStep 8: Verify back at work zero" -ForegroundColor White
    Start-Sleep -Milliseconds 1500  # Wait for motion to complete
    Send-GCode -Port $serialPort -Command "?"
    
    # Test 2: G91 Relative Mode
    Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Yellow
    Write-Host " TEST 2: G91 Relative Positioning" -ForegroundColor Yellow
    Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Yellow
    Write-Host ""
    
    Write-Host "Step 1: Clear offsets and go to absolute mode" -ForegroundColor White
    Send-GCode -Port $serialPort -Command "G92.1"
    Send-GCode -Port $serialPort -Command "G90"
    
    Write-Host "`nStep 2: Check initial position" -ForegroundColor White
    Send-GCode -Port $serialPort -Command "?"
    
    Write-Host "`nStep 3: Switch to relative mode (G91)" -ForegroundColor White
    Send-GCode -Port $serialPort -Command "G91"
    
    Write-Host "`nStep 4: Move +10mm in Y (relative)" -ForegroundColor White
    Send-GCode -Port $serialPort -Command "G1 Y10 F1000"
    
    Write-Host "`nStep 5: Check position after relative move" -ForegroundColor White
    Start-Sleep -Milliseconds 1500  # Wait for motion to complete
    Send-GCode -Port $serialPort -Command "?"
    
    Write-Host "`nStep 6: Move +10mm in X (relative)" -ForegroundColor White
    Send-GCode -Port $serialPort -Command "G1 X10"
    
    Write-Host "`nStep 7: Check position after second move" -ForegroundColor White
    Start-Sleep -Milliseconds 1500  # Wait for motion to complete
    Send-GCode -Port $serialPort -Command "?"
    
    Write-Host "`nStep 8: Return with relative moves (-10mm each)" -ForegroundColor White
    Send-GCode -Port $serialPort -Command "G1 X-10"
    Start-Sleep -Milliseconds 1500  # Wait for motion to complete
    Send-GCode -Port $serialPort -Command "G1 Y-10"
    
    Write-Host "`nStep 9: Verify back at starting position" -ForegroundColor White
    Start-Sleep -Milliseconds 1500  # Wait for motion to complete
    Send-GCode -Port $serialPort -Command "?"
    
    Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Green
    Write-Host " TEST COMPLETE" -ForegroundColor Green
    Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Green
    Write-Host ""
    Write-Host "Review the position outputs above:" -ForegroundColor White
    Write-Host "  - After G92 X0 Y0: WPos should show 0.000,0.000" -ForegroundColor White
    Write-Host "  - After moves: Position should update correctly" -ForegroundColor White
    Write-Host "  - MPos and WPos should differ by the offset amount" -ForegroundColor White
    Write-Host ""
}
catch {
    Write-Host "ERROR: $_" -ForegroundColor Red
    exit 1
}
finally {
    if ($serialPort.IsOpen) {
        $serialPort.Close()
    }
    Write-Host "Serial port closed." -ForegroundColor Yellow
}
