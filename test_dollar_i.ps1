# Test specifically for $I command
$port = New-Object System.IO.Ports.SerialPort "COM4", 115200
$port.Open()

Write-Host "Testing `$I command specifically..."

# Test version info multiple times
for ($i = 1; $i -le 3; $i++) {
    Write-Host "`nTest $i - Sending `$I:"
    $port.WriteLine('$I')
    Start-Sleep -Milliseconds 200
    $response = ""
    while ($port.BytesToRead -gt 0) {
        $line = $port.ReadLine()
        $response += $line + "`r`n"
        Write-Host $line
    }
    
    # Verify completeness
    if ($response -match "VER:1\.1f\.20241012:CNC Controller" -and $response -match "OPT:V,15,128" -and $response -match "ok") {
        Write-Host "✅ Test $i PASSED - Complete response received"
    } else {
        Write-Host "❌ Test $i FAILED - Incomplete response"
        Write-Host "Response was: $response"
    }
}

$port.Close()
Write-Host "`nTest complete!"