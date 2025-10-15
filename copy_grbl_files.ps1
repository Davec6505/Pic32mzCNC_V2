# Copy GRBL Files to Project Structure
# Phase 1: Import GRBL core files for PIC32MZ integration

param(
    [switch]$DryRun = $false
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "GRBL Phase 1: Copy Core Files" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

if ($DryRun) {
    Write-Host "[DRY RUN MODE - No files will be copied]" -ForegroundColor Yellow
    Write-Host ""
}

# GRBL files to copy VERBATIM (keep algorithms intact)
$CoreFilesToCopy = @{
    # Motion planning (CRITICAL - copy verbatim)
    "planner.c" = "srcs/planner.c"
    "planner.h" = "incs/planner.h"
    
    # G-code parser (CRITICAL - copy verbatim)
    "gcode.c" = "srcs/gcode.c"
    "gcode.h" = "incs/gcode.h"
    
    # Motion control (arc generation, line motion)
    "motion_control.c" = "srcs/motion_control.c"
    "motion_control.h" = "incs/motion_control.h"
    
    # Protocol and communication
    "protocol.c" = "srcs/protocol.c"
    "protocol.h" = "incs/protocol.h"
    
    # System management
    "system.c" = "srcs/system.c"
    "system.h" = "incs/system.h"
    
    # Limits and homing
    "limits.c" = "srcs/limits.c"
    "limits.h" = "incs/limits.h"
    
    # Reporting
    "report.c" = "srcs/report.c"
    "report.h" = "incs/report.h"
    
    # Settings
    "settings.c" = "srcs/settings.c"
    "settings.h" = "incs/settings.h"
    
    # Utilities
    "nuts_bolts.c" = "srcs/nuts_bolts.c"
    "nuts_bolts.h" = "incs/nuts_bolts.h"
    "print.c" = "srcs/print.c"
    "print.h" = "incs/print.h"
    
    # Configuration
    "defaults.h" = "incs/defaults.h"
    "config.h" = "incs/config.h"
    
    # Jogging
    "jog.c" = "srcs/jog.c"
    "jog.h" = "incs/jog.h"
    
    # Optional features (copy for completeness)
    "probe.c" = "srcs/probe.c"
    "probe.h" = "incs/probe.h"
    "coolant_control.c" = "srcs/coolant_control.c"
    "coolant_control.h" = "incs/coolant_control.h"
    "spindle_control.c" = "srcs/spindle_control.c"
    "spindle_control.h" = "incs/spindle_control.h"
}

# Files to ADAPT for PIC32MZ (will create custom versions)
$FilesToAdapt = @(
    "stepper.c/h - Will create pic32_stepper.c (OCR hardware)",
    "serial.c/h - Will create pic32_serial.c (UART2 PLIB)",
    "eeprom.c/h - Will create pic32_nvm.c (NVM instead of EEPROM)",
    "cpu_map.h - Will create pic32_config.h (GPIO pin assignments)",
    "grbl.h - Will modify to include PIC32 headers"
)

Write-Host "Copying GRBL core files..." -ForegroundColor Yellow
Write-Host ""

$CopiedCount = 0
$SkippedCount = 0

foreach ($File in $CoreFilesToCopy.GetEnumerator()) {
    $SourcePath = Join-Path "grbl-source" $File.Key
    $DestPath = $File.Value
    
    if (Test-Path $SourcePath) {
        Write-Host "  [COPY] $($File.Key) -> $DestPath" -ForegroundColor Green
        
        if (-not $DryRun) {
            # Ensure destination directory exists
            $DestDir = Split-Path $DestPath -Parent
            if (-not (Test-Path $DestDir)) {
                New-Item -ItemType Directory -Path $DestDir -Force | Out-Null
            }
            
            # Copy file
            Copy-Item $SourcePath $DestPath -Force
        }
        
        $CopiedCount++
    }
    else {
        Write-Host "  [SKIP] $($File.Key) (source not found)" -ForegroundColor Red
        $SkippedCount++
    }
}

Write-Host ""
Write-Host "Files to ADAPT (not copying, will create custom):" -ForegroundColor Yellow
foreach ($Info in $FilesToAdapt) {
    Write-Host "  [ADAPT] $Info" -ForegroundColor Cyan
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Copy Summary:" -ForegroundColor Cyan
if ($DryRun) {
    Write-Host "  [DRY RUN] Would copy: $CopiedCount files" -ForegroundColor Yellow
}
else {
    Write-Host "  Copied: $CopiedCount files" -ForegroundColor Green
}
Write-Host "  Skipped: $SkippedCount files" -ForegroundColor Red
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

if (-not $DryRun) {
    Write-Host "Next Steps:" -ForegroundColor Yellow
    Write-Host "  1. Create pic32_stepper.c (OCR hardware adapter)" -ForegroundColor White
    Write-Host "  2. Create pic32_serial.c (UART2 adapter)" -ForegroundColor White
    Write-Host "  3. Create pic32_nvm.c (NVM settings storage)" -ForegroundColor White
    Write-Host "  4. Create pic32_config.h (GPIO pin assignments)" -ForegroundColor White
    Write-Host "  5. Create grbl.h (modified for PIC32MZ)" -ForegroundColor White
    Write-Host "  6. Update Makefile for GRBL build" -ForegroundColor White
    Write-Host ""
    
    # Create marker file
    $MarkerFile = "PHASE_1_COPY_COMPLETE.txt"
    $MarkerContent = @"
GRBL Phase 1: Copy Core Files - Complete
=========================================
Date: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
Files Copied: $CopiedCount

Core GRBL Files Imported:
--------------------------
✅ planner.c/h          - Motion planning and look-ahead buffer
✅ gcode.c/h            - G-code parser and modal state machine
✅ protocol.c/h         - GRBL protocol and serial handling
✅ motion_control.c/h   - Arc generation and line motion
✅ limits.c/h           - Limit switch and homing
✅ settings.c/h         - Settings management
✅ report.c/h           - Status reports and messages
✅ system.c/h           - System commands and state
✅ nuts_bolts.c/h       - Utility functions
✅ print.c/h            - String output functions
✅ jog.c/h              - Jogging functionality
✅ probe.c/h            - Probing operations
✅ coolant_control.c/h  - Coolant M-codes
✅ spindle_control.c/h  - Spindle M-codes
✅ defaults.h           - Default machine settings
✅ config.h             - GRBL configuration options

PIC32MZ Adaptations Required:
------------------------------
⚠️ stepper.c/h    → pic32_stepper.c (OCR continuous pulse mode)
⚠️ serial.c/h     → pic32_serial.c (UART2 PLIB integration)
⚠️ eeprom.c/h     → pic32_nvm.c (NVM flash storage)
⚠️ cpu_map.h      → pic32_config.h (PIC32MZ GPIO pins)
⚠️ grbl.h         → Modify for PIC32 (remove AVR headers)

Key Architecture Points:
------------------------
GRBL uses a modular callback-based architecture:
- planner.c creates motion blocks in ring buffer
- stepper.c executes blocks via ISR (WE REPLACE WITH OCR)
- protocol.c handles serial I/O and real-time commands
- gcode.c parses commands and updates modal state

Our Integration Strategy:
--------------------------
1. Keep ALL GRBL algorithms (planner, gcode, protocol)
2. Replace ONLY stepper ISR with OCR hardware control
3. Adapt serial I/O to use UART2 PLIB
4. Use NVM instead of EEPROM for settings

Next: Create PIC32MZ hardware adapters
"@
    
    $MarkerContent | Out-File -FilePath $MarkerFile -Encoding utf8
    Write-Host "Marker file created: $MarkerFile" -ForegroundColor Cyan
    Write-Host ""
}
