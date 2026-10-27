/*
 * NSKBL keeps none of the unwinder's functions that nothing in it calls. Declared static
 * before GCC's headers declare them, they stay file-local, and the compiler drops them, as
 * toolchains/libgcc/unwind_pr.c does with __gnu_unwind_frame. (File name ours.)
 */

struct _Unwind_Context;
struct _Unwind_Control_Block;

#ifndef NSKBL_UNWIND_PR
/* gcc48-unwind.c */
static __attribute__((unused)) void _Unwind_DeleteException(struct _Unwind_Control_Block *);
static __attribute__((unused)) unsigned int _Unwind_GetCFA(struct _Unwind_Context *);
static __attribute__((unused)) void _Unwind_Complete(struct _Unwind_Control_Block *);
#else
/* unwind_pr.c */
static __attribute__((unused)) unsigned int _Unwind_GetRegionStart(struct _Unwind_Context *);
static __attribute__((unused)) void *_Unwind_GetLanguageSpecificData(struct _Unwind_Context *);
static __attribute__((unused)) unsigned int _Unwind_GetDataRelBase(struct _Unwind_Context *);
static __attribute__((unused)) unsigned int _Unwind_GetTextRelBase(struct _Unwind_Context *);
#endif
