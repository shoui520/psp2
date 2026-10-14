/*
 * Exact-width integer types for the Marvell 88W8787 ("Robin") ARMv5TE firmware.
 */
#pragma once

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
typedef int intptr_t;
typedef unsigned int uintptr_t;

#define INT8_MIN	(-128)
#define INT8_MAX	127
#define UINT8_MAX	0xffu
#define INT16_MIN	(-32768)
#define INT16_MAX	32767
#define UINT16_MAX	0xffffu
#define INT32_MIN	(-0x7fffffff - 1)
#define INT32_MAX	0x7fffffff
#define UINT32_MAX	0xffffffffu
#define UINT64_MAX	0xffffffffffffffffull
