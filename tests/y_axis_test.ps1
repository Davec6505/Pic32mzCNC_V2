# Quick Y-Axis Only Test
# Simple script to test just Y-axis motion

param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200
)

Write-Host "=== Y-Axis Quick Test ===" -ForegroundColor Cyan

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
    Write-Host "✓ Port opened`n" -ForegroundColor Green
    Start-Sleep -Milliseconds 500

    function Send-Cmd {
        param([string]$cmd)
        Write-Host "`nSending: $cmd" -ForegroundColor Yellow
        $serialPort.WriteLine($cmd)
        Start-Sleep -Milliseconds 100
        
        $timeout = 0
        while ($serialPort.BytesToRead -eq 0 -and $timeout -lt 50) {
            Start-Sleep -Milliseconds 10
            $timeout++
        }
        
        while ($serialPort.BytesToRead -gt 0) {
            $line = $serialPort.ReadLine()
            Write-Host $line
        }
        Start-Sleep -Milliseconds 2000
    }

    Write-Host "`n--- Test 1: Y-axis forward ---" -ForegroundColor Cyan
    Send-Cmd "G0 Y10 F200"
    Send-Cmd "?"
    
    Write-Host "`n--- Test 2: Y-axis reverse ---" -ForegroundColor Cyan
    Send-Cmd "G0 Y1 F200"
    Send-Cmd "?"
    
    Write-Host "`n--- Test 3: Return to zero ---" -ForegroundColor Cyan
    Send-Cmd "G0 Y0 F200"
    Send-Cmd "?"

    Write-Host "`n✓ Y-axis tests complete" -ForegroundColor Green

} catch {
    Write-Host "ERROR: $_" -ForegroundColor Red
} finally {
    if ($serialPort.IsOpen) {
        $serialPort.Close()
    }
}
