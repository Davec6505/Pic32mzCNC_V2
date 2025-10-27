# Drift Diagnostic Test
# Tests different speeds and accelerations to isolate the cause of 3mm drift

param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200,
    [int]$DelayMs = 100
)

function Send-GCode {
    param([string]$Command)
    $port = New-Object System.IO.Ports.SerialPort $Port, $BaudRate, None, 8, One
    $port.Open()
    Start-Sleep -Milliseconds 50
    
    Write-Host ">> $Command" -ForegroundColor Cyan
    $port.WriteLine($Command)
    Start-Sleep -Milliseconds $DelayMs
    
    while ($port.BytesToRead -gt 0) {
        $response = $port.ReadLine()
        Write-Host "<< $response" -ForegroundColor Yellow
    }
    
    $port.Close()
    Start-Sleep -Milliseconds 50
}

Write-Host "`n=== DRIFT DIAGNOSTIC TEST SEQUENCE ===" -ForegroundColor Green
Write-Host "This will test at different speeds to isolate the drift cause`n"

# Test 1: Very slow (minimal acceleration stress)
Write-Host "`n--- TEST 1: SLOW SPEED (F500) ---" -ForegroundColor Magenta
Send-GCode "G92 X0 Y0"
Send-GCode "G1 X90 F500"
Start-Sleep -Seconds 1
Send-GCode "G1 X0 F500"
Start-Sleep -Seconds 1
Write-Host "Check position - should be at X=0" -ForegroundColor Yellow
Send-GCode "?"
Start-Sleep -Seconds 2

# Test 2: Medium speed
Write-Host "`n--- TEST 2: MEDIUM SPEED (F1500) ---" -ForegroundColor Magenta
Send-GCode "G92 X0 Y0"
Send-GCode "G1 X90 F1500"
Start-Sleep -Seconds 1
Send-GCode "G1 X0 F1500"
Start-Sleep -Seconds 1
Write-Host "Check position - compare drift vs Test 1" -ForegroundColor Yellow
Send-GCode "?"
Start-Sleep -Seconds 2

# Test 3: Fast speed (your current F3000)
Write-Host "`n--- TEST 3: FAST SPEED (F3000) ---" -ForegroundColor Magenta
Send-GCode "G92 X0 Y0"
Send-GCode "G1 X90 F3000"
Start-Sleep -Seconds 1
Send-GCode "G1 X0 F3000"
Start-Sleep -Seconds 1
Write-Host "Check position - compare drift vs Test 1 and 2" -ForegroundColor Yellow
Send-GCode "?"
Start-Sleep -Seconds 2

# Test 4: Reduce acceleration
Write-Host "`n--- TEST 4: REDUCED ACCELERATION (50mm/s²) ---" -ForegroundColor Magenta
Send-GCode "`$120=50"  # Reduce X acceleration
Send-GCode "`$121=50"  # Reduce Y acceleration
Start-Sleep -Milliseconds 500
Send-GCode "G92 X0 Y0"
Send-GCode "G1 X90 F3000"
Start-Sleep -Seconds 1
Send-GCode "G1 X0 F3000"
Start-Sleep -Seconds 1
Write-Host "Check position - if better, problem is acceleration" -ForegroundColor Yellow
Send-GCode "?"
Start-Sleep -Seconds 2

# Restore original acceleration
Write-Host "`n--- RESTORING ORIGINAL SETTINGS ---" -ForegroundColor Magenta
Send-GCode "`$120=100"
Send-GCode "`$121=100"

Write-Host "`n=== TEST COMPLETE ===" -ForegroundColor Green
Write-Host "`nDiagnostic Results:" -ForegroundColor Cyan
Write-Host "  - If drift SAME at all speeds: Mechanical backlash (belt slack)"
Write-Host "  - If drift WORSE at higher speeds: Acceleration or driver current"
Write-Host "  - If drift BETTER with reduced accel: Reduce `$120/`$121 permanently"
