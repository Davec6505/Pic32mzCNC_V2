# GRBL Command Test Script
$port = New-Object System.IO.Ports.SerialPort "COM4", 115200
$port.Open()

Write-Host "Testing comprehensive GRBL commands..."

# Test settings
Write-Host "`nTesting $$ (settings):"
$port.WriteLine('$$')
Start-Sleep -Milliseconds 100
while ($port.BytesToRead -gt 0) {
    Write-Host $port.ReadLine()
}

# Test version info
Write-Host "`nTesting `$I (version):"
$port.WriteLine('$I')
Start-Sleep -Milliseconds 100
while ($port.BytesToRead -gt 0) {
    Write-Host $port.ReadLine()
}

# Test parser state
Write-Host "`nTesting `$G (parser state):"
$port.WriteLine('$G')
Start-Sleep -Milliseconds 100
while ($port.BytesToRead -gt 0) {
    Write-Host $port.ReadLine()
}

# Test parameters
Write-Host "`nTesting `$# (parameters):"
$port.WriteLine('$#')
Start-Sleep -Milliseconds 100
while ($port.BytesToRead -gt 0) {
    Write-Host $port.ReadLine()
}

# Test G-code commands
Write-Host "`nTesting G0 (rapid move):"
$port.WriteLine('G0 X10 Y10')
Start-Sleep -Milliseconds 100
while ($port.BytesToRead -gt 0) {
    Write-Host $port.ReadLine()
}

Write-Host "`nTesting M3 (spindle on):"
$port.WriteLine('M3 S1000')
Start-Sleep -Milliseconds 100
while ($port.BytesToRead -gt 0) {
    Write-Host $port.ReadLine()
}

Write-Host "`nTesting M5 (spindle off):"
$port.WriteLine('M5')
Start-Sleep -Milliseconds 100
while ($port.BytesToRead -gt 0) {
    Write-Host $port.ReadLine()
}

# Test status report
Write-Host "`nTesting ? (status report):"
$port.Write('?')
Start-Sleep -Milliseconds 100
while ($port.BytesToRead -gt 0) {
    Write-Host $port.ReadLine()
}

$port.Close()
Write-Host "`nTest complete!"