# Exact UGS Handshake Simulation
# This replicates the EXACT sequence and timing that UGS 2.x uses

$port = New-Object System.IO.Ports.SerialPort "COM4", 115200
$port.ReadTimeout = 2000
$port.WriteTimeout = 1000
$port.DtrEnable = $true
$port.RtsEnable = $false

function Wait-ForResponse {
    param($expectedPattern = ".*", $timeoutMs = 2000)
    
    $startTime = Get-Date
    $response = ""
    
    while (((Get-Date) - $startTime).TotalMilliseconds -lt $timeoutMs) {
        if ($port.BytesToRead -gt 0) {
            $char = $port.ReadChar()
            $response += [char]$char
            
            # Check if we have a complete response
            if ($response -match $expectedPattern) {
                return $response.Trim()
            }
        }
        Start-Sleep -Milliseconds 1
    }
    
    return $response.Trim()
}

function Send-CommandAndWait {
    param($command, $description, $expectedPattern = "ok", $timeoutMs = 2000)
    
    Write-Host ""
    Write-Host "*** $description"
    Write-Host ">>> $command"
    
    # Send command exactly as UGS does
    if ($command -eq '?') {
        $port.Write($command)  # Status queries are single character, no newline
    } else {
        $port.WriteLine($command)  # Other commands get newline
    }
    
    # Wait for complete response
    $response = Wait-ForResponse -expectedPattern $expectedPattern -timeoutMs $timeoutMs
    
    if ($response) {
        $lines = $response -split "`r`n|`r|`n" | Where-Object { $_.Trim() -ne "" }
        foreach ($line in $lines) {
            Write-Host $line
        }
        
        if ($response -match $expectedPattern) {
            Write-Host "✅ Response received successfully"
            return $true
        } else {
            Write-Host "❌ Unexpected response format"
            return $false
        }
    } else {
        Write-Host "❌ No response received"
        return $false
    }
}

Write-Host "=== Exact UGS Handshake Simulation ==="
Write-Host "*** Connecting to COM4:115200 (DTR=true, RTS=false)"

try {
    $port.Open()
    
    # UGS Step 1: Soft reset and wait for Grbl startup
    Write-Host ""
    Write-Host "*** Step 1: Soft Reset (Ctrl+X)"
    $port.Write([char]24)  # Send Ctrl+X
    
    # Wait for startup message (UGS expects "Grbl" in the response)
    $startup = Wait-ForResponse -expectedPattern "Grbl.*help" -timeoutMs 3000
    if ($startup) {
        Write-Host "Startup: $startup"
        Write-Host "✅ Controller reset and ready"
    } else {
        Write-Host "❌ No startup message received"
        return
    }
    
    # UGS Step 2: Status report (single ? character)
    $success = Send-CommandAndWait -command '?' -description "Status Report" -expectedPattern "<.*>" -timeoutMs 1000
    if (-not $success) { return }
    
    # UGS Step 3: Version info
    $success = Send-CommandAndWait -command '$I' -description "Version Info" -expectedPattern ".*ok.*" -timeoutMs 2000
    if (-not $success) { return }
    
    # UGS Step 4: Settings (this is where timing is critical)
    $success = Send-CommandAndWait -command '$$' -description "GRBL Settings" -expectedPattern ".*ok.*" -timeoutMs 3000
    if (-not $success) { return }
    
    # UGS Step 5: Parser state
    $success = Send-CommandAndWait -command '$G' -description "Parser State" -expectedPattern ".*ok.*" -timeoutMs 2000
    if (-not $success) { return }
    
    # UGS Step 6: Final status to confirm connection
    $success = Send-CommandAndWait -command '?' -description "Final Status Check" -expectedPattern "<.*>" -timeoutMs 1000
    if (-not $success) { return }
    
    Write-Host ""
    Write-Host "🎉 UGS Handshake Complete - Controller is ready!"
    Write-Host "*** Connection would be established in real UGS ***"
    
} catch {
    Write-Host "❌ Connection error: $($_.Exception.Message)"
} finally {
    if ($port.IsOpen) {
        $port.Close()
    }
}