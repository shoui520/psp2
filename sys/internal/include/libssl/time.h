/*
 * libssl's time: what SceSslInternal_4AAF0D45 reads the clock into and
 * SceSslInternal_6D2A86F0 encodes.
 */

#ifndef _LIBSSL_TIME_H
#define _LIBSSL_TIME_H

#include <scetypes.h>

typedef struct SslTime {
	SceInt32 secondsHigh;
	SceUInt32 secondsLow;
	SceUInt32 milliseconds;
} SslTime;

#endif /* _LIBSSL_TIME_H */
