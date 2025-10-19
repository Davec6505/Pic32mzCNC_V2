# test_diagnostic.ps1 - Test with diagnostic byte logging
# This will show EXACTLY what bytes are received by the firmware

param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200,
    [int]$TimeoutMs = 5000
)

# Open serial port
$serialPort = New-Object System.IO.Ports.SerialPort
$serialPort.PortName = $Port
$serialPort.BaudRate = $BaudRate
$serialPort.DataBits = 8
$serialPort.Parity = 'None'
$serialPort.StopBits = 'One'
$serialPort.Handshake = 'None'
$serialPort.ReadTimeout = $TimeoutMs
$serialPort.WriteTimeout = $TimeoutMs
$serialPort.NewLine = "`n"

try {
    $serialPort.Open()
    Write-Host "✓ Connected to $Port @ $BaudRate baud" -ForegroundColor Green
    
    # Clear any buffered data
    Start-Sleep -Milliseconds 500
    if ($serialPort.BytesToRead -gt 0) {
        $serialPort.ReadExisting() | Out-Null
    }
    
    # Test 1: Send simple "G90" command
    Write-Host "`n=== TEST 1: Simple G90 command ===" -ForegroundColor Cyan
    Write-Host ">> Sending: 'G90' (bytes: 71 57 48 13 10)" -ForegroundColor Yellow
    $serialPort.WriteLine("G90")
    
    # Read all output for 2 seconds
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    while ($stopwatch.Elapsed.TotalSeconds -lt 2) {
        if ($serialPort.BytesToRead -gt 0) {
            $line = $serialPort.ReadLine()
            Write-Host "<< $line" -ForegroundColor White
        }
        Start-Sleep -Milliseconds 10
    }
    
    # Test 2: Send "G92 X0 Y0" with spaces
    Write-Host "`n=== TEST 2: G92 X0 Y0 with spaces ===" -ForegroundColor Cyan
    Write-Host ">> Sending: 'G92 X0 Y0' (bytes: 71 57 50 32 88 48 32 89 48 13 10)" -ForegroundColor Yellow
    $serialPort.WriteLine("G92 X0 Y0")
    
    # Read all output for 2 seconds
    $stopwatch.Restart()
    while ($stopwatch.Elapsed.TotalSeconds -lt 2) {
        if ($serialPort.BytesToRead -gt 0) {
            $line = $serialPort.ReadLine()
            Write-Host "<< $line" -ForegroundColor White
        }
        Start-Sleep -Milliseconds 10
    }
    
    # Test 3: Send "G1 Y10 F1000"
    Write-Host "`n=== TEST 3: G1 Y10 F1000 ===" -ForegroundColor Cyan
    Write-Host ">> Sending: 'G1 Y10 F1000'" -ForegroundColor Yellow
    $serialPort.WriteLine("G1 Y10 F1000")
    
    # Read all output for 2 seconds
    $stopwatch.Restart()
    while ($stopwatch.Elapsed.TotalSeconds -lt 2) {
        if ($serialPort.BytesToRead -gt 0) {
            $line = $serialPort.ReadLine()
            Write-Host "<< $line" -ForegroundColor White
        }
        Start-Sleep -Milliseconds 10
    }
    
    Write-Host "`n✓ Test complete!" -ForegroundColor Green
    Write-Host "Review the [BYTE] diagnostic lines to see exact byte reception" -ForegroundColor Cyan
}
catch {
    Write-Host "✗ Error: $_" -ForegroundColor Red
}
finally {
    if ($serialPort.IsOpen) {
        $serialPort.Close()
    }
}
