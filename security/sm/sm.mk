# Make rules for one secure module, included by security/sm/<module>/Makefile after it
# sets MODULE and SOURCES. GCC, BINUTILS, O and FW come from security/sm/Makefile.
#
# The modules are little-endian MeP C5 with the IVC2 coprocessor. Sony's compiler is not
# identified yet; this builds with GCC 4.5.4 and binutils 2.27 for mep-elf.

SM := $(dir $(lastword $(MAKEFILE_LIST)))

CC := $(GCC)gcc
LD := $(BINUTILS)ld

ifeq ($(FW),4.00-DEBUGGER)
FW_DEFINES := -DFW_VERSION=0x04000000 -DFW_DEBUG=1 -DFW_CEX=0 -DFW_TOOL=0
else ifeq ($(FW),3.630.011-DEBUGGER)
FW_DEFINES := -DFW_VERSION=0x03630011 -DFW_DEBUG=1 -DFW_CEX=0 -DFW_TOOL=0
else ifeq ($(FW),3.650.011-TOOL)
FW_DEFINES := -DFW_VERSION=0x03650011 -DFW_DEBUG=0 -DFW_CEX=0 -DFW_TOOL=1
else
FW_DEFINES := -DFW_VERSION=0x03650000 -DFW_DEBUG=0 -DFW_CEX=1 -DFW_TOOL=0
endif

INCLUDES := -I. -I$(SM)include -Iinclude -I$(SM)../../cmake/include -I$(SM)../../sys/sdk/target/include -I$(SM)../../sys/internal/include
CFLAGS := -mel -O2 -std=gnu99 -Wall -fno-strict-aliasing -ffreestanding -fno-builtin $(FW_DEFINES) $(INCLUDES) $(EXTRA_CFLAGS)
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
