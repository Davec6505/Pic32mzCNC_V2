# Quick Debug Test for CNC Position Feedback
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

    Write-Host "=== Quick Debug Test ===" -ForegroundColor Green
    Write-Host "Opening $Port..." -ForegroundColor Green
    $serialPort.Open()
    
    if ($serialPort.IsOpen) {
        Write-Host "Connected!" -ForegroundColor Green
        
        # Test sequence
        $commands = @("?", "DEBUG", "F100", "G1 X1", "?", "DEBUG")
        
        foreach ($cmd in $commands) {
            Write-Host "`nSending: '$cmd'" -ForegroundColor Yellow
            $serialPort.Write("$cmd`r`n")
            
            Start-Sleep -Milliseconds 500
            
            if ($serialPort.BytesToRead -gt 0) {
                $response = $serialPort.ReadExisting()
                Write-Host "Response: $response" -ForegroundColor Cyan
            }
            
            Start-Sleep -Milliseconds 1000
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