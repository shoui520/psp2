/* Standard ARM run-time ABI helpers implemented by this firmware. */
#ifndef ROBIN_RUNTIME_H
#define ROBIN_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

int __aeabi_uwrite4(int value, void *address);
/* RVCT library routines in ITCM (runtime/string.c, runtime/stringarm.c, runtime/rand.c). */
char *strcpy(char *destination, const char *source);
int memcmp(const void *left, const void *right, size_t size);
void *memset(void *dest, int value, size_t size);

char *strncpy(char *destination, const char *source, size_t size);

int strcmp(const char *left, const char *right);

void srand(unsigned int seed);
int rand(void);
void robinInitHeapPool(void);

/* The C library's start-up and exit (boot/reset.S, runtime/rtlib.c, runtime/rtlibarm.c). */
#define RT_LIBSPACE_WORDS	24
extern uint32_t __libspace_start[RT_LIBSPACE_WORDS];		/* 0x5ae54 */
void exit(int status);						/* 0x525e4 */
void __rt_exit(int status);					/* 0x5252c */
int __rt_lib_shutdown(void);					/* 0x52768 */
void __rt_lib_init_noop(void);					/* 0x52820 */
int *__rt_get_errno_addr(void);					/* 0x52854 */
void __rt_clear_errno(void);					/* 0x53cd0 */

#endif /* ROBIN_RUNTIME_H */
