#!/usr/bin/env pwsh
param(
    [string]$Port = "COM4"
)

Write-Host "=== Motion Buffer Test ==="
Write-Host "Looking for [BUFFER_ADD] and [MP_NEW_BLOCK] messages..."

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
    
    # Send a motion command
    Write-Host "`nSending motion command G1 X5 F10..."
    $serialPort.WriteLine("G1 X5 F10")
    Start-Sleep -Milliseconds 500
    
    # Read response
    $response = ""
    $timeout = 100
    while ($timeout -gt 0 -and $serialPort.BytesToRead -gt 0) {
        $response += $serialPort.ReadExisting()
        Start-Sleep -Milliseconds 10
        $timeout--
    }
    
    Write-Host "Response:"
    Write-Host $response
    
    # Check status
    Write-Host "`nChecking status..."
    $serialPort.WriteLine("DEBUG")
    Start-Sleep -Milliseconds 300
    
    $debug = ""
    $timeout = 100
    while ($timeout -gt 0 -and $serialPort.BytesToRead -gt 0) {
        $debug += $serialPort.ReadExisting()
        Start-Sleep -Milliseconds 10
        $timeout--
    }
    
    Write-Host "Debug:"
    Write-Host $debug
    
    $serialPort.Close()
    Write-Host "`nSerial port closed."
    
} catch {
    Write-Host "Error: $_"
    if ($serialPort.IsOpen) {
        $serialPort.Close()
    }
}