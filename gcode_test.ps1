#!/usr/bin/env pwsh

# Script to test G-code commands and motion reporting
# Usage: ./gcode_test.ps1

$PORT = "COM4"
$BAUD_RATE = 115200

try {
    Write-Host "Opening $PORT at $BAUD_RATE baud..."
    $serialPort = New-Object System.IO.Ports.SerialPort
    $serialPort.PortName = $PORT
    $serialPort.BaudRate = $BAUD_RATE
    $serialPort.Open()
    Write-Host "Connected! Testing G-code commands..."

    # Wait for system to initialize
    Start-Sleep -Seconds 2
    
    # Send status query first
    Write-Host "`nSending initial status query..."
    $serialPort.WriteLine("?")
    Start-Sleep -Milliseconds 100
    
    # Read any buffered data
    while ($serialPort.BytesToRead -gt 0) {
        $data = $serialPort.ReadExisting()
        Write-Host $data -NoNewline
    }
    
    # Send some basic G-code commands
    $commands = @(
        "G21",          # Set units to millimeters
        "G90",          # Absolute positioning
        "G94",          # Feed rate units per minute
        "?",            # Status after setup
        "G0 X10",       # Rapid move to X=10
        "?",            # Status after G0 X10
        "G0 Y10",       # Rapid move to Y=10  
        "?",            # Status after G0 Y10
        "G1 X20 F100",  # Linear move to X=20 at 100mm/min
        "?",            # Status after G1 X20
        "G1 Y20 F100",  # Linear move to Y=20 at 100mm/min
        "?",            # Status after G1 Y20
        "G0 X0 Y0",     # Return to origin
        "?"             # Final status
    )
    
    foreach ($command in $commands) {
        Write-Host "`nSending: $command"
        $serialPort.WriteLine($command)
        
        # Wait for response
        Start-Sleep -Milliseconds 200
        
        # Read response
        $response = ""
        $timeout = 10  # 1 second timeout
        while ($timeout -gt 0 -and $serialPort.BytesToRead -eq 0) {
            Start-Sleep -Milliseconds 100
            $timeout--
        }
        
        if ($serialPort.BytesToRead -gt 0) {
            $response = $serialPort.ReadExisting()
            Write-Host $response.Trim()
        } else {
            Write-Host "No response received"
        }
        
        # Small delay between commands
        Start-Sleep -Milliseconds 100
    }
    
    Write-Host "`nTest complete!"
    
} catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
} finally {
    if ($serialPort -and $serialPort.IsOpen) {
        Write-Host "Serial port closed."
        $serialPort.Close()
    }
}