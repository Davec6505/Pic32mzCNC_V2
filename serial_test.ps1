# Simple PowerShell Serial Monitor
param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200
)

try {
    # Load .NET serial port class
    Add-Type -AssemblyName System.IO.Ports

    # Create and configure serial port
    $serialPort = New-Object System.IO.Ports.SerialPort
    $serialPort.PortName = $Port
    $serialPort.BaudRate = $BaudRate
    $serialPort.DataBits = 8
    $serialPort.Parity = [System.IO.Ports.Parity]::None
    $serialPort.StopBits = [System.IO.Ports.StopBits]::One
    $serialPort.ReadTimeout = 1000
    $serialPort.WriteTimeout = 1000

    Write-Host "Opening $Port at $BaudRate baud..." -ForegroundColor Green
    $serialPort.Open()
    
    if ($serialPort.IsOpen) {
        Write-Host "Connected! Press Ctrl+C to exit" -ForegroundColor Green
        
        # Automatically send a valid GRBL command ($$) after connecting
        Write-Host "Sending '$$' command..." -ForegroundColor Yellow
        $serialPort.Write("`$`$`r`n")

        # Start reading in background
        while ($true) {
            try {
                if ($serialPort.BytesToRead -gt 0) {
                    $data = $serialPort.ReadExisting()
                    Write-Host $data -NoNewline -ForegroundColor Cyan
                }
                
                # Check for keyboard input
                if ([Console]::KeyAvailable) {
                    $key = [Console]::ReadKey($true)
                    if ($key.Key -eq 'Escape') {
                        break
                    }
                    
                    # Send the key to serial port
                    $serialPort.Write($key.KeyChar.ToString())
                    Write-Host $key.KeyChar -NoNewline -ForegroundColor White
                    
                    # Send CR+LF on Enter
                    if ($key.Key -eq 'Enter') {
                        $serialPort.Write("`r`n")
                        Write-Host ""
                    }
                }
                
                Start-Sleep -Milliseconds 50
            }
            catch {
                Write-Host "Read error: $_" -ForegroundColor Red
                break
            }
        }
    }
}
catch {
    Write-Host "Error: $_" -ForegroundColor Red
}
finally {
    if ($serialPort -and $serialPort.IsOpen) {
        $serialPort.Close()
        Write-Host "`nSerial port closed." -ForegroundColor Green
    }
}