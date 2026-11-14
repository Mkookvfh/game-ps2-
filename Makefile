
EE_BIN  = GraphTest.elf
EE_OBJS = graph_test.o

EE_INCS += -I$(PS2SDK)/ee/include
EE_INCS += -I$(PS2SDK)/common/include

EE_LIBS += -lgraph

all: $(EE_BIN)

$(EE_BIN): $(EE_OBJS)

clean:
	rm -f GraphTest.elf GSKitTest.elf graph_test.o gs_test.o

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
