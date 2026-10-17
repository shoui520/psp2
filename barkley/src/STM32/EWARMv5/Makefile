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
# Compiler: IAR Embedded Workbench for ARM, C++ (the reset path is IAR's __cmain:
# __low_level_init, __iar_data_init3, __call_ctors, main); version not identified. A
# Windows program, run like psp2snc.exe (tools/host.py). The flash code uses ST's
# STM32F10x Standard Peripheral Library, to be vendored under contrib/ once its
# version is pinned. Skeleton: nothing builds yet.

IMAGE ?= 01

include image/$(IMAGE).mk

DEFINES := -DMOTION_IMAGE=$(MOTION_IMAGE)
INCLUDES := -I../motion/motion/include -I../motion/motion

all:
	@echo "motionC is a skeleton: no IAR toolchain yet" >&2; exit 1

.PHONY: all
