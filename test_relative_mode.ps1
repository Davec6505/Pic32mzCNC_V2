# Test OCR period scaling with RELATIVE mode (G91) to bypass coordinate offset bugs

param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200
)

$serialPort = New-Object System.IO.Ports.SerialPort
$serialPort.PortName = $Port
$serialPort.BaudRate = $BaudRate
$serialPort.DataBits = 8
$serialPort.Parity = [System.IO.Ports.Parity]::None
$serialPort.StopBits = [System.IO.Ports.StopBits]::One
$serialPort.ReadTimeout = 2000
$serialPort.WriteTimeout = 1000

try {
    $serialPort.Open()
    Write-Host "Connected to $Port" -ForegroundColor Green
    
    # Wait for startup
    Start-Sleep -Milliseconds 1000
    $startup = $serialPort.ReadExisting()
    
    Write-Host "`n===========================================================" -ForegroundColor Cyan
    Write-Host "Testing OCR Period Scaling with RELATIVE MODE (G91)" -ForegroundColor Cyan
    Write-Host "===========================================================" -ForegroundColor Cyan
    
    # Switch to RELATIVE mode
    Write-Host "`n>>> G91 (relative mode)" -ForegroundColor Yellow
    $serialPort.WriteLine("G91")
    Start-Sleep -Milliseconds 300
    Write-Host $serialPort.ReadExisting() -ForegroundColor Gray
    
    # Move Y+10mm (relative)
    Write-Host "`n>>> G1 Y10 F1000 (move +10mm relative)" -ForegroundColor Yellow
    Write-Host "Expected: 800 steps generated, hardware moves exactly 10mm" -ForegroundColor Cyan
    $serialPort.WriteLine("G1 Y10 F1000")
    Start-Sleep -Milliseconds 300
    $response = $serialPort.ReadExisting()
    Write-Host $response -ForegroundColor White
    
    # Wait for motion to complete and check position
    Write-Host "`nWaiting for motion to complete..." -ForegroundColor Gray
    Start-Sleep -Milliseconds 3000
    
    # Query position
    Write-Host "`n>>> ? (status query)" -ForegroundColor Yellow
    $serialPort.Write("?")
    Start-Sleep -Milliseconds 200
    $status = $serialPort.ReadLine()
    Write-Host $status -ForegroundColor White
    
    # Parse position
    if ($status -match "MPos:([-\d.]+),([-\d.]+),([-\d.]+)") {
        $y_pos = [float]$matches[2]
        Write-Host "`nY Position: $y_pos mm" -ForegroundColor Cyan
        
        $error = [Math]::Abs($y_pos - 10.0)
        if ($error -lt 0.01) {
            Write-Host "✓ PASS: Motion accurate within 0.01mm!" -ForegroundColor Green
            Write-Host "OCR Period Scaling is working correctly!" -ForegroundColor Green
        } elseif ($y_pos -gt 12) {
            Write-Host "✗ FAIL: Still overshooting ($y_pos mm instead of 10mm)" -ForegroundColor Red
            Write-Host "This was the original bug - timer overflow" -ForegroundColor Red
        } elseif ($y_pos -lt 9) {
            Write-Host "✗ FAIL: Undershooting ($y_pos mm instead of 10mm)" -ForegroundColor Red
            Write-Host "Steps generated: ~$([int]($y_pos * 80))/800" -ForegroundColor Yellow
        } else {
            Write-Host "⚠ Position error: $error mm" -ForegroundColor Yellow
        }
    }
    
} finally {
    if ($serialPort.IsOpen) {
        $serialPort.Close()
    }
    Write-Host "`nDisconnected" -ForegroundColor Gray
}
