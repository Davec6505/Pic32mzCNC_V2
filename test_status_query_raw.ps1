# Test script to verify '?' status query responds every time
# This version sends RAW '?' without line terminators (matching UGS behavior)

param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200,
    [int]$TestCount = 10
)

# Open serial port
$serialPort = New-Object System.IO.Ports.SerialPort
$serialPort.PortName = $Port
$serialPort.BaudRate = $BaudRate
$serialPort.DataBits = 8
$serialPort.Parity = [System.IO.Ports.Parity]::None
$serialPort.StopBits = [System.IO.Ports.StopBits]::One
$serialPort.ReadTimeout = 500
$serialPort.WriteTimeout = 500

try {
    $serialPort.Open()
    Write-Host "Connected to $Port @ $BaudRate" -ForegroundColor Green
    
    # Wait for startup message
    Start-Sleep -Milliseconds 500
    $serialPort.ReadExisting() | Out-Null
    
    Write-Host "`nTesting '?' status query $TestCount times..." -ForegroundColor Cyan
    Write-Host "Sending RAW '?' (single byte, no line terminators)" -ForegroundColor Yellow
    Write-Host "Expected: All queries should get immediate responses`n" -ForegroundColor Yellow
    
    $successCount = 0
    $failCount = 0
    
    for ($i = 1; $i -le $TestCount; $i++) {
        Write-Host "Query #${i}: " -NoNewline
        
        # Send RAW '?' command (single byte, no \r\n)
        $serialPort.Write("?")
        Start-Sleep -Milliseconds 100
        
        # Try to read response
        try {
            $response = $serialPort.ReadLine()
            if ($response -match "<.*>") {
                Write-Host "✓ Got response: $response" -ForegroundColor Green
                $successCount++
            } else {
                Write-Host "✗ Unexpected response: $response" -ForegroundColor Red
                $failCount++
            }
        } catch {
            Write-Host "✗ NO RESPONSE (timeout)" -ForegroundColor Red
            $failCount++
        }
    }
    
    Write-Host "`n$('=' * 60)" -ForegroundColor Cyan
    Write-Host "Test Results:" -ForegroundColor Cyan
    Write-Host "  Success: $successCount / $TestCount" -ForegroundColor $(if ($successCount -eq $TestCount) { "Green" } else { "Yellow" })
    Write-Host "  Failed:  $failCount / $TestCount" -ForegroundColor $(if ($failCount -gt 0) { "Red" } else { "Green" })
    
    if ($failCount -eq 0) {
        Write-Host "`n✓ All queries responded correctly!" -ForegroundColor Green
    } else {
        Write-Host "`n✗ Some queries did not respond - buffer issue detected!" -ForegroundColor Red
        if ($failCount -eq $successCount) {
            Write-Host "Pattern suggests every 2nd query fails" -ForegroundColor Yellow
        }
    }
    
} finally {
    if ($serialPort.IsOpen) {
        $serialPort.Close()
    }
    Write-Host "`nDisconnected from $Port" -ForegroundColor Gray
}
