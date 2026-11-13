# Mueang Isara PS2 EE build (gsKit + dmaKit + pad)
EE_BIN  = MueangIsara.elf
EE_OBJS = src/main.o

# ✅ ใช้ชื่อไลบรารีตรงกับของคุณ gskit / dmakit / pad / math
EE_LIBS = -lgskit -ldmakit -lpad -lm

# ✅ เพิ่ม path include ให้ครบถ้วน
EE_INCS += -I$(PS2SDK)/ee/include
EE_INCS += -I$(PS2SDK)/common/include
EE_INCS += -I$(PS2SDK)/ports/include

all: $(EE_BIN)

$(EE_BIN): $(EE_OBJS)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
