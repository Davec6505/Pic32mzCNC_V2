# Simple test to see ALL debug output from a single move

param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200
)

$serialPort = New-Object System.IO.Ports.SerialPort
$serialPort.PortName = $Port
$serialPort.BaudRate = $BaudRate
$serialPort.DataBits = 8
$serialPort.Parity = [System.IO.Ports.Parity]::None
$serialPort.StopBits = [System.IO.Ports.StopBits]::One
$serialPort.ReadTimeout = 2000
$serialPort.WriteTimeout = 1000

try {
    $serialPort.Open()
    Write-Host "Connected to $Port" -ForegroundColor Green
    
    # Wait for startup
    Start-Sleep -Milliseconds 1000
    $startup = $serialPort.ReadExisting()
    Write-Host "Startup: $startup" -ForegroundColor Gray
    
    # Reset position
    Write-Host "`nSending G92 X0 Y0..." -ForegroundColor Cyan
    $serialPort.WriteLine("G92 X0 Y0")
    Start-Sleep -Milliseconds 500
    $response = $serialPort.ReadExisting()
    Write-Host $response -ForegroundColor Yellow
    
    # Send a short move
    Write-Host "`nSending G1 Y5 F1000..." -ForegroundColor Cyan
    $serialPort.WriteLine("G1 Y5 F1000")
    
    # Collect ALL output for 5 seconds
    $endTime = (Get-Date).AddSeconds(5)
    while ((Get-Date) -lt $endTime) {
        try {
            $line = $serialPort.ReadLine()
            Write-Host $line -ForegroundColor White
        } catch {
            Start-Sleep -Milliseconds 50
        }
    }
    
} finally {
    if ($serialPort.IsOpen) {
        $serialPort.Close()
    }
    Write-Host "`nDisconnected" -ForegroundColor Gray
}
