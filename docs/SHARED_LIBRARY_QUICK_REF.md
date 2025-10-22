# Shared Library Build System - Quick Reference

**Date**: October 22, 2025

## Overview

The build system now supports building **specific source files** as a shared library instead of compiling everything into the main executable.

## Key Concept

- **libs/** folder contains `.c` files that CAN be built as a library
- **USE_SHARED_LIB** flag controls whether to use library or compile directly
- Only `.c` files placed in `libs/` folder are included in the library

## Directory Structure

```
Pic32mzCNC_V2/
├── libs/
│   ├── README.md                    (documentation)
│   ├── example_shared_lib.c         (example library source)
│   ├── libCS23shared.a              (built library archive)
│   └── <your_file>.c                (your library sources here)
├── incs/
│   ├── example_shared_lib.h         (library headers)
│   └── <your_file>.h                (your headers here)
├── objs/
│   └── libs/
│       └── example_shared_lib.o     (compiled library objects)
└── srcs/
    └── (main application sources)
```

## Commands

### Build Shared Library

```bash
make shared_lib
```

**What it does:**
- Compiles all `.c` files in `libs/` folder
- Creates object files in `objs/libs/`
- Archives objects into `libs/libCS23shared.a`

### Build Executable (Direct Compilation - Default)

```bash
make all
```

**What it does:**
- Compiles all sources from `srcs/` AND `libs/`
- Links everything into `bins/CS23.elf`
- **Does NOT use** the pre-built library

### Build Executable (Using Library)

```bash
make all USE_SHARED_LIB=1
```

**What it does:**
- Compiles only sources from `srcs/`
- **Excludes** `libs/*.c` from compilation
- Links against `libs/libCS23shared.a`
- Faster build if library hasn't changed!

## Complete Workflow Example

```bash
# Step 1: Place your stable code in libs/
cp srcs/motion/motion_math.c libs/
# (Corresponding .h should be in incs/)

# Step 2: Build the shared library
make shared_lib
# Output: libs/libCS23shared.a

# Step 3: Build main executable using the library
make all USE_SHARED_LIB=1
# Output: bins/CS23.elf (linked against library)

# Step 4: Convert to hex for programming
cd bins && xc32-bin2hex CS23
# (This is done automatically by 'make all')
```

## When Source Changes

### If Library Source Changes

```bash
# Rebuild the library
make shared_lib

# Rebuild executable (links against new library)
make all USE_SHARED_LIB=1
```

### If Application Source Changes (but library unchanged)

```bash
# Just rebuild executable (library doesn't recompile!)
make all USE_SHARED_LIB=1
# This is FASTER because libs/*.c aren't recompiled
```

## Makefile Variables

| Variable | Default | Purpose |
|----------|---------|---------|
| `USE_SHARED_LIB` | 0 | Set to 1 to link against pre-built library |
| `LIB_DIR` | ../libs | Directory containing library sources |
| `LIB_SRCS` | libs/*.c | All .c files in libs/ folder |
| `LIB_OBJS` | objs/libs/*.o | Compiled library object files |
| `LINK_LIBS` | -L"libs" -lCS23shared | Linker flags when USE_SHARED_LIB=1 |

## Build System Logic

```makefile
ifeq ($(USE_SHARED_LIB),1)
    # Link against library
    SRCS = srcs/**/*.c (only from srcs/)
    LINK_LIBS = -L"libs" -lCS23shared
else
    # Compile everything
    SRCS = srcs/**/*.c + libs/*.c
    LINK_LIBS = (empty)
endif
```

## Benefits

### Direct Compilation (Default)
- ✅ Simple workflow (no extra steps)
- ✅ Everything built fresh each time
- ✅ No library/source sync issues
- ❌ Slower incremental builds

### Library Compilation
- ✅ **Much faster** incremental builds
- ✅ Stable code doesn't recompile unnecessarily
- ✅ Clear separation of concerns
- ✅ Can distribute library without source
- ❌ Requires two-step build process

## Example: Motion Math Library

```bash
# Scenario: motion_math.c is stable, rarely changes

# Initial setup
cp srcs/motion/motion_math.c libs/
make shared_lib
# Output: libs/libCS23shared.a (contains motion_math.o)

# Daily development (editing main.c, gcode_parser.c, etc.)
make all USE_SHARED_LIB=1
# Fast! Only recompiles changed files, links against library

# If motion_math.c needs update
vi libs/motion_math.c
make shared_lib                  # Rebuild library
make all USE_SHARED_LIB=1        # Rebuild executable
```

## Debugging

### Check What's in Library

```bash
# Windows
xc32-ar -t libs/libCS23shared.a

# Linux/macOS
xc32-ar -t libs/libCS23shared.a
```

**Output:**
```
example_shared_lib.o
motion_math.o
```

### List Symbols in Library

```bash
xc32-nm -s libs/libCS23shared.a
```

**Output:**
```
example_shared_lib.o:
00000000 T SharedLib_ExampleFunction
00000010 T SharedLib_Add
00000020 T SharedLib_Initialize
```

### Verify Library is Being Used

```bash
# Build with verbose output
cd srcs && make VERBOSE=1 USE_SHARED_LIB=1
# Look for: -L"../libs" -lCS23shared in link command
```

## Common Patterns

### Pattern 1: Math Library
```bash
libs/
  ├── kinematics.c      # Coordinate transformations
  ├── interpolation.c   # S-curve calculations
  └── filters.c         # Signal processing
```

### Pattern 2: HAL Library
```bash
libs/
  ├── gpio_hal.c        # GPIO abstraction
  ├── timer_hal.c       # Timer abstraction
  └── uart_hal.c        # UART abstraction
```

### Pattern 3: Algorithm Library
```bash
libs/
  ├── planner.c         # Motion planner
  ├── scurve.c          # Trajectory generation
  └── bresenham.c       # Line algorithm
```

## Cleaning

```bash
# Clean everything (including library)
make clean

# Clean just library artifacts
rm -f libs/libCS23shared.a
rm -f objs/libs/*.o
```

## Summary

✅ **Verified**: Build system now supports selective library compilation  
✅ **Verified**: Only `.c` files in `libs/` folder are built into library  
✅ **Verified**: `USE_SHARED_LIB` flag controls direct vs library compilation  
✅ **Example**: Provided `example_shared_lib.c` for testing  

**Try it:**
```bash
make shared_lib              # Build example library
make all USE_SHARED_LIB=1    # Use library
make all                     # Or compile directly (default)
```
