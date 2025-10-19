# Test square pattern with full debug output
# This script sends the square test pattern and captures all debug output

param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200
)

# Open serial port
$port_obj = New-Object System.IO.Ports.SerialPort
$port_obj.PortName = $Port
$port_obj.BaudRate = $BaudRate
$port_obj.Parity = [System.IO.Ports.Parity]::None
$port_obj.DataBits = 8
$port_obj.StopBits = [System.IO.Ports.StopBits]::One
$port_obj.ReadTimeout = 1000
$port_obj.WriteTimeout = 1000

try {
    $port_obj.Open()
    Write-Host "Opened $Port @ $BaudRate baud" -ForegroundColor Green
    
    # Function to send command and wait for response
    function Send-Command {
        param([string]$cmd, [int]$timeoutMs = 2000)
        
        Write-Host "`n>>> $cmd" -ForegroundColor Cyan
        $port_obj.WriteLine($cmd)
        
        $start_time = Get-Date
        $ok_received = $false
        
        while (((Get-Date) - $start_time).TotalMilliseconds -lt $timeoutMs) {
            if ($port_obj.BytesToRead -gt 0) {
                $line = $port_obj.ReadLine().Trim()
                if ($line.Length -gt 0) {
                    # Color code output
                    if ($line -match "^\[MODAL\]") {
                        Write-Host $line -ForegroundColor Yellow
                    }
                    elseif ($line -match "^\[GRBL\]") {
                        Write-Host $line -ForegroundColor Magenta
                    }
                    elseif ($line -match "^<") {
                        Write-Host $line -ForegroundColor Gray
                    }
                    elseif ($line -match "^ok") {
                        Write-Host $line -ForegroundColor Green
                        $ok_received = $true
                    }
                    else {
                        Write-Host $line
                    }
                }
            }
            Start-Sleep -Milliseconds 10
        }
        
        if (-not $ok_received) {
            Write-Host "WARNING: No 'ok' received for command: $cmd" -ForegroundColor Red
        }
        
        # Brief pause between commands
        Start-Sleep -Milliseconds 100
    }
    
    # Wait for startup messages
    Write-Host "`nWaiting for startup messages..." -ForegroundColor Yellow
    Start-Sleep -Seconds 2
    while ($port_obj.BytesToRead -gt 0) {
        $line = $port_obj.ReadLine()
        Write-Host $line -ForegroundColor DarkGray
    }
    
    # Send initialization commands
    Write-Host "`n=== INITIALIZATION ===" -ForegroundColor Cyan
    Send-Command "G10 P0 L20 X0 Y0 Z0"  # Reset work coordinates
    Start-Sleep -Milliseconds 500
    
    Send-Command "G0 Z0"                  # Ensure Z at zero
    Start-Sleep -Milliseconds 500
    
    # Send square pattern
    Write-Host "`n=== SQUARE PATTERN TEST ===" -ForegroundColor Cyan
    
    Send-Command "G1 Y10 F1000"
    Start-Sleep -Seconds 2  # Wait for motion to complete
    
    Send-Command "G1 X10"
    Start-Sleep -Seconds 2
    
    Send-Command "G1 Y0"
    Start-Sleep -Seconds 2
    
    Send-Command "G1 X0"
    Start-Sleep -Seconds 2
    
    # Final status check
    Write-Host "`n=== FINAL STATUS ===" -ForegroundColor Cyan
    $port_obj.WriteLine("?")
    Start-Sleep -Milliseconds 500
    while ($port_obj.BytesToRead -gt 0) {
        $line = $port_obj.ReadLine()
        if ($line -match "^<") {
            Write-Host $line -ForegroundColor Green
        }
        else {
            Write-Host $line
        }
    }
    
    Write-Host "`nTest complete!" -ForegroundColor Green
}
catch {
    Write-Host "Error: $_" -ForegroundColor Red
}
finally {
    if ($port_obj.IsOpen) {
        $port_obj.Close()
        Write-Host "`nPort closed" -ForegroundColor Yellow
    }
}
