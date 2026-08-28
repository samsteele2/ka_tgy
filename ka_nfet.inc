;**************************************************************
;*       Kairos Autonomi custom ESC hardware definition       *
;*             Created by Sam Steele on 8/24/2026             *
;*                                                            *
;* Comments identify the actual PCB schematic net connected   *
;*                    to each ATmega8A pin.                   *
;*                                                            *
;*                    Based on PCB-783B1-02                   *
;**************************************************************

.equ	F_CPU		= 16000000
.equ	USE_INT0	= 1	; PWM command input on PD2 / INT0
.equ	USE_I2C		= 1	; Legacy SimonK slave support; indexing reuses TWI as AS5600 master while stopped
.equ	USE_UART	= 0	; Present in known-good firmware; PD0/RX and PD1/TX are not externally used
.equ	USE_ICP		= 0

.equ	DEAD_LOW_NS		= 300
.equ	DEAD_HIGH_NS	= 300
.equ	COMP_PWM		= 1

; Post-flight blade indexing. These are the settings intended to be changed
; for this board. Controller gains, state values, protocol constants, and
; derived timing live beside the implementation in tgy.asm.
.equ	INDEX_ENABLE			= 1	; Compile the AS5600 home-index feature
.equ	INDEX_DRIVE_ENABLE		= 1	; Set to 0 for sensor-only commissioning
.equ	INDEX_HOME_DELAY_MS		= 3000	; Coast time before normal homing
.equ	INDEX_CALIBRATION_DELAY_MS = 3000	; Coast time before electrical calibration
.equ	INDEX_POLE_PAIRS		= 7	; 12N14P motor: 14 rotor poles / 2
.equ	INDEX_FOC_UPDATE_HZ		= 1000	; Sine-weighted SVM interpolation rate

; Slewed-target position controller. P, I, and D are in sixteenths: 4 means 0.25.
; I accumulates on every update, resets when position error crosses zero, and is
; divided by 32 before I gain is applied. Deadzone is a +/- angular tolerance.
.equ	INDEX_P_GAIN			= 12
.equ	INDEX_I_GAIN			= 2
.equ	INDEX_D_GAIN			= 32	; Measured-motion damping (0.5 command/count/update)
.equ	INDEX_I_MAX				= 16384
.equ	INDEX_HOME_DEADZONE_MINUTES = 180 ; 180 = 3 degrees from home
.equ	INDEX_HOME_SLEW_RPM		= 60	; Mechanical target speed toward home
.equ	INDEX_HOME_MAX_LEAD_ELECTRICAL_DEGREES = 360 ; Target-to-rotor safety bound
.equ	INDEX_HOME_TIMEOUT_SECONDS = 8	; Abort incomplete homing with five low beeps
.equ	INDEX_CAL_NOISE_DELTA	= 3	; Electrical-calibration encoder-noise threshold

; Board-level electrical safety limits. Change these only after checking the
; gate waveform and phase current on the actual hardware. This is a hard ceiling
; and fixed pulse width for sparse calibration excitation. Homing uses its own
; fixed 3.5 us pulse in the separate 20 kHz pulse-density path.
.equ	INDEX_CAL_DUTY_MAX		= 128	; 8.0 us pulse at 6.0 kHz effective / 4.8% average duty
.equ	INDEX_DEADTIME_US		= 5	; All-off delay around a six-step vector change

;*********************
; PORT D definitions *
;*********************
.equ	BnFET		= 7	; PD7: LS_PhaseB
;.equ	sense_x		= 6	; PD6 / AIN0: SenseX (analog-comparator reference input)
.equ	ApFET		= 5	; PD5: HS_PhaseA
.equ	AnFET		= 4	; PD4: LS_PhaseA
.equ	CpFET		= 3	; PD3: HS_PhaseC
.equ	rcp_in		= 2	; PD2 / INT0: PWM input
.equ	txd			= 1	; PD1 / TXD: unused PCB interface; retained for known-good image
;.equ	rxd			= 0	; PD0 / RXD: unused PCB interface

.equ	INIT_PD		= (1<<ApFET)+(1<<CpFET)+(1<<txd)
.equ	DIR_PD		= (1<<AnFET)+(1<<ApFET)+(1<<BnFET)+(1<<CpFET)+(1<<txd)

.equ	AnFET_port	= PORTD
.equ	ApFET_port	= PORTD
.equ	BnFET_port	= PORTD
.equ	CpFET_port	= PORTD

;*********************
; PORT C definitions *
;*********************
; ADC6 and ADC7 are analog-only pins on the ATmega8A package, not PC6/PC7.
; PC6 is /RESET and is connected to the ICSP header.
.equ	mux_b			= 7	; ADC7: SenseB
.equ	mux_a			= 6	; ADC6: SenseA
;.equ	i2c_clk			= 5	; PC5 / SCL: AS5600 clock in index mode
;.equ	i2c_data		= 4	; PC4 / SDA: AS5600 data in index mode
.equ	CnFET			= 3	; PC3: LS_PhaseC
.equ	mux_voltage		= 2	; PC2 / ADC2: battery-voltage divider
.equ	mux_temperature	= 1	; PC1 / ADC1: NTC thermistor
.equ	mux_c			= 0	; PC0 / ADC0: SenseC

.equ	O_POWER			= 220
.equ	O_GROUND		= 51

.equ	INIT_PC			= 0
.equ	DIR_PC			= (1<<CnFET)

.equ	CnFET_port		= PORTC

.MACRO RED_on
	sbi	DDRB, red_led
.ENDMACRO
.MACRO RED_off
	cbi	DDRB, red_led
.ENDMACRO
.MACRO GRN_on
	sbi	DDRB, green_led
.ENDMACRO
.MACRO GRN_off
	cbi	DDRB, green_led
.ENDMACRO

;*********************
; PORT B definitions *
;*********************
;.equ				= 7	; PB7 / XTAL2
;.equ				= 6	; PB6 / XTAL1
;.equ				= 5	; PB5 / SCK: ICSP header
;.equ				= 4	; PB4 / MISO: ICSP header
;.equ				= 3	; PB3 / MOSI: ICSP header
.equ	red_led		= 2	; PB2: WARN LED, N/C on PCB-783B1-02
.equ	green_led	= 1	; PB1: RUN led, N/C on PCB-783B1-02
.equ	BpFET		= 0	; PB0: HS_PhaseB

.equ	INIT_PB		= (1<<BpFET)
.equ	DIR_PB		= (1<<BpFET)

.equ	BpFET_port	= PORTB
