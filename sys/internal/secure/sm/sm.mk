# Make rules for one secure module, included by sys/internal/secure/sm/<module>/Makefile after it
# sets MODULE and SOURCES. GCC, BINUTILS, O and FW come from sys/internal/secure/sm/Makefile.
#
# The modules are little-endian MeP C5 with the IVC2 coprocessor. Sony's compiler is not
# identified yet; this builds with GCC 4.5.4 and binutils 2.27 for mep-elf.

SM := $(dir $(lastword $(MAKEFILE_LIST)))

CC := $(GCC)gcc
LD := $(BINUTILS)ld

INCLUDES := -I$(SM)include -Iinclude
CFLAGS := -mel -O2 -std=gnu99 -Wall -ffreestanding -fno-builtin $(INCLUDES)
ASFLAGS := -mel $(INCLUDES)

SM_SOURCES := $(SM)crt0.S $(SM)init.c
OBJ := $(O)/$(FW)/$(MODULE)
OBJS := $(patsubst %,$(OBJ)/%.o,$(notdir $(basename $(SM_SOURCES) $(SOURCES))))

vpath %.c $(SM) $(sort $(dir $(SOURCES)))
vpath %.S $(SM) $(sort $(dir $(SOURCES)))

.PHONY: all
all: $(OBJ)/$(MODULE).elf

$(OBJ)/%.o: %.S $(wildcard $(SM)include/*.h include/*.h)
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(OBJ)/%.o: %.c $(wildcard $(SM)include/*.h include/*.h)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ)/$(MODULE).elf: $(OBJS) $(SM)sm.ld
	$(LD) -T $(SM)sm.ld -o $@ $(OBJS)
