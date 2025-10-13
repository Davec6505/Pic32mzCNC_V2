# Send simple characters and monitor response
$port = new-object System.IO.Ports.SerialPort COM4,115200,None,8,one
$port.open()

# Send simple status query that should get immediate response
Write-Host "Sending status query..."
$port.WriteLine("?")
Start-Sleep -Milliseconds 100

# Send soft reset which should respond immediately  
Write-Host "Sending soft reset..."
$port.WriteLine("$`r`n")
Start-Sleep -Milliseconds 100

# Monitor output for a few seconds
Write-Host "Monitoring output..."
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