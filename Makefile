# PS2SDK EE build. Requires PS2SDK environment and gsKit installed.
EE_BIN  = MueangIsara.elf
EE_OBJS = src/main.o
EE_LIBS = -lgskit -ldmakit -lpad -lm
EE_INCS += -I$(PS2SDK)/ee/include -I$(PS2SDK)/ee/gs/include

all: $(EE_BIN)

$(EE_BIN): $(EE_OBJS)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
