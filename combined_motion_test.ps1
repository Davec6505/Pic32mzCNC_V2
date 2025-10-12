#!/usr/bin/env pwsh
# Combined motion test with proper G-code syntax

Write-Host "=== Combined G-code Motion Test ===" -ForegroundColor Cyan
Write-Host "Testing motion with embedded feedrate..."

$port = "COM4"
$baud = 115200

# Open serial port
$serial = New-Object System.IO.Ports.SerialPort
$serial.PortName = $port
$serial.BaudRate = $baud
$serial.DataBits = 8
$serial.Parity = [System.IO.Ports.Parity]::None
$serial.StopBits = [System.IO.Ports.StopBits]::One
$serial.ReadTimeout = 2000
$serial.WriteTimeout = 2000

try {
    Write-Host "Opening $port..."
    $serial.Open()
    Write-Host "Connected!" -ForegroundColor Green
    
    Start-Sleep -Milliseconds 1000
    
    # Test commands with embedded feedrate
    $commands = @(
        '?',
        'DEBUG',
        'G1 X1 F100',  # Move X to 1mm at 100mm/min feedrate
        '?',
        'DEBUG',
        'G1 Y1 F100',  # Move Y to 1mm at 100mm/min feedrate  
        '?',
        'DEBUG',
        'G1 X0 Y0 F200', # Return to origin at faster feedrate
        '?',
        'DEBUG'
    )
    
    foreach ($cmd in $commands) {
        Write-Host "`nSending: '$cmd'" -ForegroundColor Yellow
        $serial.WriteLine($cmd)
        Start-Sleep -Milliseconds 100
        
        # Read response
        try {
            $response = $serial.ReadLine().Trim()
            Write-Host "Response: $response" -ForegroundColor Green
        }
        catch {
            Write-Host "No response or timeout" -ForegroundColor Red
        }
        
        # Extra delay for motion commands
        if ($cmd.StartsWith("G1")) {
            Write-Host "  [Waiting 2000ms for motion...]" -ForegroundColor Gray
            Start-Sleep -Milliseconds 2000
            
            # Get status after motion
            $serial.WriteLine('?')
            Start-Sleep -Milliseconds 100
            try {
                $status = $serial.ReadLine().Trim()
                Write-Host "  After motion: $status" -ForegroundColor Cyan
            }
            catch {
                Write-Host "  No status response" -ForegroundColor Red
            }
        }
    }
    
} catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
} finally {
    if ($serial.IsOpen) {
        $serial.Close()
        Write-Host "`nSerial port closed." -ForegroundColor Yellow
    }
}

Write-Host "`nTest completed. Check the position feedback above!" -ForegroundColor Cyan