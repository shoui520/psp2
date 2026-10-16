#ifndef _COMMON_RUNTIME_UNWIND_PR_H
#define _COMMON_RUNTIME_UNWIND_PR_H

#ifdef __cplusplus
extern "C" {
#endif

/* EHABI personality routines (SNC runtime, libsnc simple_unwind_pr.o) */
int __aeabi_unwind_cpp_pr0(int state, void *ucbp, void *context);
int __aeabi_unwind_cpp_pr1(int state, void *ucbp, void *context);
int __aeabi_unwind_cpp_pr2(int state, void *ucbp, void *context);

#ifdef __cplusplus
}
#endif

#endif /* _COMMON_RUNTIME_UNWIND_PR_H */
