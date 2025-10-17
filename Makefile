# Simple Makefile for PIC32MZ project
# Name of the project binary
MODULE     := CS23

# Device configuration
# The device is expected to be a PIC32MZ family device.
DEVICE     := 32MZ2048EFH100

# Memory configuration for dynamic allocation
# These control heap and stack sizes for the PIC32MZ application
HEAP_SIZE  := 20480    # 20KB heap for dynamic memory allocation
STACK_SIZE := 20480    # 20KB stack for function calls and local variables

# Compiler location and DFP (Device Family Pack) location
# The compiler location is expected to be the path to the xc32-gcc compiler.
# The DFP location is expected to be the path to the Microchip Device Family Pack.
# The DFP is used to provide the necessary header files and libraries for the specific device.
# The DFP is expected to be installed in the MPLAB X IDE installation directory.
# The DFP is expected to be in the packs directory of the MPLAB X IDE installation directory.
# The DFP is expected to be in the format of Microchip/PIC32MZ-EF_DFP/1.4.168.
# Cross-platform compiler and DFP paths
ifeq ($(OS),Windows_NT)
    COMPILER_LOCATION := C:/Program Files/Microchip/xc32/v4.60/bin
#	DFP_LOCATION := C:/Users/Automation/.mchp_packs
    DFP_LOCATION := C:/Program Files/Microchip/MPLABX/v6.25/packs
else
    COMPILER_LOCATION := /opt/microchip/xc32/v4.60/bin
    DFP_LOCATION := /opt/microchip/mplabx/v6.25/packs
endif
DFP := $(DFP_LOCATION)/Microchip/PIC32MZ-EF_DFP/1.4.168
#DFP := $(DFP_LOCATION)/Microchip/PIC32MZ-EF_DFP/1.5.173
#C:/Users/Automation/.mchp_packs/Microchip/PIC32MZ-EF_DFP/1.5.173

# Simple Unix-style build system
BUILD=make
CLEAN=make clean DRY_RUN=$(DRY_RUN)
BUILD_DIR=make build_dir

all:
	@echo "######  BUILDING   ########"
	cd srcs && $(BUILD) COMPILER_LOCATION="$(COMPILER_LOCATION)" DFP_LOCATION="$(DFP_LOCATION)" DFP="$(DFP)" DEVICE=$(DEVICE) MODULE=$(MODULE) HEAP_SIZE=$(HEAP_SIZE) STACK_SIZE=$(STACK_SIZE)
	@echo "###### BIN TO HEX ########"
	cd bins && "$(COMPILER_LOCATION)/xc32-bin2hex" $(MODULE)
	@echo "######  BUILD COMPLETE   ########"

# Quiet build - shows only errors, warnings, and completion status
quiet:
	@echo "######  QUIET BUILD (errors/warnings only)  ########"
ifeq ($(OS),Windows_NT)
	@powershell -NoProfile -ExecutionPolicy Bypass -Command \
	"\$$output = & { Push-Location srcs; make COMPILER_LOCATION='$(COMPILER_LOCATION)' DFP_LOCATION='$(DFP_LOCATION)' DFP='$(DFP)' DEVICE=$(DEVICE) MODULE=$(MODULE) HEAP_SIZE=$(HEAP_SIZE) STACK_SIZE=$(STACK_SIZE) 2>&1; Pop-Location }; \
	\$$filtered = \$$output | Select-String -Pattern 'error|warning' -CaseSensitive:\$$false; \
	if (\$$filtered) { \$$filtered | Write-Host -ForegroundColor Red }; \
	if (\$$LASTEXITCODE -eq 0) { \
		cd bins; & '$(COMPILER_LOCATION)/xc32-bin2hex' $(MODULE) | Out-Null; \
		if (\$$LASTEXITCODE -eq 0) { Write-Host '######  BUILD COMPLETE (no errors)  ########' -ForegroundColor Green } \
		else { Write-Host '######  HEX CONVERSION FAILED  ########' -ForegroundColor Red; exit 1 } \
	} else { Write-Host '######  BUILD FAILED  ########' -ForegroundColor Red; exit 1 }"
else
	@cd srcs && $(BUILD) COMPILER_LOCATION="$(COMPILER_LOCATION)" DFP_LOCATION="$(DFP_LOCATION)" DFP="$(DFP)" DEVICE=$(DEVICE) MODULE=$(MODULE) HEAP_SIZE=$(HEAP_SIZE) STACK_SIZE=$(STACK_SIZE) 2>&1 | grep -iE 'error|warning' || echo "No errors or warnings"
	@if [ $$? -eq 0 ]; then cd bins && "$(COMPILER_LOCATION)/xc32-bin2hex" $(MODULE) >/dev/null 2>&1 && echo "######  BUILD COMPLETE  ########"; fi
endif

build_dir:
	@echo "###### BUILDING DIRECTORIES FOR OUTPUT BINARIES #######"
	cd srcs && $(BUILD_DIR)
	@echo "############ BUILDING DIRECTORIES COMPLETED ###########"

debug:
	@echo "####### DEBUGGING OUTPUTS #######"
	cd srcs && $(BUILD) debug COMPILER_LOCATION="$(COMPILER_LOCATION)" DFP_LOCATION="$(DFP_LOCATION)" DFP="$(DFP)" DEVICE=$(DEVICE) MODULE=$(MODULE) HEAP_SIZE=$(HEAP_SIZE) STACK_SIZE=$(STACK_SIZE)

platform:
	@echo "####### PLATFORM INFO #######"
	cd srcs && $(BUILD) platform COMPILER_LOCATION="$(COMPILER_LOCATION)" DFP_LOCATION="$(DFP_LOCATION)" DFP="$(DFP)" DEVICE=$(DEVICE) MODULE=$(MODULE)

clean:
	@echo "####### CLEANING OUTPUTS #######"
	cd srcs && $(CLEAN)


rem_dir:
	@echo "####### REMOVING BUILD DIRECTORIES #######"
	cd srcs && $(BUILD) rem_dir DRY_RUN=$(DRY_RUN) DEL_PATH=$(DIR_PATH)

mk_dir:
	@echo "####### CREATING BUILD DIRECTORIES #######"
	cd srcs && $(BUILD) mk_dir DIR_PATH=$(DIR_PATH)

install:
	cd srcs && $(BUILD) install

flash:
	@echo "#######LOADING OUTPUTS#######"
	cd bins && sudo ../../MikroC_bootloader_lnx/bins/mikro_hb $(MODULE).hex
	@echo "#######LOAD COMPLETE#######"

dfp_dir:
	@echo "####### DFP DIRECTORY #######"
	@echo $(DFP)

debug_path:
	@echo "####### DEBUGGING PATHS #######"
	cd srcs && $(BUILD) debug_path $(DIR_PATH)



# Unix-style utility targets (cross-platform)
find-source:
	@echo "####### FINDING SOURCE FILES #######"
ifeq ($(OS),Windows_NT)
	@powershell -Command "Get-ChildItem -Recurse srcs -Include *.c,*.h | Select-Object -ExpandProperty FullName"
else
	@find srcs -name "*.c" -o -name "*.h"
endif

grep-pattern:
	@echo "####### SEARCHING FOR PATTERN (usage: make grep-pattern PATTERN=your_pattern) #######"
ifeq ($(OS),Windows_NT)
	@powershell -Command "Select-String -Pattern '$(PATTERN)' -Path 'srcs\*' -Recurse || Write-Host 'No matches found'"
else
	@grep -r "$(PATTERN)" srcs/ || echo "No matches found"
endif

list-files:
	@echo "####### LISTING PROJECT FILES #######"
ifeq ($(OS),Windows_NT)
	@dir /b
else
	@ls -la
endif

help: cmdlets


HELP_ENTRIES = \
    make build_dir            | Start by testing build directories output (DRY_RUN=1 default). ; \
    make build_dir DRY_RUN=0  | Create build directories (set DRY_RUN=1 to simulate). ; \
    make all                  | Build the project (full output). ; \
    make quiet                | Build with filtered output (errors/warnings only). ; \
    make clean                | Clean build outputs. ; \
    make platform             | Show platform information. ; \
    make rem_dir DIR_PATH=    | Remove specified directory (DIR_PATH=""). ; \
    make mk_dir DIR_PATH=     | Create specified directory (DIR_PATH=""). ; \
    make dfp_dir              | Show the DFP directory. ; \
    make debug_path DIR_PATH= | Debug specified path (DIR_PATH=""). ; \
    make help                 | Show this help message.

cmdlets:
ifeq ($(OS),Windows_NT)
	@powershell -NoProfile -Command "$$raw = '$(HELP_ENTRIES)'; \
	$$entries = $$raw -split ';'; \
	Write-Host '######################################## BUILD COMMANDS ############################################' -ForegroundColor Green; \
	foreach ($$entry in $$entries) { \
        if ($$entry.Trim() -ne '') { \
            $$parts = $$entry -split '\|'; \
            $$cmd = $$parts[0].Trim(); \
            $$desc = $$parts[1].Trim(); \
            $$cmdPadded = $$cmd.PadRight(30); \
            Write-Host $$cmdPadded -ForegroundColor Yellow -NoNewline; \
            Write-Host ' - ' $$desc -ForegroundColor White; \
        } \
	}; \
	Write-Host '#####################################################################################################' -ForegroundColor Green"
else
	@echo -e "$(GREEN)######################################## BUILD COMMANDS ############################################$(NC)"
	@for entry in $(HELP_ENTRIES); do \
        cmd=$$(echo $$entry | cut -d'|' -f1 | xargs); \
        desc=$$(echo $$entry | cut -d'|' -f2- | xargs); \
        printf "$(Yellow)%-30s$(NC) - %s\n" "$$cmd" "$$desc"; \
	done
	@echo -e "$(GREEN)#####################################################################################################$(NC)"
endif



.PHONY: all quiet build_dir clean install find-source grep-pattern list-files debug platform cmdlets 
