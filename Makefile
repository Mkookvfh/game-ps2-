EE_BIN = MueangIsara.elf
EE_OBJS = src/main.o

EE_INCS += -I$(GSKIT)/include
EE_LIBS += -lgsKit -ldmakit -lpad -lm
EE_CFLAGS += -O2

all: $(EE_BIN)
	$(EE_STRIP) $(EE_BIN)

clean:
	rm -f src/*.o $(EE_BIN)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
