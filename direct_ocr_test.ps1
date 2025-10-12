#!/usr/bin/env pwsh
# Direct OCR test with known good values

Write-Host "=== Direct OCR Test ===" -ForegroundColor Cyan
Write-Host "Testing direct OCR step pulse generation..."

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
    
    # Test sequence focusing on step counts 
    $commands = @(
        'DEBUG',
        '?',
        '$#',  # Show coordinate systems
        'G1 X5 F50',   # Very slow movement, larger distance
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
            Write-Host "  [Waiting 5000ms for slow motion...]" -ForegroundColor Gray
            Start-Sleep -Milliseconds 5000
            
            # Get status after motion
            Write-Host "  [Checking status after motion...]" -ForegroundColor Gray
            $serial.WriteLine('?')
            Start-Sleep -Milliseconds 100
            try {
                $status = $serial.ReadLine().Trim()
                Write-Host "  Status: $status" -ForegroundColor Cyan
            }
            catch {
                Write-Host "  No status response" -ForegroundColor Red
            }
            
            # Get debug info
            $serial.WriteLine('DEBUG')
            Start-Sleep -Milliseconds 100
            try {
                $debug = $serial.ReadLine().Trim()
                Write-Host "  Debug: $debug" -ForegroundColor Cyan
            }
            catch {
                Write-Host "  No debug response" -ForegroundColor Red
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

Write-Host "`nTest completed. Look for any step count changes!" -ForegroundColor Cyan