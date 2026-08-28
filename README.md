# KA nFET ESC firmware

Experimental ATmega8A firmware for PCB-783B1-02. Normal operation uses the
existing SimonK sensorless ESC code. This fork adds AS5600 electrical alignment
and post-run positioning to a stored mechanical home.

For installation, normal operation, and fault-code checks, see
[QUICK_REFERENCE.md](QUICK_REFERENCE.md).

> **Safety:** index mode controls voltage, not phase current. Use a
> current-limited supply, begin below flight voltage, inspect all six gate
> waveforms, and provide an immediate power disconnect. `INDEX_CAL_DUTY_MAX`
> limits each calibration pulse to 8.0 us. Calibration emits at most one pulse
> at 77/255 density (approximately 6.0 kHz effective, 4.8% average duty); this is not a
> phase-current limit.

## System overview

| Item | Implementation |
|---|---|
| MCU | ATmega8A at 16 MHz |
| Normal drive | SimonK sensorless six-step commutation |
| Command | RC PWM on PD2/INT0; legacy SimonK I2C is retained |
| Position sensor | AS5600, 12-bit absolute angle, 400 kHz TWI |
| Position/encoder update | 4.096 ms / 244.14 Hz |
| Index drive | Voltage-mode sensored FOC; 1 kHz two-vector SVM, 20 kHz homing PDM |
| Persistent data | Mechanical home and electrical calibration in EEPROM |
| Firmware image | `ka_nfet.hex` |

The AS5600 is used only while the motor is stopped. SimonK is otherwise
unchanged except for hooks that capture home, arm indexing after a run, and
service calibration or positioning at zero throttle.

## Runtime sequence

1. Boot with all MOSFETs off and load EEPROM.
2. Arm and run as a conventional SimonK ESC.
3. A nonzero command arms one future index cycle. Power-up at zero throttle
   cannot start indexing.
4. On return to zero throttle, turn the bridge off. Coast for
   `INDEX_HOME_DELAY_MS` when the stored electrical record is valid, or
   `INDEX_CALIBRATION_DELAY_MS` when a new electrical calibration is required.
   Both checked-in delays are 3000 ms.
5. Read the AS5600. Abort with the bridge off if the sensor or stored home is
   invalid.
6. If the electrical record is invalid, align encoder direction and electrical
   zero using the configured motor pole count, then commit the result.
7. Initialize the position target at the measured rotor angle, slew it toward
   mechanical home, and run the PID controller against that moving target.
8. After the target reaches home, turn the complete bridge off when position
   and motion are within tolerance. Abort after eight seconds if it cannot settle.

A new nonzero throttle command always cancels calibration or positioning and
returns control to SimonK.

## Home and EEPROM data

Throttle calibration captures the AS5600 angle at the learned low-throttle
endpoint as mechanical home. A new home invalidates the electrical record so it
is recalibrated on the next eligible stop.

The EEPROM record contains:

- mechanical home and its validity marker;
- encoder direction and the configured pole-pair count;
- electrical-angle offset;
- fixed homing low-side pulse width;
- a legacy nonzero density field retained for EEPROM layout compatibility; and
- electrical record marker `0x60`, written last as the commit.

The legacy density field is not measured and is not used by the position
controller.

`INDEX_POLE_PAIRS` is compiled from `ka_nfet.inc` and copied into the committed
electrical record. On boot, a different stored value invalidates that record and
forces a new electrical alignment before homing. The checked-in value is 7 for
the standard 12-slot, 14-pole motor arrangement.

## Electrical calibration

Calibration logic runs every 4.096 ms and directly commands a stationary or
slowly rotating six-step field. The low-side FET receives 77 pulses per 255
20 kHz carrier frames: approximately 6.0 kHz effective. The selected high-side source remains
on for the vector, but conducts phase current only during the low-side pulse and
motor-current decay.

| Phase | Behavior |
|---|---|
| Initial hold | Apply vector 0 with the fixed calibration waveform until it satisfies the settling consensus. |
| Acquisition | Step through one complete six-vector electrical revolution. Settle at every vector and discard all acquisition travel. |
| Forward measurement | Step through 12 vectors (two electrical revolutions). Do not advance until the current vector has settled; accumulate only settled endpoint-to-endpoint travel. |
| Reverse measurement | Return through 12 settled vector steps and accumulate endpoint travel independently. |
| Per-step validation | Permit zero-motion, reversed, and catch-up steps. Reject only a gross settled endpoint jump above 1024 encoder counts. |
| Geometry validation | Determine direction from complete sweeps; require opposite aggregate directions, return within 32 counts, forward/reverse travel within 32 counts, and sufficient total movement. Pole count is not estimated. |
| Commit | Average the two vector-zero endpoints reached from opposite directions, calculate the electrical offset, and write marker `0x60` last. |

With `INDEX_CAL_NOISE_DELTA = 3`, settling requires 64 consecutive samples
(262.144 ms) with no sample jump above three counts and no excursion beyond six
counts from the stable-window origin. Each vector has a three-second timeout.
This consensus window is used directly instead of calculating a separate mean;
transient samples are excluded from all geometry measurements. The minimum
successful calibration time is approximately 8.1 seconds.

### Calibration frequency rationale

The audible approximately 6 kHz calibration pulse rate is intentional. Earlier calibration
used a low-side pulse in every 20 kHz carrier frame. With the rotor stationary
and therefore producing no back-EMF, the winding current did not decay
sufficiently during the approximately 46 us off-time. Successive pulses
accumulated phase current, causing excessive supply current and high-side FET
heating. Hardware testing showed approximately eight times less calibration
input current after changing to an 8 us pulse every 200 us.

Timer2 still runs at 20 kHz, but calibration emits at 77/255 pulse density.
The resulting approximately 6 kHz winding-current and torque excitation can therefore
be audible. Restoring an ultrasonic *electrical excitation* is not considered a
safe timing-only change: it would require substantially shorter pulses, closed-
loop phase-current regulation, or an actively controlled fast-decay state. The
present waveform deliberately favors bounded current and FET temperature over
inaudible calibration.

The selected high-side source remains statically enabled within each six-step
vector; only the low-side sink is carrier-switched. High-side temperature during
calibration is consequently treated primarily as a phase-current/conduction or
gate-enhancement concern, not as 20 kHz high-side switching loss.

Failure behavior:

- AS5600 transaction failure: bridge off, two low beeps;
- missing mechanical home: bridge off, three low beeps;
- electrical-calibration settling timeout: bridge off, four low beeps;
- homing timeout: bridge off, five low beeps;
- gross calibration step above 1024 counts: bridge off, six low beeps;
- aggregate sweep direction, return, or travel rejection: bridge off, seven low beeps;
- AS5600 magnet field too weak (`ML`): eight low beeps immediately before homing, then homing continues;
- invalid internal calibration state: bridge off, nine low beeps; and
- AS5600 magnet field too strong (`MH`): ten low beeps immediately before homing, then homing continues.

### AS5600 angle and filter configuration

All position feedback comes from the 12-bit `RAW ANGLE` registers (`0x0C` and
`0x0D`), not the scaled `ANGLE` registers. This avoids the `ANGLE` path's range
scaling and 10-LSB hysteresis at the 0/360-degree boundary. Each read begins at
`STATUS` (`0x0B`) and receives status plus both raw-angle bytes in one I2C burst.

Before mechanical-home capture and before an indexing sequence, firmware writes
a known volatile AS5600 configuration: normal power mode, output hysteresis off,
8x slow filtering, fast-threshold filtering off, and watchdog off. The 8x slow
filter has an approximately 1.1 ms documented step response, which settles well
inside the 4.096 ms position-control interval while retaining low output noise.
Disabling the watchdog also keeps sensor behavior consistent after the rotor has
been stationary for a long time. The analog-output selection is left at its
default full-range mode but is irrelevant because this firmware uses I2C only.

## Index power stage

The configured pole count, calibrated encoder direction, and electrical offset
convert mechanical angle to rotor electrical angle. Controller sign requests a
positive or negative 90-degree electrical q-axis voltage angle. This is
voltage-mode sensored FOC: there is no phase-current measurement or closed-loop
d/q current regulator.

The requested voltage angle is decomposed into its two adjacent inverter active
vectors. A sine-weighted table implements the standard sector dwell law:

```text
T1 = magnitude * sin(60 degrees - alpha)
T2 = magnitude * sin(alpha)
T0 = 1 - T1 - T2
```

Here, `alpha` is the requested angle inside its 60-degree sector. At
`INDEX_FOC_UPDATE_HZ`, a first-order dwell accumulator selects between the two
active vectors in the ratio `T2/(T1+T2)`. The 20 kHz pulse density is multiplied
by `T1+T2`; unpowered carrier frames supply `T0`. Unlike linear interpolation,
this traces the largest constant-radius circle inside the inverter voltage
hexagon. Maximum circular magnitude is therefore 86.6% of a single active-vector
magnitude. The table is sampled every eight encoder counts (about 0.7 electrical
degrees), limiting calculated radius error to less than 0.25%.

This space-vector interpolation removes the former 60-electrical-degree angle
steps and 15.5% hexagonal magnitude envelope. The checked-in 1 kHz rate is
intentionally audible and bounds worst-case high-side commutation to 1 kHz
rather than 20 kHz.

Each vector energizes one high-side source and a different phase low-side sink.
Only the sink is pulsed. A vector transition performs:

1. all six MOSFETs off;
2. `INDEX_DEADTIME_US` all-off delay;
3. select one high-side source;
4. another `INDEX_DEADTIME_US` delay; and
5. start low-side pulses on the sink phase.

With the current configuration, calibration uses fixed 8.0 us low-side pulses
at approximately 6.0 kHz effective, for 4.8% average applied duty. Homing uses a
separate fixed 3.5 us pulse, and an 8-bit modulo-255 accumulator varies its
density from 0 to 255 frames at a 20 kHz carrier. Its accumulator is preserved
across adjacent-vector commutation so torque magnitude does not restart at every
SVM update. Zero controller output coasts; indexing never applies dynamic braking.

## Position-control specification

Angles use AS5600 counts: 4096 counts per mechanical revolution. Position and
sample-to-sample motion are wrapped to the signed range -2048 to 2047.

### Slewed target

At index start, the demanded position is initialized to the measured rotor
angle. It then follows the shortest mechanical path toward home at
`INDEX_HOME_SLEW_RPM`. At the configured 60 RPM setting and 4.096 ms update
period, a fractional accumulator advances the target by 16 or 17 AS5600
counts per update for approximately the configured average rate.

The target is permitted to lead the measured rotor by at most
`INDEX_HOME_MAX_LEAD_ELECTRICAL_DEGREES`. The checked-in 360-degree limit is
evaluated using the configured pole-pair count. If the rotor falls behind, the
target pauses before crossing the limit. An already-stored target is never
reanchored to the measured rotor: doing that would momentarily zero the position
error and release torque at the limit. External displacement therefore leaves
the target intact, and the controller continues to apply its limited restoring
command. The lead limit constrains new trajectory advance; it is not a
rotor-speed controller.

### PID law

Every 4.096 ms:

```text
error[k] = wrapped(target[k] - position[k])
velocity[k] = wrapped(position[k] - position[k-1])
if error[k] crossed zero:
    integral[k-1] = 0
integral[k] = clamp(integral[k-1] + error[k], -I_MAX, I_MAX)

u = trunc(P * error[k])
  + trunc(I * trunc(integral[k] / 32))
  - trunc(D * velocity[k])
```

P, I, and D are configured in sixteenths. D acts on measured rotor movement
rather than the change in moving-target error, preventing target-slew derivative
kick. Integral action runs on every control
update, including while output is saturated and while the rotor is moving. The
accumulator is cleared when the signed position error changes sign or is exactly
zero, preventing stored torque from continuing in the old direction after the
rotor crosses the moving target. Between crossings, `INDEX_I_MAX` bounds its
magnitude.

Once the target reaches home, the same law naturally becomes home-position PID
control. The signed command is saturated to -255 through 255. Its magnitude sets
the circular q-axis voltage request, and sine-weighted SVM converts that request
to angle-dependent pulse density. Its sign selects torque direction. There is no
stall counter, dead-zone inversion, learned minimum output, or homing breakaway
pulse.

Current configured values are:

| Setting | Value | Effective behavior |
|---|---:|---|
| `INDEX_P_GAIN` | 12 | P = 0.75 |
| `INDEX_I_GAIN` | 2 | I = 0.125 |
| `INDEX_D_GAIN` | 8 | D = 0.5 per measured encoder count/update |
| `INDEX_I_MAX` | 16384 | Symmetric integral-accumulator clamp |
| `INDEX_HOME_DEADZONE_MINUTES` | 180 | Approximately +/-3 degrees |
| `INDEX_HOME_SLEW_RPM` | 60 | Mechanical target slew rate |
| `INDEX_HOME_MAX_LEAD_ELECTRICAL_DEGREES` | 360 | Maximum target-to-rotor separation |
| `INDEX_HOME_TIMEOUT_SECONDS` | 8 | Maximum homing duration |

### Completion

Homing completes only after the moving target has reached mechanical home and
both existing terminal conditions are true:

- absolute position error is inside `INDEX_HOME_DEADZONE_MINUTES`; and
- absolute raw one-sample rotor delta is at most two counts.

The deadzone setting is expressed in angular minutes, where 60 minutes equals
one degree. Firmware converts it to the nearest AS5600 count at assembly time;
180 minutes becomes 34 counts, or approximately 2.99 degrees on either side of
home.

Completion immediately turns the bridge off and is terminal for that stopped
cycle. If these conditions have not been met after
`INDEX_HOME_TIMEOUT_SECONDS`, the firmware turns the bridge off and emits five
low beeps.

## Remaining predictable edge cases

The controller is substantially simpler, but these behaviors are intentional:

- With P = 0.75, target-tracking errors of approximately 340 counts or more
  saturate the proportional output.
- The integral can still reach its clamp during a saturated approach, although
  crossing the moving target clears it before accumulation begins in the
  opposite direction.
- D uses one raw wrapped AS5600 sample delta. Gain 8 suppresses a one-count
  change through fixed-point truncation, but larger encoder noise appears in the
  damping command.
- Output magnitude remains quantized by 8-bit pulse density. Voltage angle is a
  sine-weighted time average of adjacent active vectors at `INDEX_FOC_UPDATE_HZ`.
- The trajectory limits target separation, but there is no direct rotor-speed
  feedback or acceleration controller.
- The exact 180-degree position error has two equivalent paths; signed wrapping
  selects one.
- Position control uses a fixed safe pulse width but has no current feedback.

Tune P first, increase D to remove overshoot, then introduce I only as needed to
overcome steady position error. Use `INDEX_I_MAX` to bound the maximum stored
integral torque.

## User configuration

User-facing settings are in `ka_nfet.inc`:

| Setting | Purpose |
|---|---|
| `INDEX_ENABLE` | Compile AS5600 indexing. |
| `INDEX_DRIVE_ENABLE` | Set to 0 for sensor-only commissioning. |
| `INDEX_HOME_DELAY_MS` | All-off coast time before normal homing, in milliseconds. |
| `INDEX_CALIBRATION_DELAY_MS` | All-off coast time before electrical calibration, in milliseconds. |
| `INDEX_POLE_PAIRS` | Rotor pole-pair count. Use 7 for a standard 12N14P motor. |
| `INDEX_FOC_UPDATE_HZ` | Sine-weighted space-vector interpolation rate; must divide 20 kHz. |
| `INDEX_P_GAIN`, `INDEX_I_GAIN`, `INDEX_D_GAIN` | Controller gains in sixteenths. D damps measured rotor motion. |
| `INDEX_I_MAX` | Integral accumulator limit. |
| `INDEX_HOME_DEADZONE_MINUTES` | Terminal +/- position window in angular minutes. |
| `INDEX_HOME_SLEW_RPM` | Mechanical target slew rate toward home. |
| `INDEX_HOME_MAX_LEAD_ELECTRICAL_DEGREES` | Maximum target lead over the measured rotor. |
| `INDEX_HOME_TIMEOUT_SECONDS` | Homing watchdog before the five-beep abort. |
| `INDEX_CAL_NOISE_DELTA` | Electrical-calibration encoder-noise band. |
| `INDEX_CAL_DUTY_MAX` | 128 cycles: 8.0 us at 6.0 kHz effective / 4.8% average-duty ceiling. |
| `INDEX_DEADTIME_US` | All-off delay around vector changes. |

Protocol constants, state values, validation tolerances, and fixed-point scales
remain private to `tgy.asm`.

## Build and flash

Build with AVRA:

```powershell
.\build.bat
```

```bash
./build.sh
```

The current normal build uses 6504 application bytes and 120 bytes of SRAM,
leaving 664 bytes before the boot section at word address `0x0E00`.

The USBasp flash scripts program `ka_nfet.hex`, low fuse `0x3F`, and high
fuse `0xCA`, then verify them. They do not rebuild:

```powershell
.\flash.bat
```

```bash
./flash.sh
```

Review the image and fuse values for the exact board before programming.

## Source map

- `tgy.asm` — SimonK base plus AS5600 calibration and position control.
- `ka_nfet.inc` — PCB pin map and user-facing configuration.
- `ka_nfet.hex` — generated firmware image.
- `scripts/build.ps1`, `build.sh` — AVRA builds.
- `scripts/flash.ps1`, `flash.sh` — USBasp programming and verification.
