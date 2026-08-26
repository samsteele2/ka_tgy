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
.equ	INDEX_START_DELAY_SECONDS = 5	; Coast time after throttle reaches zero
.equ	INDEX_SLEW_RPM			= 15	; Maximum mechanical homing speed

; Motion-controller tuning. P and D act on encoder counts per 4.096 ms update.
; I acts on the final-only error accumulator after its internal /32 scaling.
; FF is pulse-density command per target encoder-count/update (64 is nominal).
.equ	INDEX_P_GAIN			= 1	; Tracking-error stiffness (previously 12)
.equ	INDEX_I_GAIN			= 1	; Final static-error correction
.equ	INDEX_D_GAIN			= 0	; Measured-motion damping (previously 16)
.equ	INDEX_I_MAX			= 4096	; Absolute final-error accumulator clamp
.equ	INDEX_FF_GAIN			= 0	; Target-rate feed-forward

; Board-level electrical safety limits. Change these only after checking the
; gate waveform and phase current on the actual hardware.
.equ	INDEX_CAL_DUTY_MAX		= 128	; 8.0 us low-side pulse / 16% at 20 kHz
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
