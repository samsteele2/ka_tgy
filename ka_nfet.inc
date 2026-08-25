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

; Post-flight blade indexing. Index mode uses AS5600-sensored six-step
; commutation. One high-side source is held on while the selected low-side
; sink is switched by SimonK's normal ultrasonic Timer2 PWM.
.equ	INDEX_ENABLE			= 1
.equ	INDEX_DRIVE_ENABLE		= 1
.equ	INDEX_DELAY_OVF			= 1221	; 1221 * 4096 us = 5.001216 s
.equ	INDEX_AS5600_ADDR_W		= 0x6c	; AS5600 7-bit address 0x36 + write bit
.equ	INDEX_AS5600_ADDR_R		= 0x6d
.equ	INDEX_AS5600_ANGLE_REG	= 0x0e	; Filtered 12-bit ANGLE register
.equ	INDEX_TWBR				= 12	; 400 kHz SCL at 16 MHz, prescaler = 1
.equ	INDEX_HOME_MARKER		= 0xa5	; EEPROM marker: home captured during throttle calibration
.equ	INDEX_ELECTRICAL_MARKER = 0x5a	; EEPROM marker: automatic electrical calibration is valid
.equ	INDEX_CAL_DUTY_MIN		= 56	; 3.5 us low-side pulse, SimonK's normal minimum duty
.equ	INDEX_CAL_DUTY_MAX		= 128	; Conservative 8.0 us / 14.95% calibration ceiling
.equ	INDEX_CAL_DUTY_STEP		= 4	; Timer cycles added per breakaway trial
.equ	INDEX_CAL_DUTY_MARGIN	= 8	; Added after encoder motion is detected
.equ	INDEX_CAL_MOVE_COUNTS	= 4	; Encoder counts required to prove breakaway
.equ	INDEX_CAL_RAMP_STEP_TICKS = 12	; Increase duty every 49.152 ms
.equ	INDEX_CAL_BASE_HOLD_TICKS = 122	; Establish vector-zero position at minimum duty
.equ	INDEX_CAL_ALIGN_HOLD_TICKS = 122	; Hold vector zero for 499.712 ms
.equ	INDEX_CAL_SWEEP_STEP		= 8	; Electrical counts per 4.096 ms service
.equ	INDEX_CAL_SWEEP_TICKS	= 4096 / INDEX_CAL_SWEEP_STEP
.equ	INDEX_CAL_END_HOLD_TICKS = 122	; Settle after each electrical revolution
.equ	INDEX_CAL_MAX_POLE_PAIRS = 20	; Accepted automatic pole-pair range is 1..20
.equ	INDEX_CAL_MIN_TRAVEL	= 180	; Reject a sweep too small for 20 pole pairs
.equ	INDEX_CAL_TRAVEL_MATCH	= 32	; Forward/reverse mechanical-count agreement
.equ	INDEX_CAL_RETURN_TOLERANCE = 32	; Return-to-start mechanical-count agreement
.equ	INDEX_CAL_POLE_FIT		= 64	; Maximum p*travel error from 4096 counts
.if (4096 % INDEX_CAL_SWEEP_STEP)
.error "INDEX_CAL_SWEEP_STEP must divide one electrical revolution"
.endif
.equ	INDEX_SLEW_RPM			= 15
.equ	INDEX_CONTROL_PERIOD_US = 4096	; One Timer1 overflow, approximately 244.14 Hz
.equ	INDEX_SLEW_STEP_Q8		= 1074	; 4.195 counts/update = 15.004 RPM
.equ	INDEX_POSITION_TOLERANCE = 2	; AS5600 counts
.equ	INDEX_PWM_CARRIER_HZ	= 18691	; F_CPU / 856 for this 16 MHz SimonK build
.equ	INDEX_DEADTIME_US		= 5	; All-off delay before changing six-step vectors
.equ	INDEX_DEADTIME_CYCLES	= (F_CPU / 1000000) * INDEX_DEADTIME_US
.equ	INDEX_DEADTIME_LOOPS	= (INDEX_DEADTIME_CYCLES + 1) / 3	; dec/brne loop is 3n-1 cycles
.equ	INDEX_ARMED				= 0
.equ	INDEX_WAITING			= 1
.equ	INDEX_ACTIVE			= 2
.equ	INDEX_ANGLE_VALID		= 3
.equ	INDEX_CONTROL_DUE		= 4
.equ	INDEX_PWM_RUNNING		= 5
.equ	INDEX_CALIBRATING		= 6

.equ	INDEX_CAL_BASE_HOLD		= 1
.equ	INDEX_CAL_BREAKAWAY		= 2
.equ	INDEX_CAL_ALIGN_HOLD	= 3
.equ	INDEX_CAL_SWEEP_FORWARD = 4
.equ	INDEX_CAL_FORWARD_HOLD = 5
.equ	INDEX_CAL_SWEEP_REVERSE = 6
.equ	INDEX_CAL_FINAL_HOLD	= 7

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
