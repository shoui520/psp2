/*
 * The stack guard of libsas's GCC code.  Linked statically into an SNC module (ScePaf), it
 * checks its frames against a guard and a failure handler of its own, from the GCC runtime
 * linked with it, not against SceLibKernel's imported __stack_chk_guard and __stack_chk_fail
 * that the module's SNC code uses: GCC's references are bound to those.
 */
#ifndef SCE_SAS_AS_SSP_H
#define SCE_SAS_AS_SSP_H

#ifdef SCE_SAS_STATIC_RUNTIME
__asm__(".set __stack_chk_guard, sceSasAsStackChkGuard\n"
	".set __stack_chk_fail, sceSasAsStackChkFail\n");
#endif

#endif
