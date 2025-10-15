# Multi-Axis Motion Test Suite
# Tests X, Y, Z axes individually and coordinated motion

param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200,
    [int]$DelayMs = 2000  # Delay between commands
)

Write-Host "=== Multi-Axis Motion Test Suite ===" -ForegroundColor Cyan
Write-Host "Port: $Port | Baud: $BaudRate`n" -ForegroundColor Gray

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
    Write-Host "✓ Serial port opened successfully`n" -ForegroundColor Green
    Start-Sleep -Milliseconds 500

    # Helper function to send command and wait for response
    function Send-Command {
        param([string]$cmd, [string]$description)
        
        Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor DarkGray
        Write-Host "TEST: $description" -ForegroundColor Yellow
        Write-Host "CMD:  $cmd" -ForegroundColor Cyan
        Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor DarkGray
        
        $serialPort.WriteLine($cmd)
        Start-Sleep -Milliseconds 100
        
        # Read response
        $timeout = 0
        while ($serialPort.BytesToRead -eq 0 -and $timeout -lt 50) {
            Start-Sleep -Milliseconds 10
            $timeout++
        }
        
        while ($serialPort.BytesToRead -gt 0) {
            $line = $serialPort.ReadLine()
            Write-Host $line
            Start-Sleep -Milliseconds 10
        }
        
        Start-Sleep -Milliseconds $DelayMs
    }

    # Test sequence
    Write-Host "`n╔════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║  PHASE 1: Individual Axis Tests       ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════╝" -ForegroundColor Cyan

    # X-axis tests
    Send-Command "G0 X10 F200" "X-axis Forward (0 → 10mm)"
    Send-Command "?" "Position Check"
    Send-Command "G0 X5 F200" "X-axis Reverse (10 → 5mm)"
    Send-Command "?" "Position Check"
    Send-Command "G0 X0 F200" "X-axis Return to Zero"
    Send-Command "?" "Position Check"

    # Y-axis tests
    Write-Host "`n╔════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║  Y-Axis Tests                          ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════╝" -ForegroundColor Cyan
    
    Send-Command "G0 Y10 F200" "Y-axis Forward (0 → 10mm)"
    Send-Command "?" "Position Check"
    Send-Command "G0 Y5 F200" "Y-axis Reverse (10 → 5mm)"
    Send-Command "?" "Position Check"
    Send-Command "G0 Y0 F200" "Y-axis Return to Zero"
    Send-Command "?" "Position Check"

    # Z-axis tests (careful - may be vertical!)
    Write-Host "`n╔════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║  Z-Axis Tests (CAREFUL!)               ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════╝" -ForegroundColor Cyan
    
    Send-Command "G0 Z5 F100" "Z-axis Forward (0 → 5mm) - SLOW"
    Send-Command "?" "Position Check"
    Send-Command "G0 Z2 F100" "Z-axis Reverse (5 → 2mm)"
    Send-Command "?" "Position Check"
    Send-Command "G0 Z0 F100" "Z-axis Return to Zero"
    Send-Command "?" "Position Check"

    # Coordinated motion tests
    Write-Host "`n╔════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║  PHASE 2: Multi-Axis Coordinated      ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════╝" -ForegroundColor Cyan

    Send-Command "G0 X10 Y10 F200" "XY Diagonal (0,0 → 10,10)"
    Send-Command "?" "Position Check"
    
    Send-Command "G0 X0 Y10 F200" "X moves, Y stays"
    Send-Command "?" "Position Check"
    
    Send-Command "G0 X10 Y0 F200" "Both axes reverse"
    Send-Command "?" "Position Check"
    
    Send-Command "G0 X0 Y0 F200" "Return to origin"
    Send-Command "?" "Position Check"

    # 3-axis test
    Write-Host "`n╔════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║  PHASE 3: Three-Axis Test              ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════╝" -ForegroundColor Cyan
    
    Send-Command "G0 X5 Y5 Z2 F150" "XYZ Move (0,0,0 → 5,5,2)"
    Send-Command "?" "Position Check"
    
    Send-Command "G0 X0 Y0 Z0 F150" "Return to absolute zero"
    Send-Command "?" "Final Position Check"

    Write-Host "`n╔════════════════════════════════════════╗" -ForegroundColor Green
    Write-Host "║  Test Sequence Complete                ║" -ForegroundColor Green
    Write-Host "╚════════════════════════════════════════╝" -ForegroundColor Green

} catch {
    Write-Host "ERROR: $_" -ForegroundColor Red
} finally {
    if ($serialPort.IsOpen) {
        $serialPort.Close()
        Write-Host "`n✓ Serial port closed" -ForegroundColor Green
    }
}

Write-Host "`nPress any key to exit..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
