# Sony built it at -O3 (the packet checksums are vectorized), with no
# constant-propagated clones, and calls kept out of line.
set(MODULE_COMPILE_OPTIONS -O3 -fno-inline-functions -fno-inline-small-functions -fno-ipa-cp-clone)
psp2_module(os0_kd_syscon KIND kernel COMPILER gcc48 SOURCES
	syscon.c
	battery.c
)
