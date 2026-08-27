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
7. Initialize the position target at the measured rotor angle, slew it toward
   mechanical home, and run the PI controller against that moving target.
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
- encoder direction and pole-pair count;
- electrical-angle offset;
- fixed homing low-side pulse width;
- a legacy nonzero density field retained for EEPROM layout compatibility; and
- electrical record marker `0x5f`, written last as the commit.

The legacy density field is not measured and is not used by the position
controller.

## Electrical calibration

Calibration logic runs every 4.096 ms and directly commands a stationary or
slowly rotating six-step field. The low-side FET receives one pulse per four
20 kHz carrier frames: 5 kHz effective. The selected high-side source remains
on for the vector, but conducts phase current only during the low-side pulse and
motor-current decay.

| Phase | Behavior |
|---|---|
| Initial hold | Apply vector 0 with the fixed calibration waveform until it satisfies the settling consensus. |
| Acquisition | Step through one complete six-vector electrical revolution. Settle at every vector and discard all acquisition travel. |
| Forward measurement | Step through 12 vectors (two electrical revolutions). Do not advance until the current vector has settled; accumulate only settled endpoint-to-endpoint travel. |
| Reverse measurement | Return through 12 settled vector steps and accumulate endpoint travel independently. |
| Per-step validation | Require settled endpoint displacement from 7 through 1024 encoder counts and a consistent direction. Reverse steps must have the opposite sign. |
| Geometry validation | Require return within 32 counts, forward/reverse travel within 32 counts, 1–20 pole pairs, and pole-fit error below 129 counts. |
| Commit | Calculate encoder direction, pole pairs, and electrical offset; write marker `0x5f` last. |

With `INDEX_CAL_NOISE_DELTA = 3`, settling requires 64 consecutive samples
(262.144 ms) with no sample jump above three counts and no excursion beyond six
counts from the stable-window origin. Each vector has a three-second timeout.
This consensus window is used directly instead of calculating a separate mean;
transient samples are excluded from all geometry measurements. The minimum
successful calibration time is approximately 8.1 seconds.

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
- rejected electrical calibration: bridge off, four low beeps; and
- homing timeout: bridge off, five low beeps.

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

With the current configuration, calibration uses fixed 8.0 us low-side pulses
at 5 kHz effective, for 4.0% average applied duty. Homing uses a separate fixed
3.5 us pulse, and an 8-bit first-order accumulator varies its density from 0 to
255 frames at a 20 kHz carrier. Zero controller output coasts; indexing never
applies dynamic braking.

## Position-control specification

Angles use AS5600 counts: 4096 counts per mechanical revolution. Position and
sample-to-sample motion are wrapped to the signed range -2048 to 2047.

### Slewed target

At index start, the demanded position is initialized to the measured rotor
angle. It then follows the shortest mechanical path toward home at
`INDEX_HOME_SLEW_RPM`. At the checked-in 15 RPM setting and 4.096 ms update
period, a fractional accumulator advances the target by four or five AS5600
counts per update for approximately the configured average rate.

The target is permitted to lead the measured rotor by at most
`INDEX_HOME_MAX_LEAD_ELECTRICAL_DEGREES`. The checked-in 360-degree limit is
evaluated using the calibrated pole-pair count. If the rotor falls behind, the
target pauses before crossing the limit. If an external displacement has
already put it outside the limit, the target is reanchored at the measured
rotor position. This bounds stored trajectory separation after a stick-slip
event; it is not a rotor-speed controller.

### PI law

Every 4.096 ms:

```text
error[k] = wrapped(target[k] - position[k])
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
rotor crosses the moving target. Between crossings, `INDEX_I_MAX` bounds its
magnitude.

Once the target reaches home, the same law naturally becomes home-position PI
control. The signed command is saturated to -255 through 255. Its magnitude directly
sets pulse density and its sign selects torque direction. There is no stall
counter, dead-zone inversion, learned minimum output, or homing breakaway pulse.

Current checked-in values are:

| Setting | Value | Effective behavior |
|---|---:|---|
| `INDEX_P_GAIN` | 6 | P = 0.375 |
| `INDEX_I_GAIN` | 3 | I = 0.1875 |
| `INDEX_I_MAX` | 16348 | Symmetric integral-accumulator clamp |
| `INDEX_HOME_DEADZONE_MINUTES` | 180 | Approximately +/-3 degrees |
| `INDEX_HOME_SLEW_RPM` | 15 | Mechanical target slew rate |
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

- With P = 0.375, target-tracking errors of approximately 680 counts or more
  saturate the proportional output.
- The integral can still reach its clamp during a saturated approach, although
  crossing the moving target clears it before accumulation begins in the
  opposite direction.
- There is no derivative term or electronic velocity damping.
- Output remains quantized by 8-bit pulse density and six 60-degree electrical
  vectors.
- The trajectory limits target separation, but there is no direct rotor-speed
  feedback or acceleration controller.
- The exact 180-degree position error has two equivalent paths; signed wrapping
  selects one.
- Position control uses a fixed safe pulse width but has no current feedback.

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
| `INDEX_HOME_SLEW_RPM` | Mechanical target slew rate toward home. |
| `INDEX_HOME_MAX_LEAD_ELECTRICAL_DEGREES` | Maximum target lead over the measured rotor. |
| `INDEX_HOME_TIMEOUT_SECONDS` | Homing watchdog before the five-beep abort. |
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

The current normal build uses 6184 application bytes and 111 bytes of SRAM,
leaving 984 bytes before the boot section at word address `0x0E00`.

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
