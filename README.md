# KA nFET ESC firmware

Experimental ATmega8A firmware for PCB-783B1-02. Normal operation uses the
existing SimonK sensorless ESC code. This fork adds AS5600 electrical calibration
and post-run positioning to a stored mechanical home.

> **Safety:** index mode controls voltage, not phase current. Use a
> current-limited supply, begin below flight voltage, inspect all six gate
> waveforms, and provide an immediate power disconnect. `INDEX_CAL_DUTY_MAX`
> limits each calibration pulse to 8.0 us. Calibration emits at most one pulse
> per four 20 kHz frames (5 kHz effective, 4% average duty); this is not a
> phase-current limit.

## System overview

| Item | Implementation |
|---|---|
| MCU | ATmega8A at 16 MHz |
| Normal drive | SimonK sensorless six-step commutation |
| Command | RC PWM on PD2/INT0; legacy SimonK I2C is retained |
| Position sensor | AS5600, 12-bit absolute angle, 400 kHz TWI |
| Index update | 4.096 ms / 244.14 Hz |
| Index drive | Sensored six-step; 20 kHz homing PDM, 5 kHz calibration pulses |
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
4. On return to zero throttle, turn the bridge off and coast for
   `INDEX_START_DELAY_SECONDS`.
5. Read the AS5600. Abort with the bridge off if the sensor or stored home is
   invalid.
6. If the electrical record is invalid, run electrical calibration and commit
   the result.
7. Apply a direct position step to mechanical home and run the PI controller.
8. When position and motion are within tolerance, turn the complete bridge off
   until another nonzero run.

A new nonzero throttle command always cancels calibration or positioning and
returns control to SimonK.

## Home and EEPROM data

Throttle calibration captures the AS5600 angle at the learned low-throttle
endpoint as mechanical home. A new home invalidates the electrical record so it
is recalibrated on the next eligible stop.

The EEPROM record contains:

- mechanical home and its validity marker;
- encoder direction and pole-pair count;
- electrical-angle offset;
- calibrated low-side operating pulse width;
- measured breakaway density; and
- electrical record marker `0x5e`, written last as the commit.

Breakaway density remains part of the electrical-calibration record but is not
used by the position controller.

## Electrical calibration

Calibration logic runs every 4.096 ms and directly commands a stationary or
slowly rotating six-step field. The low-side FET receives one pulse per four
20 kHz carrier frames: 5 kHz effective. The selected high-side source remains
on for the vector, but conducts phase current only during the low-side pulse and
motor-current decay.

| Phase | Behavior |
|---|---|
| Initial hold | Apply vector 0 at minimum pulse width until motion remains inside the encoder-noise band for 64 samples (262.144 ms). Timeout is approximately 3 s. |
| Excitation search | Alternate vectors 0 and 1 every 12 samples while increasing pulse width by eight Timer2 cycles (0.5 us). Require four consecutive samples beyond the full noise band before accepting movement. |
| Operating pulse | Save the measured threshold and use threshold + 16 Timer2 cycles (1 us), capped by `INDEX_CAL_DUTY_MAX`. |
| Alignment | Hold vector 0 until mechanically settled. |
| Forward sweep | Advance the field by 8/4096 electrical revolution per update for two electrical revolutions, then settle. |
| Reverse sweep | Return through two electrical revolutions and settle. |
| Validation | Require opposite travel signs, matched magnitudes, return to the initial position, 1–20 pole pairs, and bounded pole-fit error. |
| Commit | Calculate direction, pole pairs, electrical offset, pulse width, and breakaway density; write the validity marker last. |

With `INDEX_CAL_NOISE_DELTA = 3`, settling permits a three-count instantaneous
delta and six-count total window excursion. Movement proof requires at least
seven counts from baseline for four consecutive samples.

### Calibration frequency rationale

The audible 5 kHz calibration pulse rate is intentional. Earlier calibration
used a low-side pulse in every 20 kHz carrier frame. With the rotor stationary
and therefore producing no back-EMF, the winding current did not decay
sufficiently during the approximately 46 us off-time. Successive pulses
accumulated phase current, causing excessive supply current and high-side FET
heating. Hardware testing showed approximately eight times less calibration
input current after changing to an 8 us pulse every 200 us.

Timer2 still runs at 20 kHz, but calibration emits only one pulse per four
frames. The resulting 5 kHz winding-current and torque excitation can therefore
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
- rejected electrical calibration: bridge off, four low beeps.

## Index power stage

The calibrated pole count, encoder direction, and electrical offset convert
mechanical angle to rotor electrical angle. Controller sign requests positive or
negative 90-degree electrical torque angle, rounded to the nearest of six active
vectors.

Each vector energizes one high-side source and a different phase low-side sink.
Only the sink is pulsed. A vector transition performs:

1. all six MOSFETs off;
2. `INDEX_DEADTIME_US` all-off delay;
3. select one high-side source;
4. another `INDEX_DEADTIME_US` delay; and
5. start low-side pulses on the sink phase.

Calibration uses 7.0-8.0 us low-side pulses at 5 kHz effective, for 3.5-4.0%
average applied duty. The measured sparse-pulse threshold is converted to the
homing timer domain; homing retains 3.5-4.0 us pulses and an 8-bit first-order
accumulator varies their density from 0 to 255 frames at a 20 kHz carrier. Zero
controller output coasts; indexing never applies dynamic braking.

## Position-control specification

Angles use AS5600 counts: 4096 counts per mechanical revolution. Position and
sample-to-sample motion are wrapped to the signed range -2048 to 2047.

### Direct step

At index start, the demanded position immediately becomes mechanical home.
There is no target trajectory, acceleration limit, RPM limit, taper, or
feed-forward term.

### PI law

Every 4.096 ms:

```text
error[k] = wrapped(home - position[k])
if error[k] crossed zero:
    integral[k-1] = 0
integral[k] = clamp(integral[k-1] + error[k], -I_MAX, I_MAX)

u = trunc(P * error[k])
  + trunc(I * trunc(integral[k] / 32))
```

P and I are configured in sixteenths. Integral action runs on every control
update, including while output is saturated and while the rotor is moving. The
accumulator is cleared when the signed position error changes sign or is exactly
zero, preventing stored torque from continuing in the old direction after the
rotor crosses home. Between crossings, `INDEX_I_MAX` bounds its magnitude.

The signed command is saturated to -255 through 255. Its magnitude directly
sets pulse density and its sign selects torque direction. There is no stall
counter, dead-zone inversion, learned minimum output, or homing breakaway pulse.

Current checked-in values are:

| Setting | Value | Effective behavior |
|---|---:|---|
| `INDEX_P_GAIN` | 8 | P = 0.50 |
| `INDEX_I_GAIN` | 4 | I = 0.25 |
| `INDEX_I_MAX` | 16348 | Symmetric integral-accumulator clamp |
| `INDEX_HOME_DEADZONE_MINUTES` | 180 | Approximately +/-3 degrees |

### Completion

Homing completes when both conditions are true:

- absolute position error is inside `INDEX_HOME_DEADZONE_MINUTES`; and
- absolute raw one-sample rotor delta is at most two counts.

The deadzone setting is expressed in angular minutes, where 60 minutes equals
one degree. Firmware converts it to the nearest AS5600 count at assembly time;
180 minutes becomes 34 counts, or approximately 2.99 degrees on either side of
home.

Completion immediately turns the bridge off and is terminal for that stopped
cycle.

## Remaining predictable edge cases

The controller is substantially simpler, but these behaviors are intentional:

- With P = 0.50, position errors of approximately 510 counts or more saturate the
  output immediately. Most initial home steps therefore begin at maximum pulse
  density.
- The integral can still reach its clamp during a saturated approach, although
  crossing home clears it before accumulation begins in the opposite direction.
- There is no derivative term or electronic velocity damping.
- Output remains quantized by 8-bit pulse density and six 60-degree electrical
  vectors.
- The exact 180-degree position error has two equivalent paths; signed wrapping
  selects one.
- Position control uses the calibrated pulse width but has no current feedback.

Tune P first, then introduce I only as needed to overcome steady position error.
Use `INDEX_I_MAX` to bound the maximum stored integral torque.

## User configuration

User-facing settings are in `ka_nfet.inc`:

| Setting | Purpose |
|---|---|
| `INDEX_ENABLE` | Compile AS5600 indexing. |
| `INDEX_DRIVE_ENABLE` | Set to 0 for sensor-only commissioning. |
| `INDEX_START_DELAY_SECONDS` | All-off coast time after zero throttle. |
| `INDEX_P_GAIN`, `INDEX_I_GAIN` | Controller gains in sixteenths. |
| `INDEX_I_MAX` | Integral accumulator limit. |
| `INDEX_HOME_DEADZONE_MINUTES` | Terminal +/- position window in angular minutes. |
| `INDEX_CAL_NOISE_DELTA` | Electrical-calibration encoder-noise band. |
| `INDEX_CAL_DUTY_MAX` | 128 cycles: 8.0 us at 5 kHz effective / 4% average-duty ceiling. |
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

The current normal build uses 6256 application bytes and 108 bytes of SRAM,
leaving 912 bytes before the boot section at word address `0x0E00`.

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
