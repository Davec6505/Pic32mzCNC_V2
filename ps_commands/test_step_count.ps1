# Test step counting accuracy
# Simple forward/reverse test with step count verification

param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200,
    [int]$DelayMs = 200
)

Write-Host "=== STEP COUNT VERIFICATION TEST ===" -ForegroundColor Cyan
Write-Host "Port: $Port @ $BaudRate baud" -ForegroundColor Gray
Write-Host ""

# Open serial port
$serialPort = New-Object System.IO.Ports.SerialPort
$serialPort.PortName = $Port
$serialPort.BaudRate = $BaudRate
$serialPort.DataBits = 8
$serialPort.Parity = [System.IO.Ports.Parity]::None
$serialPort.StopBits = [System.IO.Ports.StopBits]::One
$serialPort.Handshake = [System.IO.Ports.Handshake]::None
$serialPort.ReadTimeout = 5000
$serialPort.WriteTimeout = 5000

try {
    $serialPort.Open()
    Write-Host "Serial port opened successfully" -ForegroundColor Green
    Start-Sleep -Milliseconds 500
    
    # Clear any startup messages
    Start-Sleep -Milliseconds 500
    $serialPort.DiscardInBuffer()
    
    # Test sequence
    Write-Host "`n--- Setting zero position ---" -ForegroundColor Yellow
    Write-Host ">> G92 X0 Y0"
    $serialPort.WriteLine("G92 X0 Y0")
    Start-Sleep -Milliseconds $DelayMs
    
    # Read all responses
    while ($serialPort.BytesToRead -gt 0) {
        $line = $serialPort.ReadLine()
        Write-Host "<< $line" -ForegroundColor Gray
    }
    
    Write-Host "`n--- Test 1: 10mm forward (slow) ---" -ForegroundColor Yellow
    Write-Host ">> G1 X10 F500"
    $serialPort.WriteLine("G1 X10 F500")
    Start-Sleep -Milliseconds $DelayMs
    
    # Wait for motion to complete
    Start-Sleep -Milliseconds 3000
    
    # Read all responses (including STEP_COUNT debug)
    while ($serialPort.BytesToRead -gt 0) {
        $line = $serialPort.ReadLine()
        Write-Host "<< $line" -ForegroundColor Gray
    }
    
    Write-Host "`n--- Query position ---" -ForegroundColor Yellow
    Write-Host ">> ?"
    $serialPort.WriteLine("?")
    Start-Sleep -Milliseconds $DelayMs
    
    while ($serialPort.BytesToRead -gt 0) {
        $line = $serialPort.ReadLine()
        Write-Host "<< $line" -ForegroundColor Cyan
    }
    
    Write-Host "`n--- Test 2: Return to zero ---" -ForegroundColor Yellow
    Write-Host ">> G1 X0 F500"
    $serialPort.WriteLine("G1 X0 F500")
    Start-Sleep -Milliseconds $DelayMs
    
    # Wait for motion to complete
    Start-Sleep -Milliseconds 3000
    
    # Read all responses (including STEP_COUNT debug)
    while ($serialPort.BytesToRead -gt 0) {
        $line = $serialPort.ReadLine()
        Write-Host "<< $line" -ForegroundColor Gray
    }
    
    Write-Host "`n--- Final position check ---" -ForegroundColor Yellow
    Write-Host ">> ?"
    $serialPort.WriteLine("?")
    Start-Sleep -Milliseconds $DelayMs
    
    while ($serialPort.BytesToRead -gt 0) {
        $line = $serialPort.ReadLine()
        Write-Host "<< $line" -ForegroundColor Cyan
    }
    
    Write-Host "`n=== TEST COMPLETE ===" -ForegroundColor Green
    Write-Host "Expected: MPos:0.000 (or very close)" -ForegroundColor Yellow
    Write-Host "Check [STEP_COUNT] messages for Cmd vs Exec" -ForegroundColor Yellow
}
catch {
    Write-Host "Error: $_" -ForegroundColor Red
}
finally {
    if ($serialPort.IsOpen) {
        $serialPort.Close()
    }
}
