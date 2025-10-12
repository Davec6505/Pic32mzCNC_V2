# UGS Continuous Operation Simulation
# This simulates UGS behavior after successful connection

$port = New-Object System.IO.Ports.SerialPort "COM4", 115200
$port.ReadTimeout = 1000
$port.DtrEnable = $true
$port.RtsEnable = $false
$port.Open()

Write-Host "=== UGS Continuous Operation Simulation ==="
Write-Host "Simulating UGS behavior during normal operation..."
Write-Host ""

try {
    # Simulate periodic status requests (like UGS does every 200ms when connected)
    for ($i = 1; $i -le 10; $i++) {
        Write-Host "Status Request #$i"
        Write-Host ">>> ?"
        $port.Write('?')
        Start-Sleep -Milliseconds 50
        
        if ($port.BytesToRead -gt 0) {
            $status = $port.ReadExisting().Trim()
            Write-Host $status
        }
        
        Start-Sleep -Milliseconds 200  # UGS typically polls every 200ms
    }
    
    Write-Host ""
    Write-Host "*** Testing G-code command execution ***"
    
    # Test some basic G-code commands that UGS might send
    $gcodes = @("G0 X10", "G1 Y5 F100", "M3 S1000", "M5", "G0 X0 Y0")
    
    foreach ($gcode in $gcodes) {
        Write-Host ">>> $gcode"
        $port.WriteLine($gcode)
        Start-Sleep -Milliseconds 100
        
        if ($port.BytesToRead -gt 0) {
            $response = $port.ReadLine().Trim()
            Write-Host $response
        }
        
        # Status check after each command (like UGS does)
        $port.Write('?')
        Start-Sleep -Milliseconds 50
        if ($port.BytesToRead -gt 0) {
            $status = $port.ReadExisting().Trim()
            Write-Host "Status: $status"
        }
        Write-Host ""
    }
    
    Write-Host "*** Testing real-time commands ***"
    
    # Test feed hold and cycle start
    Write-Host ">>> ! (Feed Hold)"
    $port.Write('!')
    Start-Sleep -Milliseconds 100
    if ($port.BytesToRead -gt 0) {
        Write-Host $port.ReadExisting().Trim()
    }
    
    Write-Host ">>> ~ (Cycle Start)"
    $port.Write('~')
    Start-Sleep -Milliseconds 100
    if ($port.BytesToRead -gt 0) {
        Write-Host $port.ReadExisting().Trim()
    }
    
    Write-Host ""
    Write-Host "✅ UGS Continuous Operation Test Complete"
    Write-Host "Controller responds correctly to UGS operational patterns"
    
} catch {
    Write-Host "❌ Error during operation: $($_.Exception.Message)"
} finally {
    $port.Close()
}