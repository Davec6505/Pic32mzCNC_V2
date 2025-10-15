# GRBL v1.1f Source Code Download Script
# Downloads official GRBL files to prepare for PIC32MZ port

param(
    [string]$TargetDir = ".\grbl-source",
    [string]$GrblVersion = "v1.1f.20170801"
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "GRBL v1.1f Source Download Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Create target directory
if (-not (Test-Path $TargetDir)) {
    New-Item -ItemType Directory -Path $TargetDir | Out-Null
    Write-Host "[OK] Created directory: $TargetDir" -ForegroundColor Green
}

# GRBL core source files to download
$GrblFiles = @(
    # Core motion planning
    "planner.c",
    "planner.h",
    "stepper.c",
    "stepper.h",
    
    # G-code parsing
    "gcode.c",
    "gcode.h",
    
    # Motion execution
    "motion_control.c",
    "motion_control.h",
    
    # Protocol and communication
    "protocol.c",
    "protocol.h",
    "serial.c",
    "serial.h",
    
    # System management
    "system.c",
    "system.h",
    "limits.c",
    "limits.h",
    "report.c",
    "report.h",
    
    # Settings and configuration
    "settings.c",
    "settings.h",
    "defaults.h",
    "config.h",
    
    # Utilities
    "nuts_bolts.c",
    "nuts_bolts.h",
    "print.c",
    "print.h",
    
    # Peripheral control
    "spindle_control.c",
    "spindle_control.h",
    "coolant_control.c",
    "coolant_control.h",
    "probe.c",
    "probe.h",
    "jog.c",
    "jog.h",
    
    # EEPROM (will need replacement for PIC32)
    "eeprom.c",
    "eeprom.h",
    
    # CPU mapping (AVR-specific, will need replacement)
    "cpu_map.h",
    
    # Main header
    "grbl.h",
    "main.c",
    
    # License
    "../COPYING"
)

# GitHub raw content base URL
$BaseUrl = "https://raw.githubusercontent.com/gnea/grbl/$GrblVersion/grbl"

Write-Host "Downloading GRBL v1.1f source files..." -ForegroundColor Yellow
Write-Host ""

$SuccessCount = 0
$FailCount = 0

foreach ($File in $GrblFiles) {
    $Url = "$BaseUrl/$File"
    $OutputFile = Join-Path $TargetDir (Split-Path $File -Leaf)
    
    Write-Host "  Downloading: $File" -NoNewline
    
    try {
        Invoke-WebRequest -Uri $Url -OutFile $OutputFile -ErrorAction Stop
        Write-Host " [OK]" -ForegroundColor Green
        $SuccessCount++
    }
    catch {
        Write-Host " [FAILED]" -ForegroundColor Red
        Write-Host "    Error: $($_.Exception.Message)" -ForegroundColor Red
        $FailCount++
    }
    
    Start-Sleep -Milliseconds 100  # Be nice to GitHub servers
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Download Summary:" -ForegroundColor Cyan
Write-Host "  Success: $SuccessCount files" -ForegroundColor Green
Write-Host "  Failed:  $FailCount files" -ForegroundColor Red
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

if ($SuccessCount -gt 0) {
    Write-Host "Next Steps:" -ForegroundColor Yellow
    Write-Host "  1. Review downloaded files in: $TargetDir" -ForegroundColor White
    Write-Host "  2. Copy core files to srcs/ directory" -ForegroundColor White
    Write-Host "  3. Create grbl_stepper.c for OCR hardware adaptation" -ForegroundColor White
    Write-Host "  4. Update Makefile to build GRBL files" -ForegroundColor White
    Write-Host ""
}

# Create a summary file
$SummaryFile = Join-Path $TargetDir "DOWNLOAD_SUMMARY.txt"
$SummaryContent = @"
GRBL v1.1f Source Code Download Summary
========================================
Download Date: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
GRBL Version: $GrblVersion
Files Downloaded: $SuccessCount
Files Failed: $FailCount

Files to Port to PIC32MZ:
--------------------------
COPY VERBATIM (keep GRBL algorithms):
  - planner.c/h       : Motion planning and look-ahead buffer
  - gcode.c/h         : G-code parser and modal state machine
  - protocol.c/h      : GRBL protocol and serial handling
  - motion_control.c/h: Arc generation and line motion
  - limits.c/h        : Limit switch and homing
  - settings.c/h      : EEPROM settings management
  - report.c/h        : Status reports and messages
  - nuts_bolts.c/h    : Utility functions
  - jog.c/h           : Jogging functionality

ADAPT FOR PIC32MZ (custom implementation):
  - stepper.c/h       : Replace with grbl_stepper.c (OCR hardware)
  - serial.c/h        : Adapt for UART2 peripheral
  - eeprom.c/h        : Use PIC32 NVM instead of AVR EEPROM
  - cpu_map.h         : Create pic32mz_map.h with GPIO assignments
  - config.h          : Update for PIC32MZ capabilities

NOT NEEDED (already have):
  - spindle_control.c/h: Already implemented in app.c
  - coolant_control.c/h: Can add later if needed
  - probe.c/h          : Can add later if needed

Key Architecture Points:
------------------------
1. GRBL uses 16-block circular buffer (BLOCK_BUFFER_SIZE=16)
2. Planner calculates junction velocities and acceleration profiles
3. Stepper ISR uses Bresenham algorithm - WE REPLACE WITH OCR HARDWARE
4. Character-counting serial protocol with 128-byte RX buffer
5. Real-time command override system (feed hold, cycle start, reset)

PIC32MZ Integration Strategy:
------------------------------
PHASE 1: Copy planner.c, gcode.c verbatim to srcs/grbl_*.c
PHASE 2: Create grbl_stepper.c that implements GRBL stepper.h interface
PHASE 3: Adapt serial.c for UART2, eeprom.c for NVM
PHASE 4: Wire up protocol.c to main loop
PHASE 5: Test and tune

Critical Difference:
--------------------
GRBL AVR:  Timer ISR → Bresenham step calculation → GPIO toggle
PIC32MZ:   Calculate OCR period → Hardware generates pulses automatically

Our Advantage:
--------------
Offload step generation to OCR hardware = more CPU time for planning!
"@

$SummaryContent | Out-File -FilePath $SummaryFile -Encoding utf8
Write-Host "Summary saved to: $SummaryFile" -ForegroundColor Cyan
Write-Host ""
