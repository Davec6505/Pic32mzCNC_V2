# UGS Handshake Test Script - Mimics exactly what UGS does
$port = New-Object System.IO.Ports.SerialPort "COM4", 115200
$port.ReadTimeout = 1000
$port.Open()

Write-Host "=== UGS Handshake Simulation ==="
Write-Host "*** Connecting to jserialcomm://COM4:115200"

# Step 1: Fetching device status
Write-Host "*** Fetching device status"
Write-Host ">>> ?"
$port.Write('?')
Start-Sleep -Milliseconds 200
$response = ""
try {
    while ($port.BytesToRead -gt 0) {
        $line = $port.ReadLine()
        $response += $line
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

# Step 3: Fetching device settings (this is where UGS fails)
Write-Host "*** Fetching device settings"
Write-Host ">>> `$`$"
$port.WriteLine('$$')
Start-Sleep -Milliseconds 500
try {
    $settingsReceived = 0
    while ($port.BytesToRead -gt 0) {
        $line = $port.ReadLine()
        Write-Host $line
        $settingsReceived++
    }
    if ($settingsReceived -gt 0) {
        Write-Host "✅ Settings received successfully ($settingsReceived lines)"
    } else {
        Write-Host "❌ No settings received"
    }
} catch {
    Write-Host "❌ Error reading settings: $($_.Exception.Message)"
}

Write-Host "*** Connection test complete"
$port.Close()