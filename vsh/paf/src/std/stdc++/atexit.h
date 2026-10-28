#ifndef PAF_ATEXIT_H
#define PAF_ATEXIT_H

/* Sony: __cxa_atexit, __aeabi_atexit and __cxa_finalize. */
int ScePafStdc_AD9500EF(void (*func)(void *), void *arg, void *dso);
int ScePafStdc_C95732BB(void *object, void (*destroy)(void *), void *dso);
int ScePafStdc_F36BABE3(void *dso);

#endif
