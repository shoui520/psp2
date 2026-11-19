/*
 * setjmp.h: the SDK's nonlocal jumps (SDK Release 03.570.011, target/include/setjmp.h).
 */
#ifndef _SETJMP
#define _SETJMP

#ifdef __cplusplus
extern "C" {
#endif

typedef long long jmp_buf[24];

void longjmp(jmp_buf env, int val);
int setjmp(jmp_buf env);

#ifdef __cplusplus
}
#endif

#endif
