# S-Curve Profile Calculator
$max_vel = 5000.0        # steps/sec
$max_accel = 10000.0     # steps/sec²
$max_jerk = 50000.0      # steps/sec³
$distance = 5000         # steps

# Calculate jerk parameters
$t_jerk = $max_accel / $max_jerk
Write-Host "t_jerk = $t_jerk sec"

$v_jerk = 0.5 * $max_accel * $t_jerk
Write-Host "v_jerk = $v_jerk steps/sec"

$d_jerk = (1.0 / 6.0) * $max_jerk * [Math]::Pow($t_jerk, 3)
Write-Host "d_jerk = $d_jerk steps (per jerk segment)"

# Calculate constant acceleration parameters
$v_between = $max_vel - 2 * $v_jerk
Write-Host "v_between_jerks = $v_between steps/sec"

if ($v_between -gt 0) {
    $d_const_accel = [Math]::Pow($v_between, 2) / (2 * $max_accel)
    $t_const = $v_between / $max_accel
}
else {
    $d_const_accel = 0
    $t_const = 0
}
Write-Host "d_const_accel = $d_const_accel steps"
Write-Host "t_const = $t_const sec"

# Total acceleration and deceleration distances
$d_accel_total = 2 * $d_jerk + $d_const_accel
$d_decel_total = $d_accel_total
Write-Host "`nAcceleration phase:"
Write-Host "  d_accel_total = $d_accel_total steps"
Write-Host "  t_accel_total = $(2*$t_jerk + $t_const) sec"

Write-Host "`nDeceleration phase:"
Write-Host "  d_decel_total = $d_decel_total steps"
Write-Host "  t_decel_total = $(2*$t_jerk + $t_const) sec"

# Check if there's cruise phase
$total_accel_decel = $d_accel_total + $d_decel_total
Write-Host "`nTotal distance needed for accel+decel = $total_accel_decel steps"
Write-Host "Actual move distance = $distance steps"

if ($distance -ge $total_accel_decel) {
    Write-Host "`n>>> LONG MOVE - Will reach max velocity and cruise <<<"
    $d_cruise = $distance - $total_accel_decel
    $t_cruise = $d_cruise / $max_vel
    Write-Host "Cruise distance = $d_cruise steps"
    Write-Host "Cruise time = $t_cruise sec"
    
    $total_time = 4 * $t_jerk + 2 * $t_const + $t_cruise
    Write-Host "`nTotal motion time = $total_time sec"
}
else {
    Write-Host "`n>>> SHORT MOVE - Will NOT reach max velocity <<<"
}

Write-Host "`n=== SYMMETRY CHECK ==="
Write-Host "Accel time should equal Decel time: $(2*$t_jerk + $t_const) sec"
Write-Host "If decel looks faster on scope, there's a bug!"
