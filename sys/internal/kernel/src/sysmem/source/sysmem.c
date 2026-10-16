/* KernelUtils key utility export names. */

/* Compile the shared key utilities under this module's original export names. */
#define DesAdjustKeyParity SceKernelUtilsForDriver_919C12E2
#define DesCheckKeyParity SceKernelUtilsForDriver_227B2E53
#define DesIsWeakKey SceKernelUtilsForDriver_AFD56168
#include "../../../../../../common/crypto/des_parity.c"

#define AesSubWord SceKernelUtilsForDriver_60ED6EA9
#define AesInverseMixColumn SceKernelUtilsForDriver_C76A7685
#include "../../../../../../common/crypto/aes_helpers.c"

