# Diagnostic test for position tracking issues
$port = "COM4"
$baudRate = 115200

try {
    $serialPort = New-Object System.IO.Ports.SerialPort
    $serialPort.PortName = $port
    $serialPort.BaudRate = $baudRate
    $serialPort.DataBits = 8
    $serialPort.Parity = "None"
    $serialPort.StopBits = "One"
    $serialPort.Handshake = "None"
    $serialPort.ReadTimeout = 2000
    $serialPort.WriteTimeout = 2000
    
    $serialPort.Open()
    Write-Host "Connected to $port at $baudRate baud" -ForegroundColor Green
    
    Start-Sleep -Milliseconds 500
    $serialPort.DiscardInBuffer()
    $serialPort.DiscardOutBuffer()
    
    Write-Host "`n=== Diagnosing Position Tracking Issues ===" -ForegroundColor Yellow
    
    # Send a detailed status command to see what's happening
    Write-Host "`nRequesting detailed debug status..." -ForegroundColor Cyan
    $serialPort.WriteLine("`$`$D")
    
    # Wait for detailed response
    $startTime = Get-Date
    while (((Get-Date) - $startTime).TotalMilliseconds -lt 2000) {
        if ($serialPort.BytesToRead -gt 0) {
            $data = $serialPort.ReadExisting()
            Write-Host $data -NoNewline -ForegroundColor White
        }
        Start-Sleep -Milliseconds 50
    }
    
} catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
} finally {
    if ($serialPort -and $serialPort.IsOpen) {
        $serialPort.Close()
        Write-Host "`nSerial connection closed." -ForegroundColor Green
    }
}