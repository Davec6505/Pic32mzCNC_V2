#!/usr/bin/env pwsh
# Simple startup test to see initialization messages

$port = "COM4"
$baudRate = 115200

Write-Host "Opening $port at $baudRate baud..."
$serialPort = New-Object System.IO.Ports.SerialPort $port, $baudRate

try {
    $serialPort.Open()
    Write-Host "Connected! Waiting for startup messages..."
    
    # Wait for startup messages (first 3 seconds)
    $startTime = Get-Date
    while ((Get-Date) - $startTime -lt [TimeSpan]::FromSeconds(3)) {
        if ($serialPort.BytesToRead -gt 0) {
            $response = $serialPort.ReadLine().Trim()
            Write-Host "STARTUP: $response"
        }
        Start-Sleep -Milliseconds 10
    }
    
    # Send a simple status request
    Write-Host "`nSending status request..."
    $serialPort.WriteLine("?")
    Start-Sleep -Milliseconds 100
    
    if ($serialPort.BytesToRead -gt 0) {
        $response = $serialPort.ReadLine().Trim()
        Write-Host "STATUS: $response"
    }
    
} catch {
    Write-Host "Error: $_"
} finally {
    if ($serialPort.IsOpen) {
        $serialPort.Close()
        Write-Host "Serial port closed."
    }
}