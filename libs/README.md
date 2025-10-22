# Shared Library Directory

This directory contains:
1. **Source files** (`.c`) that can be built as a shared library
2. **Library archives** (`.a`) - pre-built static libraries

## Purpose

Place specific `.c` files here that you want to build as a **separate shared library** instead of compiling them directly into the main executable every time.

## Building the Shared Library

To build a library from all `.c` files in this folder:

```bash
make shared_lib
```

This will create `libCS23shared.a` in this directory from all `libs/*.c` files.

## Using the Library

### Option 1: Compile Directly (Default)

```bash
make all
```

All sources (including `libs/*.c`) are compiled directly into the executable.

### Option 2: Link Against Pre-Built Library

```bash
# First, build the shared library
make shared_lib

# Then, build executable linked against the library
make all USE_SHARED_LIB=1
```

The executable will link against `libCS23shared.a` instead of recompiling those sources.

## What Gets Built

When building as a library, ONLY the `.c` files placed in this `libs/` directory are compiled and archived:

Example: If you place `motion_math.c` in this folder, only that file gets built into the library.

**Typical use cases:**
- Math libraries that rarely change
- Algorithm implementations (S-curve calculations, kinematics)
- Hardware abstraction layers (HAL)
- Utility functions shared across projects
- Third-party code that's stable and doesn't need frequent recompilation

## Workflow Example

### Example: Moving `motion_math.c` to Shared Library

```bash
# Step 1: Copy source file to libs/
cp srcs/motion/motion_math.c libs/

# Step 2: Build the shared library
make shared_lib
# Output: libs/libCS23shared.a (contains motion_math.o)

# Step 3: Build main executable (now excludes motion_math.c from compilation)
make all USE_SHARED_LIB=1
# Output: bins/CS23.elf (linked against libCS23shared.a)
```

### When to Use Each Method

**Compile Directly** (`make all`):
- ✅ Development phase (frequent code changes)
- ✅ Debugging (easier to step through source)
- ✅ Single project build

**Use Shared Library** (`make all USE_SHARED_LIB=1`):
- ✅ Stable algorithm code (rarely changes)
- ✅ Faster incremental builds (library doesn't recompile)
- ✅ Multiple projects sharing same code
- ✅ Distribution (provide .a + headers to users)

## Build Artifacts

- **Source**: `libs/*.c` (your shared library sources)
- **Objects**: `objs/libs/*.o` (compiled object files)
- **Library**: `libs/libCS23shared.a` (archive of object files)
- **Headers**: Place corresponding `.h` files in `incs/` as usual

## Cleaning Library Builds

```bash
# Clean all build outputs (including library)
make clean
```

## Benefits of Shared Library Approach

1. **Faster Incremental Builds**: Library code doesn't recompile unless `.c` files in `libs/` change
2. **Code Organization**: Separate stable algorithms from frequently-changing application code
3. **Reduced Link Time**: Pre-compiled objects link faster than full recompilation
4. **Modularity**: Clear separation between library and application layers
5. **Distribution**: Can provide `.a` + headers to users without source code

## Linker Behavior

When `USE_SHARED_LIB=1` is set:

```makefile
# Makefile automatically adds:
-L"libs"           # Search for libraries in libs/ folder
-lCS23shared       # Link against libCS23shared.a
```

The linker will pull only the object files from the library that are actually referenced by your code (standard Unix ar behavior).

## Architecture Benefits

This library architecture demonstrates:
- ✅ **Hardware-accelerated pulse generation** (OCR modules, not ISR-driven)
- ✅ **Junction velocity optimization** (GRBL planner with look-ahead)
- ✅ **Jerk-limited S-curve profiles** (smooth acceleration transitions)
- ✅ **Time-based interpolation** (not Bresenham step counting)
- ✅ **Modular design** (motion, parsing, and communication separated)

**The Breakthrough**: First open-source CNC controller combining hardware acceleration with continuous junction motion!
