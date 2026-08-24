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
.equ	USE_I2C		= 1	; Present in known-good firmware; PC4/SDA and PC5/SCL are currently unused
.equ	USE_UART	= 1	; Present in known-good firmware; PD0/RX and PD1/TX are not externally used
.equ	USE_ICP		= 0

.equ	DEAD_LOW_NS		= 300
.equ	DEAD_HIGH_NS	= 300
.equ	COMP_PWM		= 1

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
;.equ	i2c_clk			= 5	; PC5 / SCL: currently unused; reserved for future AS5600
;.equ	i2c_data		= 4	; PC4 / SDA: currently unused; reserved for future AS5600
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
	sbi	DDRC, red_led
.ENDMACRO
.MACRO RED_off
	cbi	DDRC, red_led
.ENDMACRO
.MACRO GRN_on
	sbi	DDRC, green_led
.ENDMACRO
.MACRO GRN_off
	cbi	DDRC, green_led
.ENDMACRO

;*********************
; PORT B definitions *
;*********************
;.equ				= 7	; PB7 / XTAL2
;.equ				= 6	; PB6 / XTAL1
;.equ				= 5	; PB5 / SCK: ICSP header
;.equ				= 4	; PB4 / MISO: ICSP header
;.equ				= 3	; PB3 / MOSI: ICSP header
.equ	red_led		= 2	; PB2: WARN led, N/C on PCB-783B1-02
.equ	green_led	= 1	; PB1: RUN led, N/C on PCB-783B1-02
.equ	BpFET		= 0	; PB0: HS_PhaseB

.equ	INIT_PB		= (1<<BpFET)
.equ	DIR_PB		= (1<<BpFET)

.equ	BpFET_port	= PORTB