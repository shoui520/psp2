/* Confirmed private error definitions shared by C and named Thumb wrappers. */
#pragma once

#include <kernel/error.h>

/* SCE_KERNEL_ERROR_UNKNOWN_EVF_ID as a syscall returns it */
#define USER_ERROR_UNKNOWN_EVENT_FLAG_ID	0xc00280e1
#define USER_ERROR_UNKNOWN_EVENT_ID		0xc0028201
