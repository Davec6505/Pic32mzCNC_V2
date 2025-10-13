# Send G-code and monitor response
$port = new-object System.IO.Ports.SerialPort COM4,115200,None,8,one
$port.open()

# Send some G-code commands
Write-Host "Sending G-code commands..."
$port.WriteLine("G01 X10 Y10 F100")
Start-Sleep -Milliseconds 100
$port.WriteLine("G01 X20 Y20 F200")
Start-Sleep -Milliseconds 100
$port.WriteLine("G01 X0 Y0 F100")
Start-Sleep -Milliseconds 100

# Monitor output for a few seconds
Write-Host "Monitoring output..."
$timeout = (Get-Date).AddSeconds(5)
while ((Get-Date) -lt $timeout) {
    if ($port.BytesToRead -gt 0) {
        $data = $port.ReadExisting()
        Write-Host $data -NoNewline
    }
    Start-Sleep -Milliseconds 50
}

$port.close()
Write-Host "`nTest complete."