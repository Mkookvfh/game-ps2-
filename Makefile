
EE_BIN = MueangIsara.elf

EE_OBJS = src/scene3d.o

EE_INCS += -I$(GSKIT)/include

EE_LDFLAGS += -L$(GSKIT)/lib

EE_LIBS += -lgskit -ldmakit -lm

.PHONY: all clean

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
