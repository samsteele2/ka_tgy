# KA nFET ESC quick reference

This guide covers normal setup, operation, and fault identification. For design
details and firmware settings, see [README.md](README.md).

## What the system does

The ESC runs the motor normally while throttle is applied. The checked-in
persistent-homing configuration also homes after a normal zero-throttle startup.
After the motor has run and throttle returns to zero, it waits about three
seconds for the rotor to coast, then moves the rotor slowly to the stored home
position.

The homing target moves at about 60 rotor RPM. Homing stops when the rotor is at
home and nearly stationary. It then turns the bridge off but continues monitoring
the AS5600. If the rotor moves beyond +/-15 degrees from home, it starts another
homing movement. A homing attempt is cancelled immediately by a new nonzero
throttle command and is aborted if it takes longer than eight seconds. A hard
failure leaves the bridge off for fifteen seconds and then retries while zero
throttle remains commanded.

Homing and calibration drive are tuned at 16.8 V and automatically compensated
from the PC2 battery-voltage measurement. At 11.1 V the firmware applies
approximately 1.516 times the nominal command; compensation cannot exceed
approximately 1.6. The checked-in `INDEX_HOME_OUTPUT_MAX = 196` is the nominal
homing ceiling at 16.8 V, and the compensated 8-bit command is clamped at 255.
Calibration compensation increases pulse density but retains the 8.0 us
individual pulse ceiling. These are voltage limits, not measured phase-current
protection.

The checked-in firmware is configured for a standard 12-slot, 14-pole motor
(`INDEX_POLE_PAIRS = 7`). Electrical calibration aligns encoder direction and
electrical zero; it does not estimate the pole count. Any changes to motor
mounting, motor wiring, encoder magnet, or encoder mounting require recalibration
by following the steps under "Initial commissioning". Calibration causes a
sequence of small forward and reverse rotor steps and an audible tone. This is
expected. The motor must be allowed to rotate freely during this time.

## Initial commissioning

1. Secure the motor and remove the propeller if practical.
2. Check all three motor phases, the encoder/ESC wiring, magnet position, and RC
   signal connection.
3. Program and verify `ka_nfet.hex`. The supplied flash scripts use USBasp and
   also program the board fuse values. They do not rebuild the firmware.
4. Perform throttle and home calibration as described below.
5. Leave the rotor untouched. After the endpoint acknowledgments and RC-ready
   tones, allow electrical calibration and the following homing movement to
   finish; a brief motor run is not required first.
6. If calibration succeeds, cycle power and verify startup homing, then repeat a
   brief run-to-zero test.
7. Confirm that the rotor returns smoothly to home and that the ESC, motor, and
   wiring remain at acceptable temperatures before fitting the propeller.

## Learning throttle range and home

Throttle calibration also records the rotor position that will be used as
home.

1. Switch the transmitter on and command full throttle.
2. With power disconnected, place the rotor at the required home position. Use
   a safe, non-damaging fixture if necessary; do not plan to hold it by hand.
3. Remove the propeller whenever practical, connect the current-limited supply,
   and keep clear of the motor.
4. Wait for one high-pitched acknowledgment beep. This confirms the high
   throttle endpoint.
5. Move the throttle to its lowest position and keep the rotor safely at home
   until the measurement finishes.
6. Wait for two high-pitched acknowledgment beeps. These confirm the low
   endpoint and home position.
7. Keep clear after the remaining startup tones. The electrical record is now
   invalidated intentionally, so electrical calibration begins immediately and
   proceeds directly into homing.
8. Disconnect power before removing any fixture.

If the desired home changes, repeat this procedure. Never reach into the
propeller or manually restrain a powered motor while the low endpoint is being
accepted.

## Normal operating procedure

1. Begin with a valid zero-throttle command and power the ESC normally.
2. After the RC-ready tones, expect approximately three seconds with the bridge
   off, followed by startup homing.
3. Operate the motor as a conventional ESC.
4. After the motor has run, return throttle to zero and expect approximately
   three seconds of unpowered coasting.
5. Keep clear while the rotor slews slowly to home. An approximately 435 Hz tone
   from the audible-rate sine-weighted voltage-vector interpolation is expected.
6. The ESC turns the bridge off when the rotor reaches home and settles, but
   continues monitoring position. Moving more than +/-15 degrees from home
   starts another homing movement.

Applying throttle during the startup delay, retry cooldown, calibration, homing,
or bridge-off monitoring cancels that operation and returns control to normal
motor operation.

These statements describe the checked-in settings. If
`INDEX_PERSISTENT_HOMING = 0`, startup homing, post-home monitoring, continuous
hold, and automatic retry are disabled; the tolerance, retry-enable, and retry
cooldown settings then have no effect. If
`INDEX_RESTART_HOMING_BEYOND_DEGREES = 0`, the bridge does not release after
settling: the PID controller remains active continuously at zero throttle and
opposes external movement.

## Expected calibration behavior

- The rotor first aligns, then steps through a forward sequence and a reverse
  sequence.
- Some individual steps may be very small, reverse briefly, or catch up on a
  later step. The complete sequence is what is checked.
- An approximately 6 kHz electrical tone may be audible.
- A successful calibration takes at least about eight seconds and may take
  longer while the rotor settles.
- After calibration, the rotor proceeds directly into the normal slow homing
  movement.
- An invalid electrical CRC automatically starts this calibration. The stored
  mechanical home is not part of that CRC and remains the homing target.
- Do not restrain, push, or reposition the rotor during calibration.

## Low-beep fault codes

Count each group of **low-pitched** beeps after an indexing failure. With the
checked-in retry setting, a hard fault is followed by five bridge-off seconds and
then another attempt, so the same group may repeat. Power the system off before
inspecting it.

| Beeps | Meaning | First checks |
|---:|---|---|
| 2 | AS5600 communication failure or no detected magnet (`MD = 0`) | Check sensor power, ground, SDA/SCL wiring, connectors, and magnet alignment. The bridge remains off. |
| 3 | No stored home position | Repeat throttle/home calibration and hold the rotor at the intended home during low-endpoint capture. |
| 4 | Rotor did not settle during electrical calibration | Check for vibration, loose folding blades, encoder noise, or a rotor that continues rocking. |
| 5 | Homing did not finish within eight seconds | Check for an obstruction, excessive friction, a slipping propeller assembly, or inability to hold the home position. |
| 6 | Calibration detected an implausibly large rotor step | Check the encoder magnet and mounting, intermittent sensor wiring, and sudden mechanical movement. |
| 7 | Complete calibration sweeps did not agree | Check for a blocked or binding shaft and make sure the propeller or load is not forcing inconsistent forward and reverse movement. |
| 8 | AS5600 magnet field is too weak (`ML`) | Reduce the sensor-to-magnet air gap and check magnet strength, centering, and orientation. Homing continues after this advisory code. |
| 9 | Invalid internal calibration state | Cycle power once. If it repeats, reflash the verified HEX and report the fault. |
| 10 | AS5600 magnet field is too strong (`MH`) | Increase the sensor-to-magnet air gap and check magnet strength and centering. Homing continues after this advisory code. |

Startup tones and throttle-calibration acknowledgments are not fault codes.
Codes 8 and 10 are advisory and sound immediately before the motor starts
homing; they do not inhibit homing. The other low-pitched groups report a
stopped calibration or homing sequence.

## If calibration repeatedly fails

1. Record the exact low-beep count before cycling power.
2. Remove the propeller or unload the shaft and retry if possible.
3. Confirm the rotor turns freely without rubbing, binding, or loose hardware.
4. Inspect the AS5600 magnet for correct centering, spacing, and secure mounting.
5. Check sensor wiring while gently moving the harness to find intermittent
   connections.
6. Retry at reduced supply voltage with a suitable current limit.
7. Do not increase calibration power merely to force a successful result.

Report the beep count, supply voltage, observed current, whether the propeller
was installed, and what the rotor did immediately before the failure.

After correcting the problem, persistent mode retries automatically after its
cooldown while zero throttle remains valid. Applying nonzero throttle cancels a
pending retry. With retry disabled, apply a nonzero command and return to zero,
or cycle power. Stop repeated attempts if temperatures continue to rise.

## Flashing the supplied image

The flash scripts program the existing `ka_nfet.hex`; they do not compile a new
image.

Windows:

```powershell
.\flash.bat
```

Linux:

```bash
./flash.sh
```

Use these scripts only with the intended PCB-783B1-02 hardware and verify the
programmer orientation before applying power.
