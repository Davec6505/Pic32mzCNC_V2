[CmdletBinding()]
param (
    [Parameter()]
    [ValidateRange(300, 115200)]
    [int32]$baudrate = 115200,
    [ValidateRange(5, 8)]
    [int32]$databits = 8,
    [ValidateRange(1, 2)]
    [int32]$stopbits = 1,
    [System.IO.Ports.Parity]$parity = [System.IO.Ports.Parity]::None,
    [string]$portname = "COM4"
)


# Create and configure the serial port
try {
    $serialPort = New-Object System.IO.Ports.SerialPort
    $serialPort.PortName = $portname
    $serialPort.BaudRate = $baudrate
    $serialPort.Parity = $parity
    $serialPort.DataBits = $databits
    $serialPort.StopBits = $stopbits
    $serialPort.Handshake = [System.IO.Ports.Handshake]::None
    $serialPort.ReadTimeout = 5000  # 5 seconds
    $serialPort.WriteTimeout = 5000 # 5 seconds
    
    Write-Host "Opening serial port $portname @ $baudrate baud..." -ForegroundColor Cyan
    $serialPort.Open()
    
    if ($serialPort.IsOpen) {
        Write-Host "✓ Serial port opened successfully!" -ForegroundColor Green
        Write-Host "Port: $portname | Baud: $baudrate | Data: $databits | Stop: $stopbits | Parity: $parity" -ForegroundColor Yellow
    }
}
catch {
    Write-Host "✗ Failed to open serial port: $_" -ForegroundColor Red
    exit 1
}

# Example: Read and write data
try {
    # Send a command
    $command = "?`r`n"  # GRBL status query
    Write-Host "`nSending: $command" -ForegroundColor Cyan
    $serialPort.WriteLine($command)
    
    # Read response
    Start-Sleep -Milliseconds 100
    if ($serialPort.BytesToRead -gt 0) {
        $response = $serialPort.ReadExisting()
        Write-Host "Received: $response" -ForegroundColor Green
    }

    $command = "G0 X10 Y10`r`n"  # Example G-code command
    Write-Host "`nSending: $command" -ForegroundColor Cyan
    $serialPort.WriteLine($command)
    Start-Sleep -Milliseconds 100
    if ($serialPort.BytesToRead -gt 0) {
        $response = $serialPort.ReadExisting()
        Write-Host "Received: $response" -ForegroundColor Green
    }
    
}
catch {
    Write-Host "Communication error: $_" -ForegroundColor Red
}
finally {
    # Always close the port when done
    if ($serialPort.IsOpen) {
        $serialPort.Close()
        Write-Host "`n✓ Serial port closed" -ForegroundColor Yellow
    }
    $serialPort.Dispose()
}
