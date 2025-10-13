# Test script to check intermediate position reporting during a long move
param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200
)

try {
    # Open serial port
    Write-Host "Opening $Port at $BaudRate baud..."
    $port = New-Object System.IO.Ports.SerialPort($Port, $BaudRate)
    $port.Parity = [System.IO.Ports.Parity]::None
    $port.DataBits = 8
    $port.StopBits = [System.IO.Ports.StopBits]::One
    $port.Handshake = [System.IO.Ports.Handshake]::None
    $port.DtrEnable = $true
    $port.RtsEnable = $true
    $port.ReadTimeout = 1000
    $port.WriteTimeout = 1000

    $port.Open()
    Write-Host "Connected! Testing long move..."

    # Wait for system startup
    Start-Sleep -Milliseconds 2000

    # Send a long, slow move
    Write-Host "Sending long move: G0 X100 F50"
    $port.WriteLine("G0 X100 F50")
    Start-Sleep -Milliseconds 100

    # Read response
    $response = $port.ReadExisting()
    Write-Host $response

    # Query status repeatedly during the move
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Milliseconds 200
        $port.WriteLine("?")
        Start-Sleep -Milliseconds 50
        $status = $port.ReadExisting()
        Write-Host "Status $i : $status"
    }

} catch {
    Write-Host "Error: $($_.Exception.Message)"
} finally {
    if ($port -and $port.IsOpen) {
        $port.Close()
        Write-Host "Serial port closed."
    }
}