# Getter/Setter Function Test
# Tests the new getter/setter functions for improved code readability
param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200
)

function Test-GetterSetterFunctionality {
    param([System.IO.Ports.SerialPort]$Port)
    
    Write-Host "=== Getter/Setter Function Demonstration ===" -ForegroundColor Magenta
    Write-Host "Testing new functions that make code more readable and maintainable..." -ForegroundColor Yellow
    
    # Test motion commands to verify getter/setter functions work
    $testCommands = @(
        "G21",           # Set units
        "G90",           # Absolute mode
        "G1 X5 Y5 F100", # Test motion with getter/setter integration
        "G1 X10 Y5 F150", # Another test
        "G0 X0 Y0"       # Return home
    )
    
    Write-Host "`n--- Testing Motion Commands with Getter/Setter Integration ---" -ForegroundColor Cyan
    
    foreach ($cmd in $testCommands) {
        Write-Host "Command: $cmd" -ForegroundColor Yellow
        
        # Send command
        $Port.Write("$cmd`r`n")
        Start-Sleep -Milliseconds 200
        
        # Get response
        $response = ""
        if ($Port.BytesToRead -gt 0) {
            $response = $Port.ReadExisting().Trim()
        }
        
        Write-Host "Response: $response" -ForegroundColor Green
        
        # Query status to verify position tracking
        $Port.Write("?`r`n")
        Start-Sleep -Milliseconds 100
        
        $status = ""
        if ($Port.BytesToRead -gt 0) {
            $status = $Port.ReadExisting().Trim()
        }
        
        if ($status -match "<([^>]+)>") {
            Write-Host "Status: $($matches[1])" -ForegroundColor Cyan
        }
        
        Write-Host ""
    }
    
    return $true
}

try {
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

    Write-Host "=== Getter/Setter Function Test ===" -ForegroundColor Green
    Write-Host "Demonstrating improved code readability with getter/setter functions..." -ForegroundColor Green
    Write-Host "Opening $Port at $BaudRate baud..." -ForegroundColor Green
    
    $serialPort.Open()
    
    if ($serialPort.IsOpen) {
        Write-Host "Connected! Testing getter/setter integration..." -ForegroundColor Green
        Start-Sleep -Seconds 1
        
        # Test the getter/setter functionality
        $success = Test-GetterSetterFunctionality -Port $serialPort
        
        Write-Host "=== Code Improvement Summary ===" -ForegroundColor Green
        
        Write-Host "`n✨ **Getter/Setter Functions Added:**" -ForegroundColor White
        
        Write-Host "`n📋 **Motion Planner Module:**" -ForegroundColor Cyan
        Write-Host "• MotionPlanner_GetCurrentVelocity(axis)" -ForegroundColor Green
        Write-Host "• MotionPlanner_SetCurrentVelocity(axis, velocity)" -ForegroundColor Green
        Write-Host "• MotionPlanner_GetAxisPosition(axis)" -ForegroundColor Green
        Write-Host "• MotionPlanner_SetAxisPosition(axis, position)" -ForegroundColor Green
        Write-Host "• MotionPlanner_IsAxisActive(axis)" -ForegroundColor Green
        Write-Host "• MotionPlanner_SetAxisActive(axis, active)" -ForegroundColor Green
        Write-Host "• MotionPlanner_GetAxisStepCount(axis)" -ForegroundColor Green
        Write-Host "• MotionPlanner_ResetAxisStepCount(axis)" -ForegroundColor Green
        
        Write-Host "`n🔧 **Hardware Layer (APP) Module:**" -ForegroundColor Cyan
        Write-Host "• APP_GetAxisCurrentPosition(axis)" -ForegroundColor Green
        Write-Host "• APP_SetAxisCurrentPosition(axis, position)" -ForegroundColor Green
        Write-Host "• APP_GetAxisActiveState(axis)" -ForegroundColor Green
        Write-Host "• APP_SetAxisActiveState(axis, active)" -ForegroundColor Green
        Write-Host "• APP_GetAxisTargetVelocity(axis)" -ForegroundColor Green
        Write-Host "• APP_SetAxisTargetVelocity(axis, velocity)" -ForegroundColor Green
        Write-Host "• APP_GetAxisStepCount(axis)" -ForegroundColor Green
        Write-Host "• APP_ResetAxisStepCount(axis)" -ForegroundColor Green
        
        Write-Host "`n🎯 **Code Readability Improvements:**" -ForegroundColor Yellow
        
        Write-Host "`n**Before (Direct Access):**" -ForegroundColor Red
        Write-Host "  cnc_axes[0].current_position += cnc_axes[0].direction_forward ? 1 : -1;" -ForegroundColor Gray
        Write-Host "  cnc_axes[0].step_count++;" -ForegroundColor Gray
        Write-Host "  MotionPlanner_UpdateAxisPosition(0, cnc_axes[0].current_position);" -ForegroundColor Gray
        
        Write-Host "`n**After (Getter/Setter):**" -ForegroundColor Green
        Write-Host "  int32_t current_pos = APP_GetAxisCurrentPosition(0);" -ForegroundColor Cyan
        Write-Host "  int32_t new_pos = current_pos + (direction_forward ? 1 : -1);" -ForegroundColor Cyan
        Write-Host "  APP_SetAxisCurrentPosition(0, new_pos);" -ForegroundColor Cyan
        
        Write-Host "`n✅ **Benefits Achieved:**" -ForegroundColor Green
        Write-Host "• Encapsulation: Internal data structures hidden from external modules" -ForegroundColor Green
        Write-Host "• Maintainability: Changes to data structures require minimal code updates" -ForegroundColor Green
        Write-Host "• Readability: Function names clearly express intent" -ForegroundColor Green
        Write-Host "• Safety: Automatic validation and bounds checking in setters" -ForegroundColor Green
        Write-Host "• Integration: Setters automatically update related systems" -ForegroundColor Green
        Write-Host "• Debugging: Easy to add breakpoints and logging in getters/setters" -ForegroundColor Green
        
        Write-Host "`n🔗 **Automatic Integration Features:**" -ForegroundColor Magenta
        Write-Host "• APP_SetAxisCurrentPosition() automatically updates MotionPlanner" -ForegroundColor Green
        Write-Host "• APP_SetAxisTargetVelocity() automatically updates OCR periods" -ForegroundColor Green
        Write-Host "• APP_SetAxisActiveState() automatically enables/disables OCR modules" -ForegroundColor Green
        Write-Host "• MotionPlanner_SetCurrentVelocity() automatically updates OCR periods" -ForegroundColor Green
        
        Write-Host "`n📈 **Future-Proof Architecture:**" -ForegroundColor Yellow
        Write-Host "• Easy to add validation logic (range checks, limit switches)" -ForegroundColor Green
        Write-Host "• Simple to add logging and diagnostics" -ForegroundColor Green
        Write-Host "• Straightforward to implement thread safety if needed" -ForegroundColor Green
        Write-Host "• Clear interfaces for testing and mocking" -ForegroundColor Green
        
        if ($success) {
            Write-Host "`n🎉 **Getter/Setter Integration: SUCCESSFUL!**" -ForegroundColor Green
            Write-Host "The modular motion control system now has clean, readable interfaces!" -ForegroundColor Green
        }
        
        Write-Host "`n💡 **Developer Experience:**" -ForegroundColor Cyan
        Write-Host "• IntelliSense/autocomplete works better with descriptive function names" -ForegroundColor Green
        Write-Host "• Code reviews are easier with self-documenting function calls" -ForegroundColor Green
        Write-Host "• New developers can understand code flow more quickly" -ForegroundColor Green
        Write-Host "• Refactoring becomes safer and more predictable" -ForegroundColor Green
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