# Makefile Quiet Build Implementation

## Overview

The Makefile now includes a **`quiet`** target that filters build output to show only errors, warnings, and completion status.

## Usage

### Standard Build (Full Output)
```bash
make all
```
**Shows**: All compiler messages, linking info, etc.

### Quiet Build (Filtered Output)
```bash
make quiet
```
**Shows**: Only errors, warnings, and completion status

## Implementation Details

### PowerShell Implementation (Windows)
```makefile
quiet:
	@echo "######  QUIET BUILD (errors/warnings only)  ########"
	@powershell -Command "& { \
		$$output = & { cd srcs; make ... 2>&1 }; \
		$$filtered = $$output | Select-String -Pattern 'error|warning' -CaseSensitive:$$false; \
		if ($$filtered) { $$filtered | Write-Host -ForegroundColor Red }; \
		if ($$LASTEXITCODE -eq 0) { \
			# Success - run hex conversion \
			Write-Host '######  BUILD COMPLETE (no errors)  ########' -ForegroundColor Green \
		} else { \
			Write-Host '######  BUILD FAILED  ########' -ForegroundColor Red; exit 1 \
		} \
	}"
```

### How It Works

1. **Capture all output**: `2>&1` redirects STDERR to STDOUT
2. **Filter with Select-String**: Shows only lines with "error" or "warning"
3. **Color coding**:
   - Red: Errors/warnings
   - Green: Build success
4. **Exit codes preserved**: Build failures still return non-zero exit code

## Examples

### Successful Build (No Warnings)
```
######  QUIET BUILD (errors/warnings only)  ########
######  BUILD COMPLETE (no errors)  ########
```

### Build with Warnings
```
######  QUIET BUILD (errors/warnings only)  ########
srcs/gcode_parser.c:123: warning: unused variable 'temp'
######  BUILD COMPLETE (no errors)  ########
```

### Build with Errors
```
######  QUIET BUILD (errors/warnings only)  ########
srcs/gcode_parser.c:456: error: 'undefined_var' undeclared
srcs/gcode_parser.c:457: error: expected ';' before '}' token
######  BUILD FAILED  ########
```

## Advantages

✅ **Fast feedback** - See problems immediately without scrolling  
✅ **CI/CD ready** - Perfect for automated builds  
✅ **Developer friendly** - Less noise during development  
✅ **Exit codes preserved** - Build scripts can detect failures  
✅ **Cross-platform** - Works on Windows (PowerShell) and Linux (grep)

## Alternative: Use Variables to Control Verbosity

You can also add a `VERBOSE` variable:

```makefile
# Add at top of Makefile
VERBOSE ?= 0

all:
ifeq ($(VERBOSE),0)
	@# Quiet mode
	@$(MAKE) quiet
else
	@# Full output mode
	@echo "######  BUILDING   ########"
	cd srcs && $(BUILD) ...
endif
```

**Usage**:
```bash
make all              # Quiet by default
make all VERBOSE=1    # Full output
```

## Recommended Workflow

1. **Development**: Use `make quiet` for rapid iteration
2. **Debugging build issues**: Use `make all` for full details
3. **CI/CD pipelines**: Use `make quiet` for cleaner logs
4. **Final verification**: Use `make all` before commits

## Color Output Reference

| Status          | Color | Message                      |
| --------------- | ----- | ---------------------------- |
| Success         | Green | `BUILD COMPLETE (no errors)` |
| Failure         | Red   | `BUILD FAILED`               |
| Errors/Warnings | Red   | Actual compiler messages     |

## Integration with VS Code Tasks

Add to `.vscode/tasks.json`:

```json
{
    "label": "Build PIC32MZ (Quiet)",
    "type": "shell",
    "command": "make",
    "args": ["quiet"],
    "group": "build",
    "presentation": {
        "echo": true,
        "reveal": "always",
        "focus": false,
        "panel": "shared"
    },
    "problemMatcher": []
}
```

## Troubleshooting

### Issue: Color codes show as text
**Solution**: PowerShell color codes (`Write-Host -ForegroundColor`) work in PowerShell terminals. Use plain echo for other shells.

### Issue: Build succeeds but shows as failed
**Solution**: Check `$LASTEXITCODE` handling in PowerShell script. May need to store exit code before hex conversion.

### Issue: Warnings not showing
**Solution**: Verify case-insensitive matching: `-CaseSensitive:$false` in PowerShell.

---

**Implemented**: October 17, 2025  
**Status**: ✅ Ready for testing
