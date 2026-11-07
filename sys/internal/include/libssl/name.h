/* Filename is ours. Sony's export link name is retained. */
#ifndef LIBSSL_NAME_H
#define LIBSSL_NAME_H

#include <scetypes.h>

int SceSslInternal_CE37B69E(void *name);
int SceSslInternal_81FA5DBE(void *certificate, int mode, void **result);
int SceSslInternal_08179EBD(void *certificate, int mode, void **result);
int SceSslInternal_17BF2367(void *component);
int SceSslInternal_B5A099D2(void *component, int selector, void *result);
int SceSslInternal_DABB61EE(void *name, SceUInt32 capacity, char *data);
int SceSslInternal_91C2531C(void *name, int selector, void *result);
int SceSslInternal_568F0E80(void *left, void *right);

#endif
