# KA nFET firmware build. Compatible with BSD make and GNU make.
# This private fork intentionally builds no historical SimonK board targets.

.DEFAULT_GOAL := all

AVRA ?= avra
TARGET := ka_nfet
HEX := $(TARGET).hex
SOURCES := tgy.asm ka_nfet.inc boot.inc m8def.inc

.PHONY: all build test clean help FORCE

all: $(HEX)

build: $(HEX)

# There are no host-side unit tests; this is a clean assembly/size validation.
test: $(HEX)

# FORCE deliberately rebuilds the checked-in release image for every invocation.
$(HEX): FORCE $(SOURCES) Makefile
	$(AVRA) -fI -D ka_nfet_esc -I . -I other_escs tgy.asm
	mv -f tgy.hex $(HEX)
	rm -f tgy.eep.hex tgy.obj tgy.cof tgy.map tgy.lst

FORCE:

# Keep the reviewed/generated release image. Remove AVRA intermediates only.
clean:
	rm -f tgy.hex tgy.eep.hex tgy.obj tgy.cof tgy.map tgy.lst

help:
	@echo "make, make build, or make test: build $(HEX)"
	@echo "make clean: remove AVRA intermediates (keeps $(HEX))"
