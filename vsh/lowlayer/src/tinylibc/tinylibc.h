/*
 * SceTinyLibc: the small C runtime the shell's libraries link instead of the C library (its
 * lock is named "SceTinyLibc"; liblocation_extension, liblocation_factory, liblocation_internal,
 * trigger_util and libipmi_nongame carry it). Nothing names its files or functions; the folder,
 * the file and the names are ours.
 */
#ifndef TINYLIBC_TINYLIBC_H
#define TINYLIBC_TINYLIBC_H

#ifdef __cplusplus
extern "C" {
#endif

/* tinylibc.c: the lock, __dso_handle, then the constructors; module_start calls it first */
void tinyLibcInit(void);

#ifdef __cplusplus
}
#endif

#endif
