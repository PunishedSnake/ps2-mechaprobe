EE_BIN = PS2_MECHAPROBE.ELF
EE_OBJS = main.o platform.o ui.o probe.o sha256.o
EE_LIBS = -ldebug -lpad -lfileXio -lpatches -lsecr -lcdvd -lkernel
EE_CFLAGS = -O2 -G0 -Wall -Wextra -Werror -std=gnu99 -fdata-sections -ffunction-sections -Iinclude
EE_LDFLAGS = -Wl,--gc-sections

# Keep the probe independent from whichever IOP image launched it. The order
# mirrors the hardware-tested fhdb-bootstrap-manager startup chain, minus the
# HDD/DEV9 modules that Mecha Probe does not need.
IRX_FILES = iomanX.irx fileXio.irx secrman.irx freesio2.irx freepad.irx \
	mcman.irx mcserv.irx secrsif.irx bdm.irx bdmfs_fatfs.irx usbd.irx \
	usbmass_bd.irx
EE_OBJS += $(IRX_FILES:.irx=_irx.o)

all: $(EE_BIN)

release: $(EE_BIN)
	$(EE_STRIP) --strip-all $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS) $(IRX_FILES:.irx=_irx.c)

main.o: src/main.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

platform.o: src/platform.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

ui.o: src/ui.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

probe.o: src/probe.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

sha256.o: src/sha256.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

%_irx.c:
	$(PS2SDK)/bin/bin2c $(PS2SDK)/iop/irx/$*.irx $@ $*_irx

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

.PHONY: all release clean
