#ifndef PAF_ATEXIT_H
#define PAF_ATEXIT_H

/* Sony: __cxa_atexit, __aeabi_atexit and __cxa_finalize. */
int __cxa_atexit(void (*func)(void *), void *arg, void *dso);
int __aeabi_atexit(void *object, void (*destroy)(void *), void *dso);
int __cxa_finalize(void *dso);

#endif
