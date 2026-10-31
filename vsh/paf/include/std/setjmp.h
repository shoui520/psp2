/* paf's non-local jumps (ScePafStdc), over the C library's jmp_buf. */
#ifndef PAF_STD_SETJMP_H
#define PAF_STD_SETJMP_H

#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

int sce_paf_setjmp(jmp_buf env);
void sce_paf_longjmp(jmp_buf env, int status);

#ifdef __cplusplus
}
#endif

#endif
