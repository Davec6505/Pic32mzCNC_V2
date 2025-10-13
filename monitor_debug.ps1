# Monitor output for longer period to catch debug messages
$port = new-object System.IO.Ports.SerialPort COM4,115200,None,8,one
$port.open()

Write-Host "Monitoring output for 10 seconds to catch debug messages..."
$timeout = (Get-Date).AddSeconds(10)
while ((Get-Date) -lt $timeout) {
    if ($port.BytesToRead -gt 0) {
        $data = $port.ReadExisting()
        Write-Host $data -NoNewline
    }
    Start-Sleep -Milliseconds 50
}

$port.close()
Write-Host "`nMonitoring complete."