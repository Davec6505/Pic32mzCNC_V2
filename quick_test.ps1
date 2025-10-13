# Quick test to see if LED is blinking and motion planner is working
$port = new-object System.IO.Ports.SerialPort COM4,115200,None,8,one
$port.open()

Write-Host "Monitoring for 5 seconds to see if system is alive..."
$timeout = (Get-Date).AddSeconds(5)
while ((Get-Date) -lt $timeout) {
    if ($port.BytesToRead -gt 0) {
        $data = $port.ReadExisting()
        Write-Host $data -NoNewline
    }
    Start-Sleep -Milliseconds 100
}

$port.close()
Write-Host "`nTest complete."