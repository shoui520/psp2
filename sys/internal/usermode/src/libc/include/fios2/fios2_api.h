/*
 * fios2_api.h: FIOS2's functions, under Sony's names and with Sony's prototypes (SDK Release
 * 03.570.011, target/include_common/fios2/fios2_api.h); the ones the tree uses so far.
 * SceFios2's internal exports the SDK does not declare follow, under the names its NID
 * database gives them.
 */
#ifndef _SCE_FIOS2_API_H
#define _SCE_FIOS2_API_H

#include <stdbool.h>
#include <fios2/fios2_types.h>

#ifdef __cplusplus
extern "C" {
#endif

bool sceFiosIsValidHandle(SceFiosHandle h);

int sceFiosDirectoryCreateSync(const SceFiosOpAttr *pAttr, const char *pPath);
int sceFiosDirectoryDeleteSync(const SceFiosOpAttr *pAttr, const char *pPath);
int sceFiosDeleteSync(const SceFiosOpAttr *pAttr, const char *pPath);
int sceFiosRenameSync(const SceFiosOpAttr *pAttr, const char *pOldPath, const char *pNewPath);
int sceFiosFileTruncateSync(const SceFiosOpAttr *pAttr, const char *pPath, SceFiosSize length);

int sceFiosFHOpenWithModeSync(const SceFiosOpAttr *pAttr, SceFiosFH *pOutFH, const char *pPath,
			      const SceFiosOpenParams *pOpenParams, int32_t nativeMode);
SceFiosSize sceFiosFHReadSync(const SceFiosOpAttr *pAttr, SceFiosFH fh, void *pBuf,
			      SceFiosSize length);
SceFiosSize sceFiosFHWriteSync(const SceFiosOpAttr *pAttr, SceFiosFH fh, const void *pBuf,
			       SceFiosSize length);
int sceFiosFHCloseSync(const SceFiosOpAttr *pAttr, SceFiosFH fh);
SceFiosOffset sceFiosFHSeek(SceFiosFH fh, SceFiosOffset offset, SceFiosWhence whence);

/* SceFios2's internal exports (the C library's file numbers) */
int sceFiosFHToFileno(SceFiosFH fh);
SceFiosFH sceFiosFilenoToFH(int fd);
void sceFiosDeallocatePassthruFH(SceFiosFH fh);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_FIOS2_API_H */
