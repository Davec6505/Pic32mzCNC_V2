# Test the new MikroC-style tokenizer with compound G-code commands
$port = "COM4"
$baudRate = 115200

# Open serial connection
try {
    $serialPort = New-Object System.IO.Ports.SerialPort
    $serialPort.PortName = $port
    $serialPort.BaudRate = $baudRate
    $serialPort.DataBits = 8
    $serialPort.Parity = "None"
    $serialPort.StopBits = "One"
    $serialPort.Handshake = "None"
    $serialPort.ReadTimeout = 2000
    $serialPort.WriteTimeout = 2000
    
    $serialPort.Open()
    Write-Host "Connected to $port at $baudRate baud" -ForegroundColor Green
    
    # Wait for system to initialize
    Start-Sleep -Milliseconds 500
    
    # Clear any pending data
    $serialPort.DiscardInBuffer()
    $serialPort.DiscardOutBuffer()
    
    Write-Host "`n=== Testing MikroC-Style G-code Tokenizer ===" -ForegroundColor Yellow
    
    # Test simple G-code commands
    $testCommands = @(
        "G0 X10 Y10",
        "G1 X20 Y20 F100",
        "G21G91X-1F99",  # Compound command without spaces
        "`$J=G21G91X1F100",  # UGS jogging command
        "`$J=G21G91Y-1F99",
        "G0X5Y5Z2",     # No spaces
        "M3 S1000",     # Spindle command
        "G90 G0 X0 Y0 Z0"  # Multiple commands with spaces
    )
    
    foreach ($cmd in $testCommands) {
        Write-Host "`nSending: '$cmd'" -ForegroundColor Cyan
        $serialPort.WriteLine($cmd)
        
        # Wait for response and collect debug output
        $startTime = Get-Date
        $response = ""
        
        while (((Get-Date) - $startTime).TotalMilliseconds -lt 3000) {
            if ($serialPort.BytesToRead -gt 0) {
                $data = $serialPort.ReadExisting()
                $response += $data
                Write-Host $data -NoNewline -ForegroundColor White
                
                # Check if we got "ok" or "error" response
                if ($response -match "(ok|error)" -and $response -match "\r?\n") {
                    break
                }
            }
            Start-Sleep -Milliseconds 50
        }
        
        if ($response -eq "") {
            Write-Host "No response received!" -ForegroundColor Red
        }
        
        Start-Sleep -Milliseconds 200
    }
    
    Write-Host "`n`n=== Tokenizer Test Complete ===" -ForegroundColor Yellow
    
} catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
} finally {
    if ($serialPort -and $serialPort.IsOpen) {
        $serialPort.Close()
        Write-Host "`nSerial connection closed." -ForegroundColor Green
    }
}