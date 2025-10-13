# Test status query command
$port = new-object System.IO.Ports.SerialPort COM4,115200,None,8,one
$port.open()

Write-Host "Sending status query '?' command..."
$port.WriteLine("?")
Start-Sleep -Milliseconds 200

Write-Host "Monitoring output for 3 seconds..."
$timeout = (Get-Date).AddSeconds(3)
while ((Get-Date) -lt $timeout) {
    if ($port.BytesToRead -gt 0) {
        $data = $port.ReadExisting()
        Write-Host $data -NoNewline
    }
    Start-Sleep -Milliseconds 50
}

$port.close()
Write-Host "`nTest complete."