/*
 * __aeabi_idiv for a module that links GCC's runtime beside SNC's (libsnc): __divsi3, which
 * SNC's runtime also defines, is weak here, so that SNC's code reaches SNC's and GCC's code
 * this one by __aeabi_idiv.
 */
#define L_divsi3
#include "lib1funcs.S"
	.weak	__divsi3
