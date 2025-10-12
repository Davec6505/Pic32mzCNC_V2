# UGS G-code File Sender
# Sends G-code file line by line to test modular motion control
param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200,
    [string]$GCodeFile = "modular_test.gcode"
)

function Send-GCodeLine {
    param(
        [System.IO.Ports.SerialPort]$Port,
        [string]$Line,
        [int]$TimeoutMs = 5000
    )
    
    # Skip empty lines and comments
    $Line = $Line.Trim()
    if ($Line -eq "" -or $Line.StartsWith(";")) {
        return $true
    }
    
    Write-Host "Sending: $Line" -ForegroundColor Yellow
    $Port.Write("$Line`r`n")
    
    $startTime = Get-Date
    $response = ""
    
    # Wait for response
    while (((Get-Date) - $startTime).TotalMilliseconds -lt $TimeoutMs) {
        if ($Port.BytesToRead -gt 0) {
            $data = $Port.ReadExisting()
            $response += $data
            Write-Host $data -NoNewline -ForegroundColor Cyan
            
            # Check if we got a complete response
            if ($response -match "ok" -or $response -match "error") {
                Write-Host "" # New line
                return $response -match "ok"
            }
        }
        Start-Sleep -Milliseconds 10
    }
    
    Write-Host " [TIMEOUT]" -ForegroundColor Red
    return $false
}

try {
    if (-not (Test-Path $GCodeFile)) {
        Write-Host "G-code file not found: $GCodeFile" -ForegroundColor Red
        exit 1
    }
    
    # Load .NET serial port class
    Add-Type -AssemblyName System.IO.Ports

    # Create and configure serial port
    $serialPort = New-Object System.IO.Ports.SerialPort
    $serialPort.PortName = $Port
    $serialPort.BaudRate = $BaudRate
    $serialPort.DataBits = 8
    $serialPort.Parity = [System.IO.Ports.Parity]::None
    $serialPort.StopBits = [System.IO.Ports.StopBits]::One
    $serialPort.ReadTimeout = 1000
    $serialPort.WriteTimeout = 1000

    Write-Host "=== UGS Modular Motion Control Test ===" -ForegroundColor Green
    Write-Host "Opening $Port at $BaudRate baud..." -ForegroundColor Green
    
    $serialPort.Open()
    
    if ($serialPort.IsOpen) {
        Write-Host "Connected! Loading G-code file: $GCodeFile" -ForegroundColor Green
        Start-Sleep -Seconds 1
        
        # Read G-code file
        $gCodeLines = Get-Content $GCodeFile
        $totalLines = ($gCodeLines | Where-Object { $_.Trim() -ne "" -and -not $_.Trim().StartsWith(";") }).Count
        $currentLine = 0
        $successCount = 0
        
        Write-Host "Starting G-code execution ($totalLines commands)..." -ForegroundColor Magenta
        Write-Host "Press Ctrl+C to stop" -ForegroundColor Yellow
        
        foreach ($line in $gCodeLines) {
            $line = $line.Trim()
            
            # Skip empty lines and comments
            if ($line -eq "" -or $line.StartsWith(";")) {
                if ($line.StartsWith(";")) {
                    Write-Host $line -ForegroundColor Gray
                }
                continue
            }
            
            $currentLine++
            Write-Host "[$currentLine/$totalLines] " -NoNewline -ForegroundColor White
            
            $success = Send-GCodeLine -Port $serialPort -Line $line
            if ($success) {
                $successCount++
            } else {
                Write-Host "Failed to execute: $line" -ForegroundColor Red
                $response = Read-Host "Continue? (y/n)"
                if ($response -ne "y") {
                    break
                }
            }
            
            # Small delay between commands
            Start-Sleep -Milliseconds 100
        }
        
        Write-Host "`n=== Execution Summary ===" -ForegroundColor Green
        Write-Host "Total commands: $totalLines" -ForegroundColor White
        Write-Host "Successful: $successCount" -ForegroundColor Green
        Write-Host "Failed: $($totalLines - $successCount)" -ForegroundColor Red
        
        if ($successCount -eq $totalLines) {
            Write-Host "✅ All commands executed successfully!" -ForegroundColor Green
            Write-Host "The modular motion control system is fully functional." -ForegroundColor Green
        } else {
            Write-Host "⚠️ Some commands failed. Check the motion control modules." -ForegroundColor Yellow
        }
    }
}
catch {
    Write-Host "Error: $_" -ForegroundColor Red
}
finally {
    if ($serialPort -and $serialPort.IsOpen) {
        $serialPort.Close()
        Write-Host "`nSerial port closed." -ForegroundColor Green
    }
}