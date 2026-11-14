# Mueang Isara PS2 EE build (gsKit + dmaKit + pad)
EE_BIN  = MueangIsara.elf
EE_OBJS = src/main.o

EE_INCS += -I$(GSKIT)/include
EE_INCS += -I$(PS2SDK)/ee/include
EE_INCS += -I$(PS2SDK)/common/include
EE_INCS += -I$(PS2SDK)/ports/include


EE_LIBS += -L$(GSKIT)/lib -lgskit -ldmakit -lpad -lm


all: $(EE_BIN)

$(EE_BIN): $(EE_OBJS)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
