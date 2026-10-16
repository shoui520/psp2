/*
 * The exact-width integer types. GCC 4.5 for MeP does not tell its stdint.h the
 * target's types, so they are given here; int and long are 32 bits.
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

#define INT32_MAX	0x7fffffff
#define UINT32_MAX	0xffffffffu
