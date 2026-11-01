#ifndef PAF_GUARD_H
#define PAF_GUARD_H

#include <stdint.h>

/* Sony: __cxa_guard_acquire, __cxa_guard_release and __cxa_guard_abort. */
int __cxa_guard_acquire(uint32_t *guard);
int __cxa_guard_release(uint32_t *guard);
int __cxa_guard_abort(uint32_t *guard);

#endif
