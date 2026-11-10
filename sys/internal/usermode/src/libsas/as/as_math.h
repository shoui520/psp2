/* The AS runtime's single-precision math (as_math.c), without the C library. */
#ifndef SCE_SAS_AS_MATH_H
#define SCE_SAS_AS_MATH_H

#include <scetypes.h>

#define SCE_SAS_AS_PI		3.14159274f
#define SCE_SAS_AS_HALF_PI	1.57079637f
#define SCE_SAS_AS_TWO_PI	6.28318548f

float sceSasAsFabsf(float x);
float sceSasAsAtanf(float x);
float sceSasAsAsinf(float x);
float sceSasAsAcosf(float x);
float sceSasAsAtan2f(float y, float x);
float sceSasAsSinf(float x);
float sceSasAsCosf(float x);
float sceSasAsTanf(float x);
float sceSasAsSqrtf(float x);
float sceSasAsLogf(float x);
float sceSasAsLog10f(float x);
float sceSasAsExpf(float x);
float sceSasAsCeilf(float x);
float sceSasAsFloorf(float x);
float sceSasAsPowf(float x, float y);
float sceSasAsDbToLinear(float db, float minimum);

extern const float sceSasAsDbFractionTable[128];
extern const float sceSasAsDbTable[289];

#endif
