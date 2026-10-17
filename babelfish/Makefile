include starlet.mk

CFLAGS = -fpic -mbig-endian -mthumb -march=armv5t -mthumb-interwork -fomit-frame-pointer -Os
LDFLAGS += -fpic -mbig-endian -mthumb -march=armv5t -mthumb-interwork -fomit-frame-pointer -Wl,--pic-veneer
LDSCRIPT = babelfish.ld
LIBS = -lgcc

TARGET = babelfish.elf
TARGET_BIN = babelfish.bin
#DATA = vectors.bin.o
OBJS = start.o utils.o vectors.o babelfish.o gecko.o memory.o memory_asm.o exception_asm.o exception.o $(DATA)

include common.mk

all: $(TARGET_BIN)

babelfish.o: babelfish.c #vectors.bin.o

%.bin.o: %.bin
	@$(bin2o)

# vectors contains our SWI replacement that outputs debug stuff over USBGecko
#vectors.bin: vectors.elf
#	$(OBJCOPY) -O binary vectors.elf vectors.bin

#vectors.elf: vectors.o
#	$(LD) -EB -Ttext=0 -o vectors.elf vectors.o
        
#vectors.o: vectors.s
#	$(AS) -mbig-endian $(ASFLAGS) -o vectors.o vectors.s


$(TARGET_BIN): $(TARGET)
	@echo  "  OBJCPY    $@"
	@$(OBJCOPY) -O binary $< $@

clean: myclean

myclean:
	-rm -f $(TARGET_BIN) vectors.elf vectors.bin vectors_bin.h vectors.o babelfish.map
