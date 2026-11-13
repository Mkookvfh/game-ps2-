# Mueang Isara PS2 EE build (gsKit + dmaKit + pad)
EE_BIN  = MueangIsara.elf
EE_OBJS = src/main.o

# ✅ แก้เป็น -lgsKit (ตรงกับ #include <gsKit.h> ใน main.c)
EE_LIBS = -lgsKit -ldmaKit -lpad -lm

# ✅ เพิ่ม include path ให้ครบ
EE_INCS += -I$(PS2SDK)/ee/include
EE_INCS += -I$(PS2SDK)/common/include
EE_INCS += -I$(PS2SDK)/ports/include

all: $(EE_BIN)

$(EE_BIN): $(EE_OBJS)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
