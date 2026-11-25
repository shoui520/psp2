#ifndef VIDEO_SEARCH_EMPR_H
#define VIDEO_SEARCH_EMPR_H

#include <scetypes.h>
#include <stddef.h>
#include <stdint.h>

#define VSE_ERR_EVENT_FLAG         (-2146403839) /* 0x8010D001 */
#define VSE_ERR_INVALID_ARG        (-2146403838) /* 0x8010D002 */
#define VSE_ERR_NO_MEMORY          (-2146403837) /* 0x8010D003 */
#define VSE_ERR_STATE              (-2146403836) /* 0x8010D004 */
#define VSE_ERR_DB                 (-2146403835) /* 0x8010D005 */

#define VSE_MEMBLOCK_TYPE_USER_RW  0x0C20D060U
#define VSE_MSPACE_SIZE            0x7D000U
#define VSE_MMS_MOUNT_ID           402
#define VSE_COLUMN_COUNT           8
#define VSE_NAME_MAX               64
#define VSE_QUERY_MAX              1024
#define VSE_EVENT_DONE_BIT         2U

#define VSE_VALUE_TYPE_INT         0
#define VSE_VALUE_TYPE_STRING      2

typedef struct VseColumnRecord {
	char sqlName[VSE_NAME_MAX];
	char displayName[VSE_NAME_MAX];
	int32_t valueType;
	int32_t valueSize;
	void *valuePtr;
} VseColumnRecord;

typedef struct VseResultItem {
	char columnName[VSE_NAME_MAX];
	char reservedName[VSE_NAME_MAX];
	int32_t bufferSize;
	int32_t valueType;
	void *valueBuffer;
	char guardBytes[VSE_NAME_MAX];
} VseResultItem;

/* Mspace-backed libc helpers shared with SQLite 3.7.6.3 and os_psp2.c */
void *vseMalloc(size_t size);
void *vseRealloc(void *ptr, size_t size);
void vseFree(void *ptr);
void *vseMemcpy(void *dst, const void *src, size_t len);
void *vseMemset(void *dst, int val, size_t len);
int vseMemcmp(const void *lhs, const void *rhs, size_t len);
int vseStrcmp(const char *lhs, const char *rhs);
int vseStrncmp(const char *lhs, const char *rhs, size_t len);
size_t vseStrlen(const char *str);
int vseAtoi(const char *str);

/* Exports */
int module_start(SceSize arglen, const void *argp);
int module_stop(SceSize arglen, const void *argp);
int32_t SceVideoSearchEmpr_AAE5CD38(int32_t *outDetailErr);
int32_t SceVideoSearchEmpr_95441C59(int32_t *outDetailErr);
int32_t SceVideoSearchEmpr_87F6312C(int32_t *outDetailErr);
int32_t SceVideoSearchEmpr_7B403AC0(uint32_t colIndex, char *outName);
int32_t SceVideoSearchEmpr_AA171468(int32_t *outRowCount);
int32_t SceVideoSearchEmpr_76F93BD3(int32_t *outColCount);
int32_t SceVideoSearchEmpr_A6712C5F(int32_t rowIndex, int32_t colIndex, int32_t *outSize);
int32_t SceVideoSearchEmpr_BBEDB86A(int32_t rowIndex, int32_t colIndex, VseResultItem *outItem);

#endif /* VIDEO_SEARCH_EMPR_H */
