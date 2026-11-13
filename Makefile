# Minimal PS2SDK EE build for the Mueang Isara boot test.
# This starter program does not use gsKit, dmaKit, pad, or libm.
EE_BIN  = MueangIsara.elf
EE_OBJS = src/main.o
EE_LIBS =
EE_INCS += -I$(PS2SDK)/ee/include

all: $(EE_BIN)

$(EE_BIN): $(EE_OBJS)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
