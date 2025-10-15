# Cleanup Script - Remove Old Custom Code, Keep Harmony Configuration
# Part of GRBL Rebuild - Phase 0: Clean Slate

param(
    [switch]$DryRun = $false
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Cleanup Old Custom Code" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

if ($DryRun) {
    Write-Host "[DRY RUN MODE - No files will be deleted]" -ForegroundColor Yellow
    Write-Host ""
}

# Files to DELETE from srcs/
$SrcsFilesToDelete = @(
    "gcode_helpers.c",
    "gcode_parser.c",
    "gcode_test_app.c",
    "gcode_test_framework.c",
    "grbl_serial.c",
    "grbl_settings.c",
    "interpolation_engine.c",
    "motion_buffer.c",
    "motion_gcode_parser.c",
    "motion_planner.c",
    "motion_profile.c",
    "speed_control.c",
    "uart_debug.c",
    "utils.c"
)

# Files to DELETE from incs/
$IncsFilesToDelete = @(
    "cnc_config_example.h",
    "gcode_examples.h",
    "gcode_helpers.h",
    "gcode_parser.h",
    "gcode_test_app.h",
    "gcode_test_framework.h",
    "global.h",
    "grbl_serial.h",
    "grbl_settings.h",
    "integration_example.h",
    "interpolation_engine.h",
    "motion_buffer.h",
    "motion_gcode_parser.h",
    "motion_planner.h",
    "motion_profile.h",
    "speed_control.h",
    "uart_debug.h",
    "utils.h"
)

# Files to KEEP (will be modified for GRBL integration)
$FilesToKeep = @(
    "srcs/app.c",
    "srcs/main.c",
    "incs/app.h"
)

Write-Host "Files marked for DELETION:" -ForegroundColor Yellow
Write-Host ""

$DeletedCount = 0
$KeptCount = 0

# Delete source files
Write-Host "Source Files (srcs/):" -ForegroundColor Cyan
foreach ($File in $SrcsFilesToDelete) {
    $FullPath = Join-Path "srcs" $File
    if (Test-Path $FullPath) {
        Write-Host "  [DELETE] $File" -ForegroundColor Red
        if (-not $DryRun) {
            Remove-Item $FullPath -Force
        }
        $DeletedCount++
    }
    else {
        Write-Host "  [SKIP] $File (not found)" -ForegroundColor Gray
    }
}

Write-Host ""
Write-Host "Header Files (incs/):" -ForegroundColor Cyan
foreach ($File in $IncsFilesToDelete) {
    $FullPath = Join-Path "incs" $File
    if (Test-Path $FullPath) {
        Write-Host "  [DELETE] $File" -ForegroundColor Red
        if (-not $DryRun) {
            Remove-Item $FullPath -Force
        }
        $DeletedCount++
    }
    else {
        Write-Host "  [SKIP] $File (not found)" -ForegroundColor Gray
    }
}

Write-Host ""
Write-Host "Files marked to KEEP:" -ForegroundColor Green
foreach ($File in $FilesToKeep) {
    if (Test-Path $File) {
        Write-Host "  [KEEP] $File" -ForegroundColor Green
        $KeptCount++
    }
}

Write-Host ""
Write-Host "Harmony Configuration (PRESERVED):" -ForegroundColor Green
Write-Host "  [KEEP] srcs/config/* (all Harmony generated files)" -ForegroundColor Green
Write-Host "  [KEEP] incs/config/* (all Harmony peripheral headers)" -ForegroundColor Green
Write-Host "  [KEEP] srcs/startup/* (startup assembly code)" -ForegroundColor Green

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Cleanup Summary:" -ForegroundColor Cyan
if ($DryRun) {
    Write-Host "  [DRY RUN] Would delete: $DeletedCount files" -ForegroundColor Yellow
}
else {
    Write-Host "  Deleted: $DeletedCount files" -ForegroundColor Red
}
Write-Host "  Preserved: $KeptCount core files" -ForegroundColor Green
Write-Host "  Preserved: All Harmony config files" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

if ($DryRun) {
    Write-Host "Run without -DryRun to actually delete files" -ForegroundColor Yellow
    Write-Host ""
}
else {
    Write-Host "Next Steps:" -ForegroundColor Yellow
    Write-Host "  1. Copy GRBL files from grbl-source/ to srcs/" -ForegroundColor White
    Write-Host "  2. Create grbl_stepper.c for OCR hardware" -ForegroundColor White
    Write-Host "  3. Update app.c to interface with GRBL" -ForegroundColor White
    Write-Host "  4. Update main.c to call GRBL initialization" -ForegroundColor White
    Write-Host "  5. Update Makefile for GRBL build" -ForegroundColor White
    Write-Host ""
    
    # Create a marker file
    $MarkerFile = "CLEANUP_COMPLETE.txt"
    $MarkerContent = @"
Old Custom Code Cleanup - Complete
===================================
Date: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
Files Deleted: $DeletedCount
Files Preserved: $KeptCount + Harmony config

Files Preserved for GRBL Integration:
--------------------------------------
srcs/app.c      - Hardware abstraction layer (OCR, GPIO, timers)
                  Will be adapted to provide stepper interface for GRBL

srcs/main.c     - Entry point and system initialization
                  Will be modified to call GRBL protocol_main_loop()

incs/app.h      - Hardware interface definitions
                  Will expose OCR functions to grbl_stepper.c

All Harmony Generated Files Preserved:
---------------------------------------
srcs/config/default/initialization.c
srcs/config/default/interrupts.c
srcs/config/default/exceptions.c
srcs/config/default/peripheral/* (all PLIB files)
incs/config/default/* (all PLIB headers)
srcs/startup/startup.S

Ready for GRBL Source Import:
------------------------------
Next: Copy GRBL files and create OCR stepper adapter
"@
    
    $MarkerContent | Out-File -FilePath $MarkerFile -Encoding utf8
    Write-Host "Cleanup marker saved to: $MarkerFile" -ForegroundColor Cyan
    Write-Host ""
}
