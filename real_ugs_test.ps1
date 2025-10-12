# Real UGS Handshake Simulation Script
# This script mimics exactly how Universal G-code Sender v2.x behaves during connection

$port = New-Object System.IO.Ports.SerialPort "COM4", 115200
$port.ReadTimeout = 2000
$port.WriteTimeout = 1000
$port.DtrEnable = $true
$port.RtsEnable = $false
$port.Parity = "None"
$port.DataBits = 8
$port.StopBits = "One"
$port.Handshake = "None"

Write-Host "=== Real UGS Connection Simulation ==="
Write-Host "*** Opening COM4 at 115200 baud (8N1, DTR=true, RTS=false)"

try {
    $port.Open()
    
    # UGS Step 1: Initial reset and wait for startup
    Write-Host ""
    Write-Host "*** UGS Step 1: Soft Reset (Ctrl+X)"
    $port.Write([char]24)  # Ctrl+X soft reset
    Start-Sleep -Milliseconds 1500  # UGS waits for startup message
    
    # Clear any startup messages
    $buffer = ""
    while ($port.BytesToRead -gt 0) {
        $buffer += $port.ReadExisting()
    }
    if ($buffer.Length -gt 0) {
        Write-Host "Startup message: $($buffer.Trim())"
    }
    
    # UGS Step 2: Status report to verify connection
    Write-Host ""
    Write-Host "*** UGS Step 2: Status Report"
    Write-Host ">>> ?"
    $port.Write('?')
    Start-Sleep -Milliseconds 100
    
    $statusResponse = ""
    $timeout = 0
    while ($port.BytesToRead -eq 0 -and $timeout -lt 10) {
        Start-Sleep -Milliseconds 50
        $timeout++
    }
    
    if ($port.BytesToRead -gt 0) {
        $statusResponse = $port.ReadExisting().Trim()
        Write-Host $statusResponse
        
        if ($statusResponse -match "<.*>") {
            Write-Host "✅ Status report received - connection verified"
        } else {
            Write-Host "❌ Invalid status report format"
        }
    } else {
        Write-Host "❌ No status response"
    }
    
    # UGS Step 3: Get firmware version and capabilities
    Write-Host ""
    Write-Host "*** UGS Step 3: Firmware Version"
    Write-Host ">>> `$I"
    $port.WriteLine('$I')
    Start-Sleep -Milliseconds 200
    
    $versionLines = @()
    $timeout = 0
    while ($timeout -lt 20) {
        if ($port.BytesToRead -gt 0) {
            try {
                $line = $port.ReadLine().Trim()
                $versionLines += $line
                Write-Host $line
                if ($line -eq "ok") { break }
            } catch {
                break
            }
        }
        Start-Sleep -Milliseconds 50
        $timeout++
    }
    
    if ($versionLines -contains "ok") {
        Write-Host "✅ Version info received successfully"
    } else {
        Write-Host "❌ Version info incomplete"
    }
    
    # UGS Step 4: Get all GRBL settings
    Write-Host ""
    Write-Host "*** UGS Step 4: GRBL Settings"
    Write-Host ">>> `$`$"
    $port.WriteLine('$$')
    Start-Sleep -Milliseconds 100
    
    $settingsLines = @()
    $timeout = 0
    while ($timeout -lt 50) {  # Longer timeout for settings
        if ($port.BytesToRead -gt 0) {
            try {
                $line = $port.ReadLine().Trim()
                $settingsLines += $line
                Write-Host $line
                if ($line -eq "ok") { break }
            } catch {
                break
            }
        }
        Start-Sleep -Milliseconds 50
        $timeout++
    }
    
    $settingsCount = ($settingsLines | Where-Object { $_ -match '^\$\d+=' }).Count
    if ($settingsLines -contains "ok" -and $settingsCount -gt 0) {
        Write-Host "✅ Settings received successfully ($settingsCount settings)"
    } else {
        Write-Host "❌ Settings incomplete (got $settingsCount settings)"
    }
    
    # UGS Step 5: Get parser state (modal groups)
    Write-Host ""
    Write-Host "*** UGS Step 5: Parser State"
    Write-Host ">>> `$G"
    $port.WriteLine('$G')
    Start-Sleep -Milliseconds 200
    
    $parserLines = @()
    $timeout = 0
    while ($timeout -lt 20) {
        if ($port.BytesToRead -gt 0) {
            try {
                $line = $port.ReadLine().Trim()
                $parserLines += $line
                Write-Host $line
                if ($line -eq "ok") { break }
            } catch {
                break
            }
        }
        Start-Sleep -Milliseconds 50
        $timeout++
    }
    
    if ($parserLines -contains "ok") {
        Write-Host "✅ Parser state received successfully"
    } else {
        Write-Host "❌ Parser state incomplete"
    }
    
    # UGS Step 6: Final status check
    Write-Host ""
    Write-Host "*** UGS Step 6: Final Status Check"
    Write-Host ">>> ?"
    $port.Write('?')
    Start-Sleep -Milliseconds 100
    
    $finalStatus = ""
    $timeout = 0
    while ($port.BytesToRead -eq 0 -and $timeout -lt 10) {
        Start-Sleep -Milliseconds 50
        $timeout++
    }
    
    if ($port.BytesToRead -gt 0) {
        $finalStatus = $port.ReadExisting().Trim()
        Write-Host $finalStatus
        Write-Host "✅ Final status check complete"
    }
    
    Write-Host ""
    Write-Host "*** UGS Connection Complete ***"
    Write-Host "Connection would be established and ready for G-code streaming"
    
} catch {
    Write-Host "❌ Connection error: $($_.Exception.Message)"
} finally {
    if ($port.IsOpen) {
        $port.Close()
    }
}