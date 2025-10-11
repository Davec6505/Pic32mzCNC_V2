# Pick-and-Place Mode with Limit Switch Masking

## Overview

The PIC32MZ CNC firmware now supports both traditional CNC milling operations and pick-and-place assembly operations. The key innovation is a **limit switch masking system** that allows selective disabling of specific limit switches for pick-and-place operations while maintaining safety for normal CNC work.

## Why Limit Switch Masking is Critical for Pick-and-Place

In traditional CNC operation, limit switches prevent the tool from traveling beyond safe mechanical limits. However, pick-and-place operations require different behavior:

- **Spring-loaded nozzles** must be able to compress against components
- **Z-axis needs to move below normal minimum limit** to apply proper pressure
- **Precise component placement** requires controlled compression force
- **Safety must be maintained** for X/Y axes and maximum limits

## System Architecture

### Limit Mask Types
```c
typedef enum {
    LIMIT_MASK_NONE     = 0x00,     // No limits masked (normal CNC mode)
    LIMIT_MASK_X_MIN    = 0x01,     // Mask X minimum limit
    LIMIT_MASK_X_MAX    = 0x02,     // Mask X maximum limit
    LIMIT_MASK_Y_MIN    = 0x04,     // Mask Y minimum limit
    LIMIT_MASK_Y_MAX    = 0x08,     // Mask Y maximum limit
    LIMIT_MASK_Z_MIN    = 0x10,     // Mask Z minimum limit (CRITICAL for PnP)
    LIMIT_MASK_Z_MAX    = 0x20,     // Mask Z maximum limit
    LIMIT_MASK_A_MIN    = 0x40,     // Mask A minimum limit
    LIMIT_MASK_A_MAX    = 0x80,     // Mask A maximum limit
    LIMIT_MASK_ALL      = 0xFF      // Mask all limits (DANGEROUS!)
} limit_mask_t;
```

### Safety Features
- **Runtime masking control** - Enable/disable masks on demand
- **Status verification** - Check which limits are currently masked
- **Gradual masking** - Enable specific masks individually
- **Emergency override** - All masks can be instantly cleared
- **Mask status reporting** - Debug output shows active masks

## Application Programming Interface

### High-Level Functions (Recommended)
```c
// Enable pick-and-place mode (masks Z minimum limit)
APP_SetPickAndPlaceMode(true);

// Disable pick-and-place mode (restore all limits)  
APP_SetPickAndPlaceMode(false);

// Check current mode
bool isPnP = APP_IsPickAndPlaceMode();

// Quick Z-min control
APP_EnableZMinMask();   // Mask Z minimum only
APP_DisableZMinMask();  // Restore Z minimum
```

### Low-Level Functions (Advanced)
```c
// Set specific mask combination
INTERP_SetLimitMask(LIMIT_MASK_Z_MIN | LIMIT_MASK_Y_MAX);

// Enable additional masks
INTERP_EnableLimitMask(LIMIT_MASK_X_MIN);

// Disable specific masks
INTERP_DisableLimitMask(LIMIT_MASK_Z_MIN);

// Check if specific limit is masked
bool isMasked = INTERP_IsLimitMasked(AXIS_Z, false); // Check Z minimum
```

## Typical Pick-and-Place Workflow

### 1. Setup Phase
```c
// Switch to pick-and-place mode
APP_SetPickAndPlaceMode(true);
// Output: "*** PICK-AND-PLACE MODE ENABLED ***"
// Output: "Z minimum limit masked for spring-loaded nozzle operation"
```

### 2. Component Pickup
```c
// Move to component location (normal XY movement)
// Z-axis can now move below normal minimum limit for pickup
// Spring-loaded nozzle compresses against component
```

### 3. Component Placement  
```c
// Move to placement location
// Z-axis presses component into place with controlled force
// Spring compression provides tactile feedback
```

### 4. Return to CNC Mode
```c
// Restore normal CNC operation
APP_SetPickAndPlaceMode(false);
// Output: "*** CNC MODE ENABLED ***"
// Output: "All limit switches active for normal CNC operation"
```

## Hardware Integration

### Current Limit Switch Configuration
- **X-min**: `LIMIT_X_PIN` (GPIO_PIN_RB1) - Active
- **Y-min**: `LIMIT_Y_PIN` (GPIO_PIN_RB15) - Active
- **Z-min**: `LIMIT_Z_PIN` (GPIO_PIN_RF4) - Active (maskable for PnP)
- **X-max**: Not configured (ready for future MCC setup)
- **Y-max**: Not configured (ready for future MCC setup)  
- **Z-max**: Not configured (ready for future MCC setup)

### Spring-Loaded Nozzle Requirements
- **Mechanical design**: Nozzle must compress against Z-min switch
- **Spring constant**: Appropriate force for component handling
- **Travel distance**: Sufficient compression range below switch trigger point
- **Feedback**: Optional force sensing for precise control

## Safety Considerations

### What Gets Masked
- ✅ **Z minimum limit** - Allows nozzle compression
- ❌ **X/Y limits** - Maintained for safety (unless specifically needed)
- ❌ **Maximum limits** - Maintained for safety

### Safety Mechanisms
- **Mask status logging** - All mask changes are logged to UART
- **Emergency stop** - Instantly clears all masks and stops motion
- **Alarm reset verification** - Cannot reset alarms while masks are active improperly
- **Selective masking** - Only mask what's necessary for the operation

### Emergency Procedures
```c
// Emergency stop (clears all masks automatically)
APP_EmergencyStop();

// Force clear all masks
INTERP_SetLimitMask(LIMIT_MASK_NONE);

// Check system status
limit_mask_t currentMask = INTERP_GetLimitMask();
```

## Debug and Monitoring

### UART Output Examples
```
LIMIT MASK: Set to 0x10
*** PICK-AND-PLACE MODE ENABLED ***
Z minimum limit masked for spring-loaded nozzle operation

LIMIT MASK: Set to 0x00  
*** CNC MODE ENABLED ***
All limit switches active for normal CNC operation
```

### Status Checking
```c
// Check if system is ready for pick-and-place
if (APP_IsPickAndPlaceMode()) {
    printf("Ready for component handling\n");
} else {
    printf("In CNC mode - limits fully active\n");
}
```

## Integration with GRBL Commands

The masking system integrates seamlessly with existing GRBL commands:
- **G-codes**: Work normally with masked limits
- **Homing ($H)**: Uses unmasked limits for reference
- **Emergency stop**: Automatically clears all masks
- **Status reporting**: Includes mask status in system state

## Future Enhancements

### Planned Features
- **Force feedback integration** - Monitor spring compression force
- **Automatic mask management** - Smart masking based on operation mode
- **Component library** - Pre-configured settings for different component types
- **Vision system integration** - Automatic alignment with masked limits

### Hardware Expansion
- **Additional limit switches** - Configure max limits via MCC
- **Force sensors** - Precise component placement pressure
- **Vacuum control** - Integrated pick-and-place tool control
- **Rotary axis support** - Component orientation control

This system provides the foundation for professional pick-and-place operations while maintaining the safety and precision required for CNC machining operations.