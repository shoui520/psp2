#include "hmac_internal.h"

HMAC_ALGORITHM(hmacSha256Algorithm, 256, SceKernelUtilsForDriver_A773A6A8, SceKernelUtilsForDriver_D909FA2C,
	       SceKernelUtilsForDriver_236A9097, SceKernelUtilsForDriver_4899CD4B);
HMAC_ALGORITHM(hmacSha224Algorithm, 224, SceKernelUtilsForDriver_9EA9D4DC, SceKernelUtilsForDriver_C762EA6D,
	       SceKernelUtilsForDriver_7556E611, SceKernelUtilsForDriver_FBF2A442);
HMAC_ALGORITHM(hmacSha1Algorithm, 1, SceKernelUtilsForDriver_87DC7F2F, SceKernelUtilsForDriver_E4390FFA,
	       SceKernelUtilsForDriver_478A6F3C, SceKernelUtilsForDriver_48F24106);
