# test_slow_bytes.ps1 - Send bytes SLOWLY to rule out timing issues

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
    
    # Test: Send "?" byte by byte with 50ms delays
    Write-Host "`n=== TEST: Sending '?' one byte at a time ===" -ForegroundColor Cyan
    Write-Host ">> Sending byte 63 ('?')" -ForegroundColor Yellow
    $serialPort.Write([byte[]]@(63), 0, 1)
    Start-Sleep -Milliseconds 50
    
    Write-Host ">> Sending byte 13 ('\\r')" -ForegroundColor Yellow
    $serialPort.Write([byte[]]@(13), 0, 1)
    Start-Sleep -Milliseconds 50
    
    Write-Host ">> Sending byte 10 ('\\n')" -ForegroundColor Yellow
    $serialPort.Write([byte[]]@(10), 0, 1)
    Start-Sleep -Milliseconds 50
    
    # Read all output for 2 seconds
    Write-Host "`nReading response..." -ForegroundColor Cyan
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    while ($stopwatch.Elapsed.TotalSeconds -lt 2) {
        if ($serialPort.BytesToRead -gt 0) {
            $line = $serialPort.ReadLine()
            Write-Host "<< $line" -ForegroundColor White
        }
        Start-Sleep -Milliseconds 10
    }
    
    Write-Host "`n✓ Test complete!" -ForegroundColor Green
}
catch {
    Write-Host "✗ Error: $_" -ForegroundColor Red
}
finally {
    if ($serialPort.IsOpen) {
        $serialPort.Close()
    }
}
