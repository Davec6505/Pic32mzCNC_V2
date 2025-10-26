# PowerShell script to test circle motion with detailed debug output
# Tests the junction velocity bug by sending circle commands one at a time

param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200
)

Write-Host "=== Circle Junction Debug Test ===" -ForegroundColor Cyan
Write-Host "Port: $Port @ $BaudRate baud" -ForegroundColor Cyan
Write-Host ""

# Open serial port
$serialPort = New-Object System.IO.Ports.SerialPort
$serialPort.PortName = $Port
$serialPort.BaudRate = $BaudRate
$serialPort.DataBits = 8
$serialPort.Parity = [System.IO.Ports.Parity]::None
$serialPort.StopBits = [System.IO.Ports.StopBits]::One
$serialPort.Handshake = [System.IO.Ports.Handshake]::None
$serialPort.ReadTimeout = 5000
$serialPort.WriteTimeout = 1000

try {
    $serialPort.Open()
    Write-Host "Serial port opened successfully" -ForegroundColor Green
    
    # Wait for startup message
    Start-Sleep -Milliseconds 500
    
    # Read any startup messages
    while ($serialPort.BytesToRead -gt 0) {
        $line = $serialPort.ReadLine()
        Write-Host "RX: $line" -ForegroundColor Gray
    }
    
    # Send a command and wait for response
    function Send-Command {
        param([string]$cmd)
        
        Write-Host "`nTX: $cmd" -ForegroundColor Yellow
        $serialPort.WriteLine($cmd)
        
        # Wait for response (multiple lines possible)
        $timeout = 100  # 10 second timeout (100 x 100ms)
        $responses = @()
        
        for ($i = 0; $i -lt $timeout; $i++) {
            Start-Sleep -Milliseconds 100
            
            while ($serialPort.BytesToRead -gt 0) {
                $line = $serialPort.ReadLine()
                Write-Host "RX: $line" -ForegroundColor Cyan
                $responses += $line
                
                # Check for "ok" or "error"
                if ($line -match "^ok" -or $line -match "^error") {
                    return $responses
                }
            }
        }
        
        Write-Host "TIMEOUT: No response received!" -ForegroundColor Red
        return $responses
    }
    
    # Test sequence
    Write-Host "`n=== Sending Setup Commands ===" -ForegroundColor Magenta
    
    $responses = Send-Command "?"
    $responses = Send-Command "`$I"
    $responses = Send-Command "G21"  # mm mode
    $responses = Send-Command "G90"  # absolute mode
    $responses = Send-Command "G17"  # XY plane
    
    Write-Host "`n=== Moving to Start Position ===" -ForegroundColor Magenta
    
    $responses = Send-Command "G0 Z5"     # Raise Z
    $responses = Send-Command "G0 X10 Y0" # Move to start
    $responses = Send-Command "G0 Z0"     # Lower Z
    
    Write-Host "`n=== Sending Circle Commands (First 5) ===" -ForegroundColor Magenta
    
    $circle_commands = @(
        "G1 X9.511 Y3.090 F1000",
        "G1 X8.090 Y5.878",
        "G1 X5.878 Y8.090",
        "G1 X3.090 Y9.511",
        "G1 X0.000 Y10.000"
    )
    
    for ($i = 0; $i -lt $circle_commands.Length; $i++) {
        Write-Host "`n--- Command $($i+1) of $($circle_commands.Length) ---" -ForegroundColor White
        $responses = Send-Command $circle_commands[$i]
        
        # Check machine status after each command
        Start-Sleep -Milliseconds 500
        $serialPort.WriteLine("?")
        Start-Sleep -Milliseconds 100
        while ($serialPort.BytesToRead -gt 0) {
            $line = $serialPort.ReadLine()
            if ($line -match "<.*>") {
                Write-Host "STATUS: $line" -ForegroundColor Green
            }
        }
        
        # Wait a bit before next command
        Start-Sleep -Milliseconds 200
    }
    
    Write-Host "`n=== Test Complete ===" -ForegroundColor Magenta
    Write-Host "Check above output for:" -ForegroundColor Yellow
    Write-Host "  1. [CMD_ADD] messages showing commands added to buffer" -ForegroundColor Yellow
    Write-Host "  2. [PROCBUF] messages showing commands being processed" -ForegroundColor Yellow
    Write-Host "  3. [LOOP] messages showing buffer counts" -ForegroundColor Yellow
    Write-Host "  4. Which command causes the stall (likely command 4 or 5)" -ForegroundColor Yellow
    
} catch {
    Write-Host "Error: $_" -ForegroundColor Red
} finally {
    if ($serialPort.IsOpen) {
        $serialPort.Close()
        Write-Host "`nSerial port closed" -ForegroundColor Green
    }
}
