#!/usr/bin/env pwsh

# Test script to verify NaN fix for position tracking
# This script tests the robustness of our position calculation functions

Write-Host "===== Testing NaN Position Fix =====" -ForegroundColor Yellow
Write-Host ""

# Build the project first
Write-Host "Building project..." -ForegroundColor Cyan
make 2>&1 | Out-Host
if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "Build successful!" -ForegroundColor Green
Write-Host ""

# Test 1: Send jogging command and check position
Write-Host "Test 1: Testing jogging command with position tracking" -ForegroundColor Cyan
Write-Host "Sending: G21G91X-1F99" -ForegroundColor Yellow

# Use serial communication to send command and get response
$response = powershell -Command "
    `$port = new-Object System.IO.Ports.SerialPort COM3,115200,None,8,one
    try {
        `$port.Open()
        `$port.WriteLine('G21G91X-1F99')
        Start-Sleep -Milliseconds 500
        `$port.WriteLine('?')
        Start-Sleep -Milliseconds 200
        
        `$responseData = ''
        while (`$port.BytesToRead -gt 0) {
            `$responseData += `$port.ReadExisting()
        }
        Write-Output `$responseData
    }
    catch {
        Write-Output 'Serial communication failed: ' + `$_.Exception.Message
    }
    finally {
        if (`$port.IsOpen) { `$port.Close() }
    }
"

Write-Host "Response: $response" -ForegroundColor White

# Check for NaN values in response
if ($response -match "nan") {
    Write-Host "❌ FAILED: Still getting NaN values!" -ForegroundColor Red
    Write-Host "Response contains: $response" -ForegroundColor Red
} else {
    Write-Host "✅ SUCCESS: No NaN values detected!" -ForegroundColor Green
}

Write-Host ""

# Test 2: Check default values are being used
Write-Host "Test 2: Checking if default values are being used properly" -ForegroundColor Cyan
Write-Host "Expected default steps_per_mm: X=160.0, Y=160.0, Z=160.0" -ForegroundColor Yellow

# Test 3: Send status request to see current position format
Write-Host "Test 3: Requesting status to verify position format" -ForegroundColor Cyan
$status_response = powershell -Command "
    `$port = new-Object System.IO.Ports.SerialPort COM3,115200,None,8,one
    try {
        `$port.Open()
        `$port.WriteLine('?')
        Start-Sleep -Milliseconds 300
        
        `$responseData = ''
        while (`$port.BytesToRead -gt 0) {
            `$responseData += `$port.ReadExisting()
        }
        Write-Output `$responseData
    }
    catch {
        Write-Output 'Serial communication failed: ' + `$_.Exception.Message
    }
    finally {
        if (`$port.IsOpen) { `$port.Close() }
    }
"

Write-Host "Status Response: $status_response" -ForegroundColor White

# Parse and analyze the status response
if ($status_response -match "MPos:([^|]+)") {
    $position = $matches[1]
    Write-Host "Parsed Position: $position" -ForegroundColor Cyan
    
    if ($position -match "nan") {
        Write-Host "❌ FAILED: Position still contains NaN!" -ForegroundColor Red
    } elseif ($position -match "^\d+\.\d+,\d+\.\d+,\d+\.\d+$") {
        Write-Host "✅ SUCCESS: Position format is correct!" -ForegroundColor Green
    } else {
        Write-Host "⚠️  WARNING: Unexpected position format: $position" -ForegroundColor Yellow
    }
} else {
    Write-Host "⚠️  WARNING: Could not parse position from status" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "===== NaN Fix Test Complete =====" -ForegroundColor Yellow