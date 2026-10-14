/*
 * The stack guard, and the constructors and destructors of a secure module: the
 * function pointers between the link script's __ctors_start/__ctors_end and
 * __dtors_start/__dtors_end.
 *
 * Skeleton. The bodies are not written yet.
 */

#include "sm.h"

/* the stack protector's guard, set by the entry from Bigmac's random number */
unsigned int __stack_chk_guard;

void call_ctors(void)
{
}

void call_dtors(void)
{
}
