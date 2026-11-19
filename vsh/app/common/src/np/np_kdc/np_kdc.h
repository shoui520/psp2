/*
 * SceNpKdc's interface: the license key delivery client the game plugin loads
 * (SCE_SYSMODULE_INTERNAL_NP_KDC) to fetch a license again from the store. No SDK header and no
 * record name its functions; the prototypes are taken from the shell's calls (license_server.cpp)
 * and the names are the NID's until the library is written.
 */
#ifndef _NP_KDC_H
#define _NP_KDC_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the library started on the work memory given to it */
int SceNpKdc_26F8A1B9(SceSize workSize, void *work);

/* the library stopped */
int SceNpKdc_06CB1007(void);

/*
 * contentId's license fetched with the store session's EPW into buf, its size in *licenseSize
 * (PLACEHOLDER: the two last words it gives back are read by no caller)
 */
int SceNpKdc_78E56B9B(const char *contentId, const char *epw, void *buf, SceSize bufSize,
		      SceSize *licenseSize, int *result1, int *result2);

#ifdef __cplusplus
}
#endif

#endif /* _NP_KDC_H */
