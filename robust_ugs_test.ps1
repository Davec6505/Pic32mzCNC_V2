# UGS Handshake Test - Simplified and Robust
# This script waits properly for each complete response

$port = New-Object System.IO.Ports.SerialPort "COM4", 115200
$port.ReadTimeout = 500
$port.WriteTimeout = 1000
$port.DtrEnable = $true
$port.RtsEnable = $false

function Send-GrblCommand {
    param(
        [string]$command,
        [string]$description,
        [int]$timeoutMs = 3000
    )
    
    Write-Host ""
    Write-Host "*** $description"
    Write-Host ">>> $command"
    
    # Clear any existing data
    $port.DiscardInBuffer()
    
    # Send command
    if ($command -eq '?') {
        $port.Write($command)  # Status queries don't need newline
    } else {
        $port.WriteLine($command)
    }
    
    # Collect response until we see "ok" or timeout
    $response = ""
    $lines = @()
    $startTime = Get-Date
    $foundOk = $false
    
    while (((Get-Date) - $startTime).TotalMilliseconds -lt $timeoutMs) {
        try {
            if ($port.BytesToRead -gt 0) {
                $line = $port.ReadLine().Trim()
                if ($line -ne "") {
                    $lines += $line
                    Write-Host $line
                    
                    if ($line -eq "ok") {
                        $foundOk = $true
                        break
                    }
                    if ($command -eq '?' -and $line -match '<.*>') {
                        # Status reports don't have "ok"
                        $foundOk = $true
                        break
                    }
                }
            } else {
                Start-Sleep -Milliseconds 10
            }
        } catch {
            # ReadLine timeout, continue
            Start-Sleep -Milliseconds 10
        }
    }
    
    if ($foundOk -or ($command -eq '?' -and $lines.Count -gt 0)) {
        Write-Host "✅ Command completed successfully"
        return $true
    } else {
        Write-Host "❌ Command failed or timed out"
        return $false
    }
}

Write-Host "=== UGS Handshake Test (Robust) ==="

try {
    $port.Open()
    Write-Host "*** Connected to COM4:115200"
    
    # Step 1: Soft reset
    Write-Host ""
    Write-Host "*** Soft Reset"
    $port.Write([char]24)  # Ctrl+X
    Start-Sleep -Milliseconds 1500
    
    # Read startup message
    try {
        while ($port.BytesToRead -gt 0) {
            $startup = $port.ReadLine().Trim()
            if ($startup -ne "") {
                Write-Host "Startup: $startup"
            }
        }
    } catch {}
    
    # Step 2: Status report
    if (-not (Send-GrblCommand -command '?' -description "Status Report" -timeoutMs 1000)) { return }
    
    # Step 3: Version
    if (-not (Send-GrblCommand -command '$I' -description "Version Info" -timeoutMs 2000)) { return }
    
    # Step 4: Settings
    if (-not (Send-GrblCommand -command '$$' -description "All Settings" -timeoutMs 5000)) { return }
    
    # Step 5: Parser state
    if (-not (Send-GrblCommand -command '$G' -description "Parser State" -timeoutMs 2000)) { return }
    
    # Step 6: Final status
    if (-not (Send-GrblCommand -command '?' -description "Final Status" -timeoutMs 1000)) { return }
    
    Write-Host ""
    Write-Host "🎉 UGS Handshake Sequence Complete!"
    Write-Host "*** Controller is ready for UGS connection ***"
    
} catch {
    Write-Host "❌ Error: $($_.Exception.Message)"
} finally {
    if ($port.IsOpen) {
        $port.Close()
    }
}