# Motion feedback test - Send a long move and query status during motion
param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200
)

Write-Host "Opening $Port at $BaudRate baud..." -ForegroundColor Cyan

# Open serial port
$serialPort = New-Object System.IO.Ports.SerialPort
$serialPort.PortName = $Port
$serialPort.BaudRate = $BaudRate
$serialPort.Parity = [System.IO.Ports.Parity]::None
$serialPort.DataBits = 8
$serialPort.StopBits = [System.IO.Ports.StopBits]::One
$serialPort.ReadTimeout = 1000
$serialPort.WriteTimeout = 1000

try {
    $serialPort.Open()
    Write-Host "Connected!" -ForegroundColor Green
    Start-Sleep -Milliseconds 500
    
    # Clear any startup messages
    while ($serialPort.BytesToRead -gt 0) {
        $serialPort.ReadExisting() | Out-Null
    }
    
    Write-Host "`n=== Sending long X-axis move: G0 X100 F50 ===" -ForegroundColor Yellow
    Write-Host "This should take 120 seconds (2 minutes) to complete" -ForegroundColor Yellow
    Write-Host "We'll query status every 2 seconds to see live position updates`n" -ForegroundColor Yellow
    
    # Send the long move command
    $serialPort.WriteLine("G0 X100 F50")
    Start-Sleep -Milliseconds 100
    
    # Read response
    $response = ""
    $timeout = [datetime]::Now.AddSeconds(2)
    while ([datetime]::Now -lt $timeout) {
        if ($serialPort.BytesToRead -gt 0) {
            $response += $serialPort.ReadExisting()
        }
        Start-Sleep -Milliseconds 50
    }
    Write-Host "Command response: $response" -ForegroundColor Green
    
    # Query status repeatedly during motion
    Write-Host "`nQuerying status during motion:" -ForegroundColor Cyan
    for ($i = 0; $i -lt 30; $i++) {
        Start-Sleep -Seconds 2
        
        # Send status query
        $serialPort.WriteLine("?")
        Start-Sleep -Milliseconds 100
        
        # Read status response
        $status = ""
        $timeout = [datetime]::Now.AddSeconds(1)
        while ([datetime]::Now -lt $timeout) {
            if ($serialPort.BytesToRead -gt 0) {
                $status += $serialPort.ReadExisting()
            }
            Start-Sleep -Milliseconds 50
        }
        
        if ($status) {
            Write-Host "[$(Get-Date -Format 'HH:mm:ss')] $status" -ForegroundColor White -NoNewline
        }
        
        # Check if motion is complete (position reached 100)
        if ($status -match "MPos:100\.000") {
            Write-Host "`nMotion complete!" -ForegroundColor Green
            break
        }
    }
    
    Write-Host "`nFinal status check:" -ForegroundColor Cyan
    Start-Sleep -Seconds 1
    $serialPort.WriteLine("?")
    Start-Sleep -Milliseconds 500
    $finalStatus = $serialPort.ReadExisting()
    Write-Host $finalStatus -ForegroundColor Green
}
catch {
    Write-Host "Error: $_" -ForegroundColor Red
}
finally {
    if ($serialPort.IsOpen) {
        $serialPort.Close()
        Write-Host "`nSerial port closed." -ForegroundColor Cyan
    }
}
