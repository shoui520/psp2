# motionC image 00: Sony's branch U0652 (its assert path: barkley\branch\U0652), the
# AK8975 magnetometer; also in 0.990. Its s8 is a plain char (unsigned): the branch's
# library patch.
# Package header: id 0x52001a10, +4 = 6, size 0xc400,
# +8 0x00300000, +0xc 0x00418608. Reset 0x0800a325, stack 0x20000400.
MOTION_IMAGE := 0
LIBPATCHES := ../Libraries/fwlib-u0652.patch
