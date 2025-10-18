# Simple Serial Test - Send individual commands with verification
param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200
)

# Open serial port
$serialPort = New-Object System.IO.Ports.SerialPort
$serialPort.PortName = $Port
$serialPort.BaudRate = $BaudRate
$serialPort.DataBits = 8
$serialPort.Parity = "None"
$serialPort.StopBits = "One"
$serialPort.ReadTimeout = 1000
$serialPort.WriteTimeout = 1000

Write-Host "Connecting to $Port @ $BaudRate baud..." -ForegroundColor Cyan
$serialPort.Open()
Write-Host "✓ Connected!" -ForegroundColor Green

Start-Sleep -Milliseconds 500

function Send-Command {
    param([string]$cmd)
    
    Write-Host "`n>> Sending: '$cmd'" -ForegroundColor Yellow
    $serialPort.WriteLine($cmd)
    
    Start-Sleep -Milliseconds 500
    
    # Read all available responses
    while ($serialPort.BytesToRead -gt 0) {
        try {
            $response = $serialPort.ReadLine()
            Write-Host "<< $response" -ForegroundColor Gray
        }
        catch {
            break
        }
    }
}

# Test 1: Simple status query
Write-Host "`n=== TEST 1: Status Query ===" -ForegroundColor Cyan
$serialPort.Write("?")
Start-Sleep -Milliseconds 500
while ($serialPort.BytesToRead -gt 0) {
    try {
        $line = $serialPort.ReadLine()
        Write-Host "<< $line" -ForegroundColor Gray
    }
    catch { break }
}

# Test 2: Simple G-code with spaces
Write-Host "`n=== TEST 2: G92 X0 Y0 (with spaces) ===" -ForegroundColor Cyan
Send-Command "G92 X0 Y0"

# Test 3: G92.1 (no spaces)
Write-Host "`n=== TEST 3: G92.1 (no spaces) ===" -ForegroundColor Cyan
Send-Command "G92.1"

# Test 4: G90 (simple)
Write-Host "`n=== TEST 4: G90 ===" -ForegroundColor Cyan
Send-Command "G90"

# Test 5: Check what bytes are actually sent
Write-Host "`n=== TEST 5: Byte-by-byte analysis ===" -ForegroundColor Cyan
$testString = "G92 X0 Y0"
Write-Host "String: '$testString'" -ForegroundColor Yellow
Write-Host "Bytes: " -NoNewline
[System.Text.Encoding]::ASCII.GetBytes($testString) | ForEach-Object {
    Write-Host "$_ " -NoNewline -ForegroundColor Cyan
}
Write-Host ""
Write-Host "Chars: " -NoNewline
[System.Text.Encoding]::ASCII.GetBytes($testString) | ForEach-Object {
    Write-Host "[$([ char]$_)] " -NoNewline -ForegroundColor Green
}
Write-Host "`n"

$serialPort.Close()
Write-Host "Serial port closed." -ForegroundColor Gray
