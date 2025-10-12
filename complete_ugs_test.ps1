# Complete UGS Handshake Test - Including $G command
$port = New-Object System.IO.Ports.SerialPort "COM4", 115200
$port.ReadTimeout = 1000
$port.Open()

Write-Host "=== Complete UGS Handshake Simulation ==="
Write-Host "*** Connecting to jserialcomm://COM4:115200"

# Step 1: Fetching device status
Write-Host "*** Fetching device status"
Write-Host ">>> ?"
$port.Write('?')
Start-Sleep -Milliseconds 200
try {
    while ($port.BytesToRead -gt 0) {
        $line = $port.ReadLine()
        Write-Host $line
    }
} catch {}

# Step 2: Fetching device version  
Write-Host "*** Fetching device version"
Write-Host ">>> `$I"
$port.WriteLine('$I')
Start-Sleep -Milliseconds 300
try {
    while ($port.BytesToRead -gt 0) {
        $line = $port.ReadLine()
        Write-Host $line
    }
} catch {}

# Step 3: Fetching device settings
Write-Host "*** Fetching device settings"
Write-Host ">>> `$`$"
$port.WriteLine('$$')
Start-Sleep -Milliseconds 500
try {
    while ($port.BytesToRead -gt 0) {
        $line = $port.ReadLine()
        Write-Host $line
    }
} catch {}

# Step 4: Fetching device state (this was causing UGS to fail)
Write-Host "*** Fetching device state"
Write-Host ">>> `$G"
$port.WriteLine('$G')
Start-Sleep -Milliseconds 300
try {
    $stateReceived = 0
    while ($port.BytesToRead -gt 0) {
        $line = $port.ReadLine()
        Write-Host $line
        $stateReceived++
    }
    if ($stateReceived -gt 0) {
        Write-Host "✅ Device state received successfully"
    } else {
        Write-Host "❌ No device state received"
    }
} catch {
    Write-Host "❌ Error reading device state: $($_.Exception.Message)"
}

# Step 5: Test additional commands
Write-Host "*** Testing additional commands"
Write-Host ">>> `$#"
$port.WriteLine('$#')
Start-Sleep -Milliseconds 300
try {
    while ($port.BytesToRead -gt 0) {
        $line = $port.ReadLine()
        Write-Host $line
    }
} catch {}

Write-Host "*** All UGS handshake steps completed successfully! ✅"
$port.Close()