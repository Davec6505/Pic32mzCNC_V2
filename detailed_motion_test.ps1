# Detailed Motion Debug Test
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

    Write-Host "=== Detailed Motion Debug Test ===" -ForegroundColor Green
    $serialPort.Open()
    
    if ($serialPort.IsOpen) {
        Write-Host "Connected!" -ForegroundColor Green
        
        # Function to send command and get response
        function Send-Command($cmd) {
            Write-Host "`nSending: '$cmd'" -ForegroundColor Yellow
            $serialPort.Write("$cmd`r`n")
            Start-Sleep -Milliseconds 200
            
            if ($serialPort.BytesToRead -gt 0) {
                $response = $serialPort.ReadExisting()
                Write-Host "Response: " -NoNewline -ForegroundColor White
                Write-Host $response.Trim() -ForegroundColor Cyan
                return $response
            }
            return ""
        }
        
        # Test sequence with detailed debugging
        Send-Command "?"
        Send-Command "DEBUG"
        
        Write-Host "`n--- Starting Motion Test ---" -ForegroundColor Green
        
        # Send G1 X1 and immediately check status multiple times
        Send-Command "G1 X1"
        
        for ($i = 1; $i -le 10; $i++) {
            Start-Sleep -Milliseconds 200
            Write-Host "`n[Check $i]" -ForegroundColor Gray
            Send-Command "?"
            Send-Command "DEBUG"
        }
        
        Write-Host "`n--- Motion Test Complete ---" -ForegroundColor Green
        
        # Try a larger move
        Write-Host "`n--- Testing Larger Move ---" -ForegroundColor Green
        Send-Command "G1 X100"
        
        for ($i = 1; $i -le 5; $i++) {
            Start-Sleep -Milliseconds 500
            Write-Host "`n[Large Move Check $i]" -ForegroundColor Gray
            Send-Command "?"
            Send-Command "DEBUG"
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