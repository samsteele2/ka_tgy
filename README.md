# KA SimonK sensored blade indexing fork

This repository is a hardware-specific SimonK fork for the Kairos Autonomi
ATmega8A n-channel ESC. Its purpose is to retain SimonK's proven sensorless
motor operation in flight, then use an AS5600 magnetic encoder and low-voltage
sensored control to place the stopped blades at a repeatable home position.

> **Safety:** ESC firmware can destroy MOSFETs, motors, batteries, test
> equipment, and propellers. Develop with the propeller removed, a
> current-limited supply, and an emergency power disconnect. This build now
> contains an experimental torque-producing index path; it has assembled and
> passed static timing checks but has not yet been validated on the motor.

## Intended behavior

The final control sequence is:

1. Power-up and arming behave like normal SimonK. **Indexing never runs at
   startup**, even if the throttle input is already below zero-throttle.
2. A valid command above the programmed powered-on threshold starts and runs
   the motor with SimonK's existing sensorless six-step commutation.
3. When that same armed run later falls below the programmed zero-throttle
   threshold, all normal SimonK phase drive is turned off and a 5-second stop
   delay begins. This gives the blades time to coast to a stop.
4. After the delay, the ESC reads the AS5600 through the ATmega8A TWI/I2C pins.
   If electrical calibration is not present, it holds six-step vector zero at
   minimum duty, steps to vector one, and raises only the low-side PWM duty
   until AS5600 motion proves breakaway. It adds a small duty margin, then
   walks the six active vectors through one electrical revolution forward and
   backward. The measured motion gives encoder direction, pole-pair count,
   electrical offset, and a motor-specific breakaway duty. A valid result is
   committed to EEPROM and reused on later power cycles; a rejected result
   produces four low-pitch pulses and leaves every FET off.
   A 244.14 Hz control service advances an internal target toward home at
   approximately 15 RPM, computes wrapped position error, and selects the
   nearest sensored six-step torque vector. When both the slew target and rotor
   enter the two-count home window, homing completes: all six FETs turn off and
   AS5600 position polling stops for the remainder of that stopped cycle.
   If that first AS5600 read fails, the ESC gives two low-pitch warning pulses
   once and cancels homing for that stopped cycle. If the encoder responds but
   no home has been calibrated, it gives three low-pitch pulses instead.
5. A valid throttle command above the threshold cancels waiting or indexing at
   once (on the next accepted PWM frame) and returns to the existing SimonK
   sensorless start/run path. Raising throttle always has priority over homing.
6. A missing or unresponsive AS5600 must fail with all six MOSFETs off, never with a
   stuck bus wait or an uncontrolled commutation pattern.

The mechanical home is learned during SimonK's normal throttle calibration:
power on with full throttle, then lower the command while the blades are held at
the desired home position. When the low-throttle endpoint is committed to
EEPROM, the current AS5600 angle and a validity marker are committed in the
same write. An uncalibrated home prevents indexing and produces the three-low-
pulse code after the normal 5-second delay. Capturing a new home invalidates
the old electrical calibration. The next eligible post-run homing event
recalibrates the complete motor/encoder assembly before returning to home.

The trigger is a **high-to-low transition after a run**, not simply "throttle
is low." This distinction is what prevents indexing at startup.

## Current development stage

This is an experimental development build, not flight firmware.

| Stage | Status |
| --- | --- |
| Recover the KA board definition and reproduce the known firmware | Complete; baseline is retained in Git history |
| Document the actual PCB pinout and make reproducible build scripts | Complete |
| Latch indexing only after a powered run and add the 5-second post-stop timer | Implemented and assembling |
| Read AS5600 angle register `0x0E` as a 12-bit value over 400 kHz I2C | Implemented, bounded, and assembling; not yet hardware-validated |
| Post-delay two-pulse AS5600 warning, three-pulse missing-home warning, and home capture during throttle calibration | Implemented and assembling; not yet hardware-validated |
| Fail safely on I2C NACK/timeout and immediately yield to raised throttle | Implemented in the state-machine scaffold |
| 15 RPM target slew and wrapped position controller | Implemented and assembling; not hardware-validated |
| Electrical-angle and signed torque-vector math | Implemented; requires motor calibration |
| Generate sensored six-step torque with ultrasonic low-side PWM | Implemented; replaces the audible 4 kHz SVM/PDM experiment and is not yet hardware-validated |
| Automatically determine breakaway duty, encoder direction, pole pairs, and electrical offset | Implemented as a bounded six-vector forward/reverse sweep with EEPROM persistence; not yet hardware-validated |
| Tune and validate with the actual motor, encoder, supply, and gate stage | Not started |

`INDEX_ENABLE` and `INDEX_DRIVE_ENABLE` are currently `1`. A calibrated EEPROM
home is required before the post-run state machine will energize the motor.
The index backend uses the same six proven two-phase active vectors as SimonK.
For each sector, one high-side source FET stays on continuously and only the
selected low-side sink is PWM-switched. Timer2 runs at SimonK's normal carrier
of approximately 18.69 kHz, above the fixed 4 kHz tone produced by the earlier
SVM/PDM experiment. Complementary PWM is explicitly disabled in index mode.
Every carrier frame is available, giving the breakaway calibration consistent
torque without the static-like sound produced by pseudorandom frame skipping.
Every sector change first turns all six FETs off for 5 us, turns on the new
high-side source, waits another 5 us for the measured slow gate transition,
and only then starts low-side PWM.

Six-step control has more torque ripple and detent-like motion than true
three-phase FOC, but final position is still measured directly by the AS5600.
Reaching the final two-count window is terminal: the drive is shut down and no
holding or drift-correction current is applied until another run/stop cycle.

Electrical calibration is intentionally independent of SimonK's back-EMF
timing. At the first eligible stop after throttle/home calibration, vector zero
is held for 499.712 ms at 56 Timer2 cycles (3.5 us). Vector one then starts at
that duty and rises by four cycles every 49.152 ms until at least four AS5600
counts of motion are observed, with a ceiling of 128 cycles (8.0 us, about
14.95% of the carrier period). An eight-cycle margin is stored with the result.
After a second 499.712 ms alignment hold, each complete six-vector sweep takes
2.097152 seconds and is followed by another 499.712 ms settling hold. Depending
on the learned breakaway point, the complete calibration takes approximately
6.2 to 7.1 seconds after the normal 5-second coast delay. For a seven-pole-pair
motor, each sweep moves the shaft about 51.4 degrees at about 4.09 RPM before
returning it to the starting magnetic alignment.

The reference STM32 implementation in
`C:\Users\Sam\dev\OutbounderDesktop\Core\Src` informed the AS5600 register
selection and rotor-angle conventions. It cannot be copied directly: that
target has three independent hardware PWM timer channels, while this ATmega8A
has one software-selected Timer2 PWM phase. This fork therefore uses sensored
trapezoidal six-step control rather than emulating three-phase FOC. Normal
SimonK sensorless commutation and PWM remain unchanged outside index mode.

## Hardware target and pin map

The board runs an ATmega8A at 16 MHz. `ka_nfet.inc` is the authoritative board
definition.

| MCU pin | PCB net / purpose | Firmware name |
| --- | --- | --- |
| PD2 / INT0 | PWM throttle input | `rcp_in` |
| PD5, PB0, PD3 | HS_PhaseA, HS_PhaseB, HS_PhaseC | `ApFET`, `BpFET`, `CpFET` |
| PD4, PD7, PC3 | LS_PhaseA, LS_PhaseB, LS_PhaseC | `AnFET`, `BnFET`, `CnFET` |
| ADC6, ADC7, PC0 / ADC0 | SenseA, SenseB, SenseC | `mux_a`, `mux_b`, `mux_c` |
| PD6 / AIN0 | SenseX comparator reference | comparator input |
| PC4 / SDA | AS5600 data | AVR TWI SDA |
| PC5 / SCL | AS5600 clock | AVR TWI SCL |
| PC2 / ADC2 | battery-voltage divider | `mux_voltage` |
| PC1 / ADC1 | NTC thermistor | `mux_temperature` |
| PB1 | RUN LED net (not populated/connected on PCB-783B1-02) | `green_led` |
| PB2 | WARN LED net (not populated/connected on PCB-783B1-02) | `red_led` |
| PB3, PB4, PB5, RESET | MOSI, MISO, SCK, reset | USBasp / ICSP |
| PD0, PD1 | unused RX/TX interface | legacy UART definitions |

The AS5600 uses 7-bit address `0x36`; the assembly consequently sends `0x6C`
for write and `0x6D` for read. The present code reads filtered `ANGLE` at
registers `0x0E`/`0x0F`, masks it to 12 bits, and stores values from 0 through
4095. SDA and SCL require pull-up resistors to the common logic supply; verify
their value and voltage on the actual encoder board before connecting it.

## Source layout

- `tgy.asm` — SimonK core plus the KA indexing state machine and AS5600 master
  transaction.
- `ka_nfet.inc` — KA pin mapping and indexing compile-time configuration.
- `other_escs/` — inherited SimonK board definitions and `m8def.inc`; retained
  for source history and assembler includes, not built by the one-click scripts.
- `build.bat`, `flash.bat`, `build.sh`, `flash.sh` — root-level build and
  flash entry points.
- `scripts/` — PowerShell implementations used by the Windows batch launchers.
  They assemble directly in the repository root and leave no copied `tgy.asm`
  in a build directory.
- `ka_nfet.hex` — generated Intel HEX output. Rebuild it after source changes.

Important indexing settings near the top of `ka_nfet.inc` are:

- `INDEX_ENABLE` — include the state machine and AS5600 reads.
- `INDEX_DRIVE_ENABLE` — compile gate for energized index-mode vector output;
  currently `1`.
- `INDEX_DELAY_OVF` — 1221 Timer1 overflows, or 5.001216 seconds at 16 MHz.
- `INDEX_HOME_MARKER` — EEPROM marker used to distinguish a successfully
  calibrated AS5600 home from an uninitialized value. Home angle itself is
  captured during low-throttle calibration, not compiled into the image.
- `INDEX_ELECTRICAL_MARKER` — EEPROM commit marker written only after the
  automatic forward/reverse electrical calibration passes every check.
- `INDEX_CAL_DUTY_MIN`, `INDEX_CAL_DUTY_MAX`, `INDEX_CAL_DUTY_STEP`, and
  `INDEX_CAL_DUTY_MARGIN` — bounded encoder-confirmed breakaway learning for
  the low-side Timer2 pulse width.
- `INDEX_CAL_SWEEP_STEP` and the `INDEX_CAL_*_TICKS` settings — electrical field
  sweep speed and alignment/settling durations.
- `INDEX_CAL_MAX_POLE_PAIRS`, `INDEX_CAL_TRAVEL_MATCH`,
  `INDEX_CAL_RETURN_TOLERANCE`, and `INDEX_CAL_POLE_FIT` — acceptance limits
  used before direction, pole pairs, or offset can be committed.
- `INDEX_SLEW_STEP_Q8` — 1074/256 encoder counts every 4.096 ms, corresponding
  to approximately 15.004 RPM.
- `INDEX_PWM_CARRIER_HZ` — documentation constant for the approximately
  18.69 kHz carrier produced by the normal 16 MHz SimonK Timer2 timing.
- `INDEX_DEADTIME_US` — 5 us all-off break-before-make delay; the same delay
  is repeated after enabling a new high-side source and before low-side PWM.
- `INDEX_TWBR` — TWI bit-rate register value 12, producing 400 kHz at 16 MHz.

Breakaway duty, pole pairs, encoder direction, and encoder/electrical zero
offset are measured per assembly and stored in EEPROM rather than compiled into
the image. The duty ceiling, sweep acceptance tolerances, and final correction
behavior still need validation and tuning on the actual motor and supply.
Upgrading from the earlier SVM/PDM build preserves the stored mechanical home
but intentionally invalidates its shorter electrical-calibration record, so
the first eligible post-run stop performs the new breakaway calibration once.

## Installing AVRA

The build uses [AVRA](https://github.com/Ro5bert/avra), an open-source assembler
compatible with the AVRASM syntax used by SimonK. Confirm installation with:

```text
avra --version
```

### Windows (tested project setup)

There is no dependable first-party `winget` AVRA package. This project has been
verified with AVRA 1.3.0 compiled as a small native Windows executable using
TinyCC 0.9.27. Download and extract:

- [AVRA 1.3.0 source](https://github.com/Ro5bert/avra/archive/refs/tags/1.3.0.zip)
- [TinyCC 0.9.27 win64](https://download.savannah.gnu.org/releases/tinycc/tcc-0.9.27-win64-bin.zip)

Set the first two paths below to the extracted directories, then run the block
in PowerShell:

```powershell
$TccRoot = 'C:\Tools\tcc-0.9.27-win64-bin'
$AvraRoot = 'C:\Tools\avra-1.3.0'
$AvraSrc = Join-Path $AvraRoot 'src'
$InstallDir = Join-Path $env:LOCALAPPDATA 'Programs\AVRA'

$SourceNames = @(
    'avra.c', 'device.c', 'parser.c', 'expr.c', 'mnemonic.c',
    'directiv.c', 'macro.c', 'file.c', 'map.c', 'coff.c',
    'args.c', 'stdextra.c'
)
$Sources = $SourceNames | ForEach-Object { Join-Path $AvraSrc $_ }

& (Join-Path $TccRoot 'tcc.exe') `
    -Wall -O3 "-I$(Join-Path $TccRoot 'include\sys')" `
    -o (Join-Path $AvraSrc 'avra.exe') $Sources
if ($LASTEXITCODE -ne 0) { throw 'AVRA compilation failed.' }

New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
Copy-Item (Join-Path $AvraSrc 'avra.exe') $InstallDir -Force

$UserPath = [Environment]::GetEnvironmentVariable('Path', 'User')
if (($UserPath -split ';') -notcontains $InstallDir) {
    [Environment]::SetEnvironmentVariable(
        'Path', ($UserPath.TrimEnd(';') + ';' + $InstallDir), 'User'
    )
}
& (Join-Path $InstallDir 'avra.exe') --version
```

Open a new terminal after changing `PATH`. Even without that restart,
`scripts/build.ps1` also checks the exact fallback location
`%LOCALAPPDATA%\Programs\AVRA\avra.exe` used above.

The official AVRA repository also documents a Visual Studio source-build route
for newer AVRA releases, but 1.3.0 is the version used for the recovered KA
baseline and is therefore the reproducibility reference for this fork.

### macOS

Install [Homebrew](https://brew.sh/) if it is not already installed, then use
the maintained [Homebrew AVRA formula](https://formulae.brew.sh/formula/avra.html):

```sh
brew update
brew install avra
avra --version
```

The Homebrew formula currently supplies both Apple Silicon and Intel bottles.
If Homebrew cannot be used, AVRA's upstream source build is `make` followed by
`sudo make install`; see the upstream README for its toolchain prerequisites.

## Building

### Windows

Double-click `build.bat`, or run:

```powershell
.\scripts\build.ps1
```

### macOS

From Terminal:

```sh
chmod +x build.sh
./build.sh
```

To flash a reviewed `ka_nfet.hex` with USBasp, first install AVRDUDE
(`brew install avrdude`), then run:

```sh
chmod +x flash.sh
./flash.sh
```

Both paths define `ka_nfet_esc`, add the repository and `other_escs` include
directories, assemble `tgy.asm`, and write `ka_nfet.hex` in the repository root.
Intermediate `.obj`, `.cof`, and EEPROM HEX files are removed.

The current experimental build assembles without errors and reports:

- application code through word address `0x0A81` (5380 bytes),
- bootloader beginning at word address `0x0E00`, leaving 1788 bytes of unused
  application flash before the boot section,
- 101 bytes of SRAM allocated out of the ATmega8A's 1024 bytes.

There is ample raw program and data memory for the state machine and fixed-point
encoder/electrical-angle math. The more important constraint is deterministic
gate switching and conservative current during stationary six-step drive.

## First energized bench test

Remove the propeller and mechanically unload the motor. Use a current-limited
supply and begin below the normal flight voltage if the gate supply permits it.

1. Perform normal SimonK throttle calibration while holding the shaft at the
   desired mechanical home. This stores both low throttle and home in EEPROM.
2. Reboot at low throttle. The startup tones should be normal and indexing must
   not occur.
3. Command a brief normal sensorless run, then return below zero throttle.
4. Confirm all phases remain off for approximately 5 seconds. On this first
   cycle, expect a short vector-zero hold, a one-step breakaway-duty search,
   one slow six-step electrical revolution forward, one backward, and then
   normal homing. For seven pole pairs, each calibration sweep is about 51.4
   mechanical degrees at 4.09 RPM.
5. After calibration completes, homing should begin at approximately 15 RPM
   and take the shortest path to the stored mechanical home. At home, verify
   that phase current falls to zero and remains off; the firmware no longer
   polls or corrects position during that stopped cycle. On subsequent run/stop
   cycles, calibration is skipped and homing starts directly after the
   five-second delay.
6. Raise throttle during calibration or homing and confirm that the index drive
   stops on the next accepted PWM command and normal SimonK control resumes.
7. Repeat once with the AS5600 disconnected: after the delay, expect two low
   pulses and no homing. Erase or invalidate the stored home to exercise the
   three-low-pulse code. A deliberately blocked or otherwise rejected
   electrical sweep should produce four low pulses.

During the first energized test, stop immediately if motion accelerates away
from the commanded calibration vector or slew target, phase current is
excessive, the low-side carrier is not near 18.69 kHz, or any sector change
lacks the all-off interval. Those symptoms indicate a failed field capture, an
invalid AS5600 installation, or an incorrect phase-vector mapping.

## Reading, comparing, and flashing with USBasp

AVRA builds firmware; [AVRDUDE](https://github.com/avrdudes/avrdude) communicates
with USBasp. With the ESC separately and safely powered as required by the
board, read flash to Intel HEX with:

```powershell
avrdude -c usbasp -p m8 -U flash:r:ka_nfet_ripped.hex:i
```

`flash.bat` and `flash.sh` do **not** build. They program the `ka_nfet.hex`
alongside them, write the KA board's fuse values (`lfuse=0x3F`, `hfuse=0xCA`),
and verify all three. This permits a production flashing bundle with no source
or assembler installed. The lock byte is left unchanged. To flash an
already-built image manually without changing fuses:

```powershell
avrdude -c usbasp -p m8 -U flash:w:ka_nfet.hex:i
```

The fuse values in `scripts/flash.ps1` are specific to this recovered KA board:
external clock operation, SPI programming, a 512-word boot section, and reset
through the bootloader. Do not reuse them for a different ESC layout without
checking that target's clock and boot configuration.

## Upstream and license

This fork derives from [SimonK `tgy`](https://github.com/sim-/tgy), itself based
on Bernhard Konze's `tp-18a`, and retains the upstream copyright, license terms,
and no-warranty notice in `tgy.asm`. Please preserve those notices in derived
firmware. The original warning remains especially relevant: always test without
propellers, and expect ESC or FET damage while developing switching code.
