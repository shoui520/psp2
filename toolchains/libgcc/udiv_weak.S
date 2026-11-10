/*
 * __aeabi_uidiv for a module that links GCC's runtime beside SNC's (libsnc): __udivsi3, which
 * SNC's runtime also defines, is weak here, so that SNC's code reaches SNC's and GCC's code
 * this one by __aeabi_uidiv.
 */
#define L_udivsi3
#include "lib1funcs.S"
	.weak	__udivsi3
