# Limit Switch Pin Configuration

## Current Hardware Configuration (MCC Generated)

### Minimum Limit Switches (Active)
These pins are currently configured in your MCC project and connected to physical switches:

- **LIMIT_X_PIN** = GPIO_PIN_RB1  - X-axis minimum limit switch (also used for homing)
- **LIMIT_Y_PIN** = GPIO_PIN_RB15 - Y-axis minimum limit switch (also used for homing)  
- **LIMIT_Z_PIN** = GPIO_PIN_RF4  - Z-axis minimum limit switch (also used for homing)

### Maximum Limit Switches (To Be Configured)
These pins need to be configured in MCC when you want to add maximum limit switches:

- **LIMIT_X_MAX_PIN** = *Not yet configured* - X-axis maximum limit switch
- **LIMIT_Y_MAX_PIN** = *Not yet configured* - Y-axis maximum limit switch
- **LIMIT_Z_MAX_PIN** = *Not yet configured* - Z-axis maximum limit switch

## Current Functionality

### Homing Operation
- Homing cycle moves toward **negative direction** (toward min switches)
- Uses the configured min switches for home position detection
- 3-phase homing: SEEK → LOCATE → PULLOFF

### Safety Limits
- **Min switches**: Active and functional - will stop motion when triggered
- **Max switches**: Currently dummy (always return false) - no hardware limit

### Code Locations
- **Limit switch reading**: `interpolation_engine.c` - `is_limit_switch_triggered()`
- **Safety monitoring**: `app.c` - `APP_ProcessLimitSwitches()`
- **Alarm reset**: `app.c` - `APP_AlarmReset()`

## To Add Maximum Limit Switches

1. **Configure in MCC:**
   - Add 3 more GPIO input pins in MPLABX MCC
   - Name them: `LIMIT_X_MAX_PIN`, `LIMIT_Y_MAX_PIN`, `LIMIT_Z_MAX_PIN`
   - Regenerate code

2. **Update code:**
   - Replace `return false;` lines in `is_limit_switch_triggered()` 
   - Replace `bool x_max_limit = false;` lines in `APP_ProcessLimitSwitches()`
   - Replace `bool x_max_limit = false;` lines in `APP_AlarmReset()`

3. **Hardware:**
   - Connect physical limit switches to the new GPIO pins
   - Wire as normally-open, active-low (same as current switches)

## Switch Wiring
- **Active Low**: Switch closes to ground when triggered
- **Pull-up resistors**: Enabled in MCC GPIO configuration
- **Debouncing**: Handled in software (5ms debounce period)