# Close all open COM port handles in PowerShell
# Run this if you get "Access to the path 'COM4' is denied."

Write-Host "Searching for open COM port handles..." -ForegroundColor Cyan

# Get all PowerShell processes
$processes = Get-Process -Name pwsh, powershell -ErrorAction SilentlyContinue

if ($processes) {
    Write-Host "Found $($processes.Count) PowerShell process(es)" -ForegroundColor Yellow
    Write-Host "Attempting to close COM ports in current session..." -ForegroundColor Yellow
    
    # Try to close any SerialPort objects in this session
    Get-Variable -Scope Global | Where-Object { 
        $_.Value -is [System.IO.Ports.SerialPort] 
    } | ForEach-Object {
        Write-Host "Closing $($_.Name): $($_.Value.PortName)" -ForegroundColor Green
        try {
            if ($_.Value.IsOpen) {
                $_.Value.Close()
                Write-Host "  ✓ Closed $($_.Value.PortName)" -ForegroundColor Green
            }
        }
        catch {
            Write-Host "  ✗ Failed to close: $_" -ForegroundColor Red
        }
    }
}
else {
    Write-Host "No PowerShell processes found" -ForegroundColor Gray
}

# Check for other processes that might have COM4 open
Write-Host "`nChecking for other processes using COM ports..." -ForegroundColor Cyan

# Common culprits
$suspects = @(
    "putty",
    "teraterm",
    "arduino",
    "mplab",
    "mplabx",
    "ipecmd",
    "java"  # UGS runs on Java
)

$foundProcesses = @()
foreach ($name in $suspects) {
    $procs = Get-Process -Name $name -ErrorAction SilentlyContinue
    if ($procs) {
        $foundProcesses += $procs
    }
}

if ($foundProcesses.Count -gt 0) {
    Write-Host "Found processes that may have COM ports open:" -ForegroundColor Yellow
    $foundProcesses | ForEach-Object {
        Write-Host "  - $($_.ProcessName) (PID: $($_.Id))" -ForegroundColor Yellow
    }
    
    $response = Read-Host "`nDo you want to close these processes? (y/N)"
    if ($response -eq 'y' -or $response -eq 'Y') {
        $foundProcesses | ForEach-Object {
            try {
                Stop-Process -Id $_.Id -Force
                Write-Host "  ✓ Closed $($_.ProcessName)" -ForegroundColor Green
            }
            catch {
                Write-Host "  ✗ Failed to close $($_.ProcessName): $_" -ForegroundColor Red
            }
        }
    }
}
else {
    Write-Host "No known COM port applications found" -ForegroundColor Gray
}

Write-Host "`n=== Manual Steps ===" -ForegroundColor Cyan
Write-Host "If COM4 is still locked:" -ForegroundColor White
Write-Host "1. Close all PowerShell windows" -ForegroundColor White
Write-Host "2. Close UGS/Arduino IDE/PuTTY/TeraTerm" -ForegroundColor White
Write-Host "3. Unplug and replug USB cable" -ForegroundColor White
Write-Host "4. Run test script again" -ForegroundColor White
