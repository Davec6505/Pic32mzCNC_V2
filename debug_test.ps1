# Debug test script for stepper movement
param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200
)

Write-Host "Opening $Port at $BaudRate baud..." -ForegroundColor Green

try {
    # Open serial port
    $serialPort = New-Object System.IO.Ports.SerialPort
    $serialPort.PortName = $Port
    $serialPort.BaudRate = $BaudRate
    $serialPort.DataBits = 8
    $serialPort.Parity = [System.IO.Ports.Parity]::None
    $serialPort.StopBits = [System.IO.Ports.StopBits]::One
    $serialPort.Handshake = [System.IO.Ports.Handshake]::None
    $serialPort.ReadTimeout = 5000
    $serialPort.WriteTimeout = 5000
    
    $serialPort.Open()
    Write-Host "Connected! Testing steppers..." -ForegroundColor Green
    
    # Wait for initialization
    Start-Sleep -Milliseconds 1000
    
    # Clear any initial data
    while ($serialPort.BytesToRead -gt 0) {
        $null = $serialPort.ReadExisting()
    }
    
    # Test commands
    $commands = @(
        "G21",           # Set units to mm
        "G90",           # Absolute positioning  
        "G0X10F100",     # Move X axis 10mm at 100mm/min
        "?",             # Status query
        "G0X0F100",      # Return to home
        "?",             # Status query
        "G0Y10F100",     # Move Y axis
        "?",             # Status query
        "G0X10Y10F100",  # Move both axes
        "?"              # Final status
    )
    
    foreach ($cmd in $commands) {
        Write-Host "Sending: $cmd" -ForegroundColor Yellow
        $serialPort.WriteLine($cmd)
        
        # Wait for response
        Start-Sleep -Milliseconds 500
        
        # Read all available data
        $response = ""
        while ($serialPort.BytesToRead -gt 0) {
            $response += $serialPort.ReadExisting()
            Start-Sleep -Milliseconds 50
        }
        
        if ($response) {
            Write-Host "Response: $response" -ForegroundColor Cyan
        }
        
        # Extra delay for movement commands
        if ($cmd.StartsWith("G0")) {
            Write-Host "Waiting for movement to complete..." -ForegroundColor Magenta
            Start-Sleep -Milliseconds 2000
        }
    }
    
} catch {
    Write-Error "Error: $($_.Exception.Message)"
} finally {
    if ($serialPort -and $serialPort.IsOpen) {
        $serialPort.Close()
        Write-Host "Serial port closed." -ForegroundColor Red
    }
}