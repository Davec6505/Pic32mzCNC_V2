# Multi-Configuration Build System

**Date**: October 22, 2025

## Overview

The build system now supports three build configurations with separate output directories:
- **Default** - Balanced optimization (-g -O1)
- **Debug** - Full debug symbols (-g3 -O0)
- **Release** - Aggressive optimization (-O3)

## Directory Structure

```
Pic32mzCNC_V2/
├── bins/
│   ├── Default/        # Default builds (-g -O1)
│   │   ├── CS23.elf
│   │   └── CS23.hex
│   ├── Debug/          # Debug builds (-g3 -O0)
│   │   ├── CS23.elf
│   │   └── CS23.hex
│   └── Release/        # Release builds (-O3)
│       ├── CS23.elf
│       └── CS23.hex
├── libs/
│   ├── Default/        # Default library builds
│   │   └── libCS23shared.a
│   ├── Debug/          # Debug library builds
│   │   └── libCS23shared.a
│   ├── Release/        # Release library builds
│   │   └── libCS23shared.a
│   ├── example_shared_lib.c  # Library source files
│   └── README.md
├── objs/
│   ├── Default/        # Default object files
│   ├── Debug/          # Debug object files
│   └── Release/        # Release object files
└── other/
    ├── Default/        # Default map files
    ├── Debug/          # Debug map files
    └── Release/        # Release map files
```

## Build Configurations

### Default Configuration (Balanced)

```bash
make all
# or explicitly:
make all BUILD_CONFIG=Default
```

**Compiler Flags:**
- Optimization: `-O1` (balanced speed/size)
- Debug info: `-g` (basic debug symbols)
- Defines: `-DXPRJ_Default=Default`

**Use Case:**
- Daily development
- Normal testing
- Good balance between debug capability and performance

### Debug Configuration

```bash
make all BUILD_CONFIG=Debug
```

**Compiler Flags:**
- Optimization: `-O0` (none - easier debugging)
- Debug info: `-g3` (maximum debug information)
- Defines: `-DDEBUG -DXPRJ_Debug=Debug -DDEBUG_MOTION_BUFFER`

**Use Case:**
- Step-through debugging with MPLAB X IDE
- Variable inspection
- Understanding code flow
- Finding hard-to-reproduce bugs

### Release Configuration

```bash
make all BUILD_CONFIG=Release
```

**Compiler Flags:**
- Optimization: `-O3` (maximum speed)
- Debug info: None
- Defines: `-DNDEBUG -DXPRJ_Release=Release`

**Use Case:**
- Final production firmware
- Performance testing
- Field deployment
- Smallest/fastest code

## Configuration Comparison

| Feature | Default | Debug | Release |
|---------|---------|-------|---------|
| Optimization | -O1 | -O0 | -O3 |
| Debug Info | -g | -g3 | None |
| DEBUG define | No | Yes | No |
| NDEBUG define | No | No | Yes |
| Code Size | Medium | Largest | Smallest |
| Execution Speed | Medium | Slowest | Fastest |
| Debuggability | Good | Excellent | Poor |
| Build Time | Medium | Fast | Slow |

## Library Builds with Configurations

### Build Library for Specific Configuration

```bash
# Build Debug library
make shared_lib BUILD_CONFIG=Debug

# Build Release library  
make shared_lib BUILD_CONFIG=Release
```

### Use Configuration-Specific Library

```bash
# Build Debug executable using Debug library
make shared_lib BUILD_CONFIG=Debug
make all BUILD_CONFIG=Debug USE_SHARED_LIB=1

# Build Release executable using Release library
make shared_lib BUILD_CONFIG=Release
make all BUILD_CONFIG=Release USE_SHARED_LIB=1
```

## Configuration Defines in Code

Your code can check which configuration is active:

```c
// Check for Debug build
#ifdef DEBUG
    // Debug-only code (e.g., extra logging)
    printf("Debug: Variable x = %d\n", x);
#endif

// Check for Release build
#ifdef NDEBUG
    // Release optimizations (assertions disabled)
#endif

// Check for specific XPRJ configuration
#if defined(XPRJ_Debug)
    // Debug configuration specific code
#elif defined(XPRJ_Release)
    // Release configuration specific code
#elif defined(XPRJ_Default)
    // Default configuration specific code
#endif
```

## Output File Naming

All configurations produce the same filename (`CS23.elf`, `CS23.hex`) but in different directories:

```
bins/Default/CS23.hex   - Default build
bins/Debug/CS23.hex     - Debug build
bins/Release/CS23.hex   - Release build
```

This allows easy identification while maintaining standard filenames for programming tools.

## Map Files

Map files are also separated by configuration:

```
other/Default/Default.map    - Default build map
other/Debug/Debug.map        - Debug build map
other/Release/Release.map    - Release build map
```

## Cleaning Builds

```bash
# Clean all configurations
make clean

# Clean specific configuration
make clean BUILD_CONFIG=Debug
```

## Parallel Development Workflow

The multi-configuration system allows parallel development:

```bash
# Developer 1: Work on Debug build
make all BUILD_CONFIG=Debug
# Test in MPLAB X debugger

# Developer 2: Test Release performance (doesn't interfere!)
make all BUILD_CONFIG=Release
# Benchmark on hardware

# Developer 3: Normal development (independent!)
make all
# Standard workflow
```

## MPLAB X IDE Integration

The configurations match MPLAB X IDE standards:

- **XPRJ_Default** - Standard MPLAB X configuration
- **XPRJ_Debug** - MPLAB X debug configuration
- **XPRJ_Release** - MPLAB X release configuration

This allows seamless integration between Makefile builds and MPLAB X IDE builds.

## Build Command Summary

```bash
# Default builds
make all                                    # Default executable
make shared_lib                             # Default library
make all USE_SHARED_LIB=1                   # Default with library

# Debug builds
make all BUILD_CONFIG=Debug                 # Debug executable
make shared_lib BUILD_CONFIG=Debug          # Debug library
make all BUILD_CONFIG=Debug USE_SHARED_LIB=1 # Debug with library

# Release builds
make all BUILD_CONFIG=Release               # Release executable
make shared_lib BUILD_CONFIG=Release        # Release library
make all BUILD_CONFIG=Release USE_SHARED_LIB=1 # Release with library
```

## Performance Testing Example

```bash
# Build all three configurations for comparison
make all BUILD_CONFIG=Default
make all BUILD_CONFIG=Debug
make all BUILD_CONFIG=Release

# Program each and measure performance
# Debug:   ~500 steps/sec (unoptimized, easy debugging)
# Default: ~2000 steps/sec (balanced)
# Release: ~5000 steps/sec (maximum performance)
```

## Benefits

1. **Isolation**: Different configurations don't interfere with each other
2. **Speed**: Debug symbols don't slow down Release builds
3. **Safety**: Can't accidentally deploy Debug firmware
4. **Testing**: Easy to compare performance across configurations
5. **Standards**: Matches MPLAB X IDE conventions
6. **Clarity**: Output location clearly identifies build type

## Migration from Old System

**Old System:**
```bash
make all          # Single output directory
make all DEBUG=1  # Overwrites previous build
```

**New System:**
```bash
make all                      # bins/Default/
make all BUILD_CONFIG=Debug   # bins/Debug/
# Both coexist! No overwrites!
```

## Recommended Workflow

### Development Phase
```bash
# Use Default for daily work
make all
# Fast builds, decent debugging capability
```

### Debug Hard Problems
```bash
# Switch to Debug when needed
make all BUILD_CONFIG=Debug
# Full debug symbols, no optimization interference
```

### Performance Validation
```bash
# Test with Release before deployment
make all BUILD_CONFIG=Release
# Verify motion timing at maximum optimization
```

### Final Deployment
```bash
# Always deploy Release builds
make all BUILD_CONFIG=Release
# Smallest, fastest code for production
```

## Summary

✅ **Implemented**: Three-configuration build system (Default/Debug/Release)  
✅ **Organized**: Separate output directories prevent conflicts  
✅ **Compatible**: Matches MPLAB X IDE standards  
✅ **Flexible**: All configurations support library builds  
✅ **Clear**: Output location indicates build type  

**Try it:**
```bash
make all BUILD_CONFIG=Debug    # Full debug symbols
make all BUILD_CONFIG=Release  # Maximum performance
make all                       # Balanced (default)
```
