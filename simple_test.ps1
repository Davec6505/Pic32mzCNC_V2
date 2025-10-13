#!/usr/bin/env pwsh#!/usr/bin/env pwsh

param(

# Simple test script to send one G-code command and see the debug output    [string]$Port = "COM4"

$PORT = "COM4")

$BAUD_RATE = 115200

Write-Host "=== Buffer Direct Test ==="

try {Write-Host "Testing buffer add/get without motion planner..."

    Write-Host "Opening $PORT at $BAUD_RATE baud..."

    $serialPort = New-Object System.IO.Ports.SerialPort# Connect to COM port

    $serialPort.PortName = $PORT$serialPort = New-Object System.IO.Ports.SerialPort

    $serialPort.BaudRate = $BAUD_RATE$serialPort.PortName = $Port

    $serialPort.Open()$serialPort.BaudRate = 115200

    Write-Host "Connected! Sending one G-code command..."$serialPort.Parity = [System.IO.Ports.Parity]::None

$serialPort.DataBits = 8

    # Wait for system to initialize$serialPort.StopBits = [System.IO.Ports.StopBits]::One

    Start-Sleep -Seconds 2$serialPort.Handshake = [System.IO.Ports.Handshake]::None

    

    # Send status query firsttry {

    Write-Host "`nSending initial status..."    Write-Host "Opening $Port..."

    $serialPort.WriteLine("?")    $serialPort.Open()

    Start-Sleep -Milliseconds 500    Write-Host "Connected!"

        

    # Read any buffered data    # Send test command to bypass motion planner complexity

    while ($serialPort.BytesToRead -gt 0) {    Write-Host "`nSending simple test command..."

        $data = $serialPort.ReadExisting()    $serialPort.WriteLine("DEBUG")

        Write-Host $data -NoNewline    Start-Sleep -Milliseconds 1000

    }    

        # Read all available data

    # Send one G0 command    $response = ""

    Write-Host "`nSending G0 X10..."    while ($serialPort.BytesToRead -gt 0) {

    $serialPort.WriteLine("G0 X10")        $response += $serialPort.ReadExisting()

            Start-Sleep -Milliseconds 10

    # Wait and read response    }

    Start-Sleep -Milliseconds 1000    

    while ($serialPort.BytesToRead -gt 0) {    Write-Host "Response:"

        $data = $serialPort.ReadExisting()    Write-Host $response

        Write-Host $data -NoNewline    

    }    $serialPort.Close()

        Write-Host "`nSerial port closed."

    # Send status query to see if position changed    

    Write-Host "`nSending status query..."} catch {

    $serialPort.WriteLine("?")    Write-Host "Error: $_"

    Start-Sleep -Milliseconds 500    if ($serialPort.IsOpen) {

            $serialPort.Close()

    while ($serialPort.BytesToRead -gt 0) {    }

        $data = $serialPort.ReadExisting()}
        Write-Host $data -NoNewline
    }
    
    Write-Host "`nTest complete!"
    
} catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
} finally {
    if ($serialPort -and $serialPort.IsOpen) {
        Write-Host "Serial port closed."
        $serialPort.Close()
    }
}