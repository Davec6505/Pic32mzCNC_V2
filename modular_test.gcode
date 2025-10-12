; Modular Motion Control Test Program
; Tests all aspects of the new modular architecture
; Safe for simulation mode testing

; Initialize and home
G21          ; Set units to millimeters
G90          ; Absolute positioning mode
G94          ; Feed rate units per minute
G17          ; XY plane selection

; Test 1: Motion Buffer - Simple moves
G0 X0 Y0 Z0  ; Rapid to origin (tests motion_buffer + motion_gcode_parser)
G1 X10 Y0 F100    ; Linear move X-axis (tests motion planning)
G1 X10 Y10 F100   ; Linear move Y-axis
G1 X0 Y10 F100    ; Linear move back
G1 X0 Y0 F100     ; Return to origin

; Test 2: Motion Planner - Rapid sequence (tests buffering)
G1 X5 Y5 F200     ; Quick sequence to test
G1 X15 Y5 F200    ; motion planner's ability
G1 X15 Y15 F200   ; to handle rapid commands
G1 X5 Y15 F200    ; and optimize trajectories
G1 X5 Y5 F200     ; Complete the square

; Test 3: Arc Interpolation (tests motion_gcode_parser arc handling)
G2 X25 Y5 I10 J0 F150   ; Clockwise arc
G3 X25 Y15 I0 J5 F150   ; Counter-clockwise arc
G2 X5 Y15 I-10 J0 F150  ; Return with arc
G3 X5 Y5 I0 J-5 F150    ; Complete circle

; Test 4: Complex Motion Profile (tests all modules together)
G1 X0 Y0 F300     ; Rapid positioning
G1 Z-1 F50        ; Z-axis move (slow feed)
G1 X20 Y0 F200    ; Cut simulation
G2 X20 Y20 I0 J10 F200  ; Arc corner
G1 X0 Y20 F200    ; Continue cut
G1 X0 Y0 F200     ; Close shape
G0 Z0             ; Lift Z-axis

; Test 5: Feed Rate Changes (tests speed control integration)
G1 X10 Y10 F50    ; Slow move
G1 X20 Y10 F500   ; Fast move
G1 X20 Y20 F100   ; Medium move
G1 X10 Y20 F250   ; Variable speed
G0 X0 Y0          ; Return home

; Test 6: Precision Moves (tests motion buffer precision)
G1 X1 Y1 F100
G1 X2 Y1 F100
G1 X2 Y2 F100
G1 X1 Y2 F100
G1 X1 Y1 F100
G0 X0 Y0

; Program complete - all modules tested
M30              ; Program end