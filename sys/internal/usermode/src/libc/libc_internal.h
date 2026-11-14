/*
 * libc_internal.h: SceLibc's functions that its files share and that no SDK header declares.
 * The names are ours where Dinkumware's headers give none.
 */
#ifndef _LIBC_INTERNAL_H
#define _LIBC_INTERNAL_H

#include <stddef.h>

/* module.c */
extern void *__dso_handle;
extern int _ExitInProgress;

/* the heap */
int _MallocInit(void);
int _MallocTerm(void);

/* the library's locks */
int _InitLocks(void);
int _TermLocks(void);

/* the thread-local storage lock */
int _InitTlsMutex(void);
int _TermTlsMutex(void);

/* the standard streams */
void _InitStdFiles(void);
void _Fcloseall(int exiting);

/* C++ termination */
int __cxa_atexit(void (*func)(void *), void *arg, void *dso);
void __cxa_finalize(void *dso);

/* the thread-local data of the C library (sceKernelGetTLSAddr slots) */
#include <wchar.h>
mbstate_t *_Getmbrlenstate(void);
mbstate_t *_Getmbrtowcstate(void);
mbstate_t *_Getmbsrtowcsstate(void);
mbstate_t *_Getmbtowcstate(void);
mbstate_t *_Getwcrtombstate(void);
mbstate_t *_Getwcsrtombsstate(void);
mbstate_t *_Getwctombstate(void);
mbstate_t *_Getwctomb_sstate(void);

/* the message for an error code, in buf or a static buffer when buf is NULL */
char *_Strerror(int errcode, char *buf);

#endif /* _LIBC_INTERNAL_H */
