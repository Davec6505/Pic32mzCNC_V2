#!/usr/bin/env pwsh
param(
    [string]$Port = "COM4"
)

Write-Host "=== Buffer Test ==="
Write-Host "Testing buffer add/get functionality..."

# Connect to COM port
$serialPort = New-Object System.IO.Ports.SerialPort
$serialPort.PortName = $Port
$serialPort.BaudRate = 115200
$serialPort.Parity = [System.IO.Ports.Parity]::None
$serialPort.DataBits = 8
$serialPort.StopBits = [System.IO.Ports.StopBits]::One
$serialPort.Handshake = [System.IO.Ports.Handshake]::None

try {
    Write-Host "Opening $Port..."
    $serialPort.Open()
    Write-Host "Connected!"
    
    # Send buffer test command
    Write-Host "`nSending BUFTEST command..."
    $serialPort.WriteLine("BUFTEST")
    Start-Sleep -Milliseconds 2000
    
    # Read all available data
    $response = ""
    while ($serialPort.BytesToRead -gt 0) {
        $response += $serialPort.ReadExisting()
        Start-Sleep -Milliseconds 10
    }
    
    Write-Host "Response:"
    Write-Host $response
    
    $serialPort.Close()
    Write-Host "`nSerial port closed."
    
} catch {
    Write-Host "Error: $_"
    if ($serialPort.IsOpen) {
        $serialPort.Close()
    }
}