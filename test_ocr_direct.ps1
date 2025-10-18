###############################################################################
# test_ocr_direct.ps1
# Direct hardware test for OCR period scaling verification
#
# This script:
# 1. Connects to COM port
# 2. Sends '$T' command to trigger direct hardware test (GRBL system command)
# 3. Captures all output from test execution
# 4. Analyzes results for PASS/FAIL
#
# Usage: .\test_ocr_direct.ps1 -Port COM4 -BaudRate 115200
#
# Note: Uses '$T' instead of 'T' to avoid conflict with G-code T (tool change)
###############################################################################

param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200,
    [int]$TimeoutSeconds = 30
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  OCR PERIOD SCALING DIRECT TEST" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Initialize serial port
Write-Host "Connecting to $Port @ $BaudRate baud..." -ForegroundColor Yellow
$serialPort = New-Object System.IO.Ports.SerialPort
$serialPort.PortName = $Port
$serialPort.BaudRate = $BaudRate
$serialPort.DataBits = 8
$serialPort.Parity = [System.IO.Ports.Parity]::None
$serialPort.StopBits = [System.IO.Ports.StopBits]::One
$serialPort.ReadTimeout = 1000
$serialPort.WriteTimeout = 1000

try {
    $serialPort.Open()
    Write-Host "✓ Connected!" -ForegroundColor Green
    Write-Host ""
    
    # Wait for system to stabilize
    Start-Sleep -Milliseconds 500
    
    # Clear any pending data
    $serialPort.DiscardInBuffer()
    $serialPort.DiscardOutBuffer()
    
    Write-Host "Sending '`$T' command to trigger direct hardware test..." -ForegroundColor Yellow
    Write-Host ""
    
    # Send test trigger command (GRBL system command style)
    $serialPort.Write("`$T")
    
    # Capture output
    $output = @()
    $startTime = Get-Date
    $testStarted = $false
    $testComplete = $false
    
    Write-Host "Capturing test output:" -ForegroundColor Cyan
    Write-Host "----------------------------------------" -ForegroundColor DarkGray
    
    while (((Get-Date) - $startTime).TotalSeconds -lt $TimeoutSeconds) {
        try {
            $line = $serialPort.ReadLine()
            if ($line) {
                Write-Host $line -ForegroundColor Gray
                $output += $line
                
                # Detect test start
                if ($line -match "OCR PERIOD SCALING - DIRECT TEST") {
                    $testStarted = $true
                }
                
                # Detect test completion
                if ($line -match "OCR PERIOD SCALING: VERIFIED!" -or $line -match "Position error") {
                    $testComplete = $true
                    # Wait a bit more to catch final output
                    Start-Sleep -Milliseconds 500
                    break
                }
            }
        }
        catch {
            # ReadLine timeout - continue waiting
            Start-Sleep -Milliseconds 100
        }
    }
    
    Write-Host "----------------------------------------" -ForegroundColor DarkGray
    Write-Host ""
    
    # Analyze results
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  TEST ANALYSIS" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
    
    if (-not $testStarted) {
        Write-Host "✗ FAIL: Test did not start!" -ForegroundColor Red
        Write-Host "  - Verify firmware is flashed correctly" -ForegroundColor Yellow
        Write-Host "  - Check if 'T' command is recognized" -ForegroundColor Yellow
        exit 1
    }
    
    if (-not $testComplete) {
        Write-Host "✗ FAIL: Test did not complete within $TimeoutSeconds seconds!" -ForegroundColor Red
        Write-Host "  - Hardware may be stuck in motion" -ForegroundColor Yellow
        Write-Host "  - Check for mechanical binding or limit switch issues" -ForegroundColor Yellow
        exit 1
    }
    
    # Check for PASS indicators
    $test1Pass = $output -match "Test 1:.*✓ PASS"
    $test2Pass = $output -match "Test 2:.*✓ PASS"
    $test3Pass = $output -match "Test 3:.*✓ PASS" -or $output -match "✓ PASS: Returned to origin"
    $overallPass = $output -match "OCR PERIOD SCALING: VERIFIED!"
    
    Write-Host "Test 1 (Y-axis 800 steps):     " -NoNewline
    if ($test1Pass) {
        Write-Host "✓ PASS" -ForegroundColor Green
    } else {
        Write-Host "✗ FAIL" -ForegroundColor Red
    }
    
    Write-Host "Test 2 (Coordinated X/Y):      " -NoNewline
    if ($test2Pass) {
        Write-Host "✓ PASS" -ForegroundColor Green
    } else {
        Write-Host "✗ FAIL" -ForegroundColor Red
    }
    
    Write-Host "Test 3 (Return to origin):     " -NoNewline
    if ($test3Pass) {
        Write-Host "✓ PASS" -ForegroundColor Green
    } else {
        Write-Host "✗ FAIL" -ForegroundColor Red
    }
    
    Write-Host ""
    Write-Host "Overall Result:                " -NoNewline
    if ($overallPass) {
        Write-Host "✓ OCR PERIOD SCALING VERIFIED!" -ForegroundColor Green
        Write-Host ""
        Write-Host "🎉 SUCCESS! The OCR period scaling architecture is working correctly!" -ForegroundColor Green
        Write-Host ""
        Write-Host "Next Steps:" -ForegroundColor Cyan
        Write-Host "  1. Fix coordinate system bugs (G92, G91)" -ForegroundColor Yellow
        Write-Host "  2. Test full G-code motion via UGS" -ForegroundColor Yellow
        Write-Host "  3. Implement look-ahead planning" -ForegroundColor Yellow
        exit 0
    } else {
        Write-Host "✗ FAIL: OCR period scaling issues detected" -ForegroundColor Red
        Write-Host ""
        Write-Host "Diagnostics:" -ForegroundColor Yellow
        
        # Extract step counts from output
        $stepCountLines = $output | Where-Object { $_ -match "steps = .* mm" }
        if ($stepCountLines) {
            Write-Host ""
            Write-Host "Step Count Report:" -ForegroundColor Cyan
            foreach ($line in $stepCountLines) {
                Write-Host "  $line" -ForegroundColor Gray
            }
        }
        
        # Check for common issues
        $failLines = $output | Where-Object { $_ -match "✗ FAIL" }
        if ($failLines) {
            Write-Host ""
            Write-Host "Failure Details:" -ForegroundColor Cyan
            foreach ($line in $failLines) {
                Write-Host "  $line" -ForegroundColor Red
            }
        }
        
        Write-Host ""
        Write-Host "Possible Issues:" -ForegroundColor Yellow
        Write-Host "  - OCR period overflow (check TMR_CLOCK_HZ = 1562500)" -ForegroundColor Yellow
        Write-Host "  - Timer prescaler not set to 1:16 in MCC" -ForegroundColor Yellow
        Write-Host "  - Hardware connections or stepper driver issues" -ForegroundColor Yellow
        Write-Host "  - Direction pin setup incorrect" -ForegroundColor Yellow
        exit 1
    }
}
catch {
    Write-Host "✗ Error: $_" -ForegroundColor Red
    exit 1
}
finally {
    if ($serialPort.IsOpen) {
        $serialPort.Close()
        Write-Host ""
        Write-Host "Serial port closed." -ForegroundColor Gray
    }
}
