# motionC: the application of the PS Vita's motion MCU (PUP package type 0xc), an ST
# STM32F10x medium-density Cortex-M3 with 64 KiB of flash:
#   0x08000000  the MCU's boot loader, 0x3000 bytes (not in the package)
#   0x08003000  this application, to 0x0800f3ff
#   above       calibration blocks the application writes (Flash::Write)
# The package holds a 16-byte header and the image with every byte inverted.
#
#   make IMAGE=00|01
#
# image/<NN>.mk selects an image: 00 (AMI304 and AK8975), 01 (AK8975).
#   motionC.icf   IAR link configuration (flash, RAM, vector table first)
#
# Sony's layout (barkley trunk, from the images' assert paths): this folder is the IAR
# project, src/STM32/EWARMv5. The code (the start-up and the C++ classes the debug
# messages name) is in ../motion/motion, its headers in ../motion/motion/include, which is
# on the include path (the asserts name EWARMv5\..\motion\motion\include\MotionFrameUtil.h).
#
# Compiler: Sony built it with IAR Embedded Workbench for ARM 5, C++ (the reset path is
# IAR's __cmain: __low_level_init, __iar_data_init3, __call_ctors, main). IAR's compiler
# is not available here; the sources are compiled with the project's GCC 4.8 for the
# Cortex-M3 (tools/gcc48.sh), and each function is checked against Sony's for the same
# behaviour. The flash code uses ST's STM32F10x Standard Peripheral Library.

IMAGE ?= 01
O ?= build/$(IMAGE)

include image/$(IMAGE).mk

ROOT := ../../../..
GCC48 := $(ROOT)/tools/gcc48.sh
MOTION := ../motion/motion

# ST's STM32F10x Firmware Library V2.0.3 (contrib/), with Sony's changes
# (../Libraries/fwlib.patch) applied to a copy in the build folder; its configuration,
# stm32f10x_conf.h, is Sony's (../motion/motion/include)
FWLIB := $(ROOT)/contrib/stm32f10x-fwlib-2.0.3/library
LIB := $(O)/library

DEFINES := -DMOTION_IMAGE=$(MOTION_IMAGE)
# IAR's C library (DLib): our headers and code for what the image links of it
INCLUDES := -I$(MOTION)/include -I$(MOTION) -I$(LIB)/inc -Iruntime/include
# IAR's enums take the smallest type that holds them (-fshort-enums)
TARGET := -mcpu=cortex-m3 -mthumb -fshort-enums -fno-section-anchors -MMD -MP
CFLAGS := $(TARGET) -O2 -ffreestanding -fno-common -std=gnu99 $(DEFINES) $(INCLUDES)
# IAR kept the library's static helpers (FLASH's delay) as functions
LIBCFLAGS := $(CFLAGS) -fno-inline
# IAR called the small member functions (no inlining), did not turn calls into jumps, and
# compiled a switch as compares
CXXFLAGS := $(TARGET) -O2 -ffreestanding -fno-common -fno-exceptions -fno-rtti \
	-fno-threadsafe-statics -fno-use-cxa-atexit -fno-inline -fno-optimize-sibling-calls -fno-ipa-sra -fno-ipa-cp \
	-fno-jump-tables \
	$(DEFINES) $(INCLUDES)

# in the image's order: the program's (../motion/motion), the library's drivers
SOURCES := $(MOTION)/MotionProcessor.cpp $(MOTION)/MotionHif.cpp $(MOTION)/AK8975.cpp $(MOTION)/MotionProtoPacketEncoder.cpp \
	$(MOTION)/stm32f10x_it.cpp $(MOTION)/I2cMemIf.cpp $(MOTION)/Spi.cpp $(MOTION)/Usart.cpp $(MOTION)/Dma.cpp $(MOTION)/Board.cpp $(MOTION)/BufferedUsartStream.cpp $(MOTION)/I2c.cpp $(MOTION)/Gpio.cpp $(MOTION)/HifSpi.cpp $(MOTION)/AdcSampler.cpp $(MOTION)/System.cpp $(MOTION)/I2cRegIf.cpp $(MOTION)/Runtime.cpp $(MOTION)/Adc.cpp $(MOTION)/TimerSlot.cpp $(MOTION)/SampleTimer.cpp $(MOTION)/Flash.cpp $(MOTION)/Dispatcher.cpp $(MOTION)/Timer.cpp
LIBSOURCES := stm32f10x_rcc.c stm32f10x_flash.c stm32f10x_gpio.c stm32f10x_nvic.c stm32f10x_exti.c stm32f10x_adc.c stm32f10x_tim.c stm32f10x_i2c.c stm32f10x_dma.c stm32f10x_spi.c stm32f10x_usart.c stm32f10x_systick.c stm32f10x_pwr.c stm32f10x_iwdg.c
# IAR's C library (DLib), what the image links of it
RUNTIME := xprintf.c

OBJECTS := $(patsubst %,$(O)/%.o,$(notdir $(SOURCES)) $(LIBSOURCES) $(RUNTIME))

all: $(OBJECTS)

$(OBJECTS): $(LIB)/.patched Makefile

# the headers each object read
-include $(OBJECTS:.o=.d)

# the library's copy with Sony's changes
fwlib: $(LIB)/.patched
$(LIB)/.patched: ../Libraries/fwlib.patch $(wildcard $(FWLIB)/*/*)
	rm -rf $(LIB)
	mkdir -p $(O)
	cp -R $(FWLIB) $(LIB)
	cd $(LIB) && patch -s -p1 < $(CURDIR)/../Libraries/fwlib.patch
	touch $@

$(O)/%.c.o: $(LIB)/src/%.c
	@mkdir -p $(O)
	$(GCC48) $(LIBCFLAGS) -c $< -o $@

$(O)/%.c.o: $(MOTION)/%.c
	@mkdir -p $(O)
	$(GCC48) $(CFLAGS) -c $< -o $@

$(O)/%.c.o: runtime/%.c
	@mkdir -p $(O)
	$(GCC48) $(CFLAGS) -c $< -o $@

$(O)/%.cpp.o: $(MOTION)/%.cpp
	@mkdir -p $(O)
	GCC48_DRIVER=g++ $(GCC48) $(CXXFLAGS) -c $< -o $@

# the compile commands' options, for barkleymatch.py
flags:
	@echo "CFLAGS=$(CFLAGS)"
	@echo "CXXFLAGS=$(CXXFLAGS)"
	@echo "LIBCFLAGS=$(LIBCFLAGS)"
	@echo "LIB=$(LIB)"

.PHONY: all flags fwlib
