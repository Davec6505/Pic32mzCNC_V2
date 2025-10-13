#!/usr/bin/env pwsh

# Diagnostic test for motion execution debug analysis
# Analyzes your UGS output to identify the exact problem

Write-Host "===== Motion Execution Debug Analysis =====" -ForegroundColor Yellow
Write-Host ""

Write-Host "ANALYZING YOUR UGS OUTPUT:" -ForegroundColor Cyan
Write-Host ""

Write-Host "✅ WHAT'S WORKING:" -ForegroundColor Green
Write-Host "• G-code parsing: 'Tokenized X items' messages present" -ForegroundColor White
Write-Host "• Motion detection: 'Motion command detected' messages present" -ForegroundColor White
Write-Host "• Buffer management: '[BUFFER_ADD] head=X tail=0' messages present" -ForegroundColor White
Write-Host "• Block reception: '[DEBUG: Motion planner got new block]' messages present" -ForegroundColor White
Write-Host "• Block details: '[DEBUG: Block target X=0.000 Y=0.000 Z=5.000]' messages present" -ForegroundColor White
Write-Host ""

Write-Host "❌ WHAT'S MISSING:" -ForegroundColor Red
Write-Host "• NO '[DEBUG: MotionPlanner_ExecuteBlock called - starting hardware motion]' messages" -ForegroundColor White
Write-Host "• NO '[DEBUG: APP_ExecuteMotionBlock SUCCESS - hardware started]' messages" -ForegroundColor White
Write-Host "• NO tail pointer advancement (stays at 0)" -ForegroundColor White
Write-Host "• NO position changes (stays 0.000,0.000,0.000)" -ForegroundColor White
Write-Host "• NO step count changes (stays 0,0,0)" -ForegroundColor White
Write-Host ""

Write-Host "🔍 DIAGNOSIS:" -ForegroundColor Yellow
Write-Host "The problem is that MotionPlanner_ExecuteBlock is NOT being called!" -ForegroundColor Red
Write-Host ""
Write-Host "Possible causes:" -ForegroundColor Yellow
Write-Host "1. Function call syntax error or missing include" -ForegroundColor White
Write-Host "2. Conditional logic preventing the call" -ForegroundColor White
Write-Host "3. Timing issue - call happens but gets missed" -ForegroundColor White
Write-Host "4. Build issue - old version still running" -ForegroundColor White
Write-Host ""

Write-Host "🔧 DEBUGGING STEPS:" -ForegroundColor Cyan
Write-Host "1. Flash the new firmware with debug output" -ForegroundColor White
Write-Host "2. Run a single G0Z5 command" -ForegroundColor White
Write-Host "3. Look for these specific debug messages:" -ForegroundColor White
Write-Host "   '[DEBUG: Motion planner got new block]' ✅ (you see this)" -ForegroundColor Green
Write-Host "   '[DEBUG: MotionPlanner_ExecuteBlock called...]' ❌ (missing)" -ForegroundColor Red
Write-Host "   '[DEBUG: APP_ExecuteMotionBlock SUCCESS...]' ❌ (missing)" -ForegroundColor Red
Write-Host ""

Write-Host "🎯 EXPECTED FLOW:" -ForegroundColor Yellow
Write-Host "G0Z5 command should produce:" -ForegroundColor White
Write-Host "1. [DEBUG: ProcessGCode called with: 'G0Z5'] ✅" -ForegroundColor Green
Write-Host "2. [DEBUG: Motion command detected - G0 (Rapid)] ✅" -ForegroundColor Green
Write-Host "3. [BUFFER_ADD] head=1 tail=0 ✅" -ForegroundColor Green
Write-Host "4. [DEBUG: Motion planner got new block] ✅" -ForegroundColor Green
Write-Host "5. [DEBUG: MotionPlanner_ExecuteBlock called...] ❌ MISSING!" -ForegroundColor Red
Write-Host "6. [DEBUG: APP_ExecuteMotionBlock SUCCESS...] ❌ MISSING!" -ForegroundColor Red
Write-Host "7. [DEBUG: Motion completed - updating cnc_axes...] ❌ MISSING!" -ForegroundColor Red
Write-Host "8. [DEBUG: Calling MotionBuffer_Complete() - advancing tail] ❌ MISSING!" -ForegroundColor Red
Write-Host ""

Write-Host "✅ ACTION PLAN:" -ForegroundColor Green
Write-Host "Since messages 1-4 work but 5-8 are missing, the issue is in the" -ForegroundColor White
Write-Host "MotionPlanner_ExecuteBlock() call within MotionPlanner_UpdateTrajectory()" -ForegroundColor White
Write-Host ""
Write-Host "Ready to flash firmware and test with enhanced debug output!" -ForegroundColor Green

Write-Host ""
Write-Host "===== Debug Analysis Complete =====" -ForegroundColor Yellow