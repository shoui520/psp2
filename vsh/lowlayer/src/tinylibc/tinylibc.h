/*
 * SceTinyLibc: the small C runtime the shell's libraries link instead of the C library (its
 * lock is named "SceTinyLibc"; liblocation_extension, liblocation_factory, liblocation_internal,
 * trigger_util and libipmi_nongame carry it), a library of objects of which each module links what
 * it uses. Nothing names its files or functions; the folder, the files and the names are ours.
 */
#ifndef TINYLIBC_TINYLIBC_H
#define TINYLIBC_TINYLIBC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* tinylibc.c: the lock, __dso_handle, then the constructors; module_start calls it first */
void tinyLibcInit(void);
/* term.c: the module's exit handlers, then the lock; module_stop calls it */
void tinyLibcTerm(void);

/* aeabi_atexit.c: the destroyer of a static object, for __cxa_finalize */
int __aeabi_atexit(void *object, void (*destroyer)(void *), void *dso);
/* cxa_atexit.c */
void __cxa_finalize(void *dso);

/* alloc.c */
void *malloc(size_t size);
/* malloc.c */
void free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif
