#include "video_search_empr.h"
#include <kernel/threadmgr.h>
#include <kernel/sysmem.h>
#include <appmgr.h>
#include <libkernel/libkernel.h>
#include "sqlite3.h"

#define SQLITE_OPEN_PSP2_POWER_LOCK 0x10000000

typedef void *SceClibMspace;

extern int sceAppMgrMmsMount(int id, char *mountPoint);
extern SceClibMspace sceClibMspaceCreate(void *base, SceSize capacity);
extern void *sceClibMspaceMalloc(SceClibMspace mspace, SceSize size);
extern void *sceClibMspaceRealloc(SceClibMspace mspace, void *ptr, SceSize size);
extern void sceClibMspaceFree(SceClibMspace mspace, void *ptr);
extern int sceClibSnprintf(char *dst, SceSize maxLen, const char *fmt, ...);
extern void *sceClibMemset(void *dst, int val, SceSize len);
extern SceSize sceClibStrnlen(const char *str, SceSize maxLen);
extern char *sceClibStrncpy(char *dst, const char *src, SceSize maxLen);

static sqlite3 *g_vseDb;
static SceClibMspace g_vseMspace;
static int32_t g_vseInitialized;
static SceUID g_vseEventFlag;
static int32_t g_vseRowCount;
static int32_t g_vseColCount;
static VseColumnRecord **g_vseRows;
static int32_t g_vseCallbackError;
static int32_t g_vseRowCursor;
static char g_vseMountPoint[16];
static char g_vseDbPath[128];
static char g_vseTempDir[128];

static const VseColumnRecord g_vseColumns[VSE_COLUMN_COUNT] = {
	{ "title",         "title",         VSE_VALUE_TYPE_STRING, 0, NULL },
	{ "content_path",  "content_path",  VSE_VALUE_TYPE_STRING, 0, NULL },
	{ "released_date", "released_date", VSE_VALUE_TYPE_STRING, 0, NULL },
	{ "duration",      "duration",      VSE_VALUE_TYPE_INT,    0, NULL },
	{ "icon_size",     "icon_size",     VSE_VALUE_TYPE_INT,    0, NULL },
	{ "icon_offset",   "icon_offset",   VSE_VALUE_TYPE_INT,    0, NULL },
	{ "size",          "size",          VSE_VALUE_TYPE_INT,    0, NULL },
	{ "description",   "description",   VSE_VALUE_TYPE_STRING, 0, NULL },
};

void *vseMalloc(size_t size)
{
	return sceClibMspaceMalloc(g_vseMspace, (SceSize)size);
}

void *vseRealloc(void *ptr, size_t size)
{
	return sceClibMspaceRealloc(g_vseMspace, ptr, (SceSize)size);
}

void vseFree(void *ptr)
{
	sceClibMspaceFree(g_vseMspace, ptr);
}

void *vseMemcpy(void *dst, const void *src, size_t len)
{
	uint8_t *out = (uint8_t *)dst;
	const uint8_t *in = (const uint8_t *)src;
	for (size_t i = 0; i < len; i++)
		out[i] = in[i];
	return dst;
}

void *vseMemset(void *dst, int val, size_t len)
{
	uint8_t *out = (uint8_t *)dst;
	for (size_t i = 0; i < len; i++)
		out[i] = (uint8_t)val;
	return dst;
}

void *memset(void *dst, int val, size_t len)
{
	return sceClibMemset(dst, val, len);
}

int vseMemcmp(const void *lhs, const void *rhs, size_t len)
{
	const uint8_t *left = (const uint8_t *)lhs;
	const uint8_t *right = (const uint8_t *)rhs;
	for (size_t i = 0; i < len; i++) {
		if (left[i] != right[i])
			return (int)left[i] - (int)right[i];
	}
	return 0;
}

int vseStrcmp(const char *lhs, const char *rhs)
{
	while (*lhs != '\0' && *lhs == *rhs) {
		lhs++;
		rhs++;
	}
	return (int)(uint8_t)*lhs - (int)(uint8_t)*rhs;
}

int vseStrncmp(const char *lhs, const char *rhs, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		if (lhs[i] != rhs[i] || lhs[i] == '\0')
			return (int)(uint8_t)lhs[i] - (int)(uint8_t)rhs[i];
	}
	return 0;
}

size_t vseStrlen(const char *str)
{
	size_t len = 0;
	while (str[len] != '\0')
		len++;
	return len;
}

int vseAtoi(const char *str)
{
	if (str == NULL)
		return 0;
	while (*str == ' ' || *str == '\t')
		str++;
	int sign = 1;
	if (*str == '-') {
		sign = -1;
		str++;
	} else if (*str == '+') {
		str++;
	}
	int acc = 0;
	while (*str >= '0' && *str <= '9') {
		acc = acc * 10 + (*str - '0');
		str++;
	}
	return sign * acc;
}

static int32_t vseFreeRows(void)
{
	if (g_vseRows != NULL) {
		for (int32_t row = 0; row < g_vseRowCount; row++) {
			if (g_vseRows[row] != NULL) {
				for (int32_t col = 0; col < g_vseColCount; col++) {
					if (g_vseRows[row][col].valuePtr != NULL) {
						vseFree(g_vseRows[row][col].valuePtr);
						g_vseRows[row][col].valuePtr = NULL;
					}
				}
				vseFree(g_vseRows[row]);
				g_vseRows[row] = NULL;
			}
		}
		vseFree(g_vseRows);
		g_vseRows = NULL;
	}
	return 0;
}

static int vseCountCallback(void *userArg, int colCount, char **values, char **colNames)
{
	(void)userArg;
	(void)colCount;
	(void)colNames;
	g_vseRowCount = vseAtoi(values[0]);
	sceKernelSetEventFlag(g_vseEventFlag, VSE_EVENT_DONE_BIT);
	return 0;
}

static int vseRowCallback(void *userArg, int colCount, char **values, char **colNames)
{
	(void)userArg;
	(void)colCount;
	(void)colNames;

	if (g_vseCallbackError == 0) {
		int32_t rowIdx = g_vseRowCursor;
		VseColumnRecord *row = (VseColumnRecord *)vseMalloc(
			(size_t)g_vseColCount * sizeof(VseColumnRecord));
		g_vseRows[rowIdx] = row;
		if (row == NULL) {
			g_vseCallbackError = VSE_ERR_NO_MEMORY;
		} else {
			for (int32_t col = 0; col < g_vseColCount; col++) {
				vseMemset(row[col].sqlName, 0, VSE_NAME_MAX);
				sceClibStrncpy(row[col].sqlName, g_vseColumns[col].sqlName, VSE_NAME_MAX);
				vseMemset(row[col].displayName, 0, VSE_NAME_MAX);
				sceClibStrncpy(row[col].displayName, g_vseColumns[col].displayName, VSE_NAME_MAX);
				row[col].valueType = g_vseColumns[col].valueType;
				row[col].valueSize = g_vseColumns[col].valueSize;
				row[col].valuePtr = g_vseColumns[col].valuePtr;
			}

			for (int32_t col = 0; col < g_vseColCount && g_vseCallbackError == 0; col++) {
				if (values[col] == NULL)
					continue;
				if (row[col].valueType == VSE_VALUE_TYPE_STRING) {
					SceSize strSize = sceClibStrnlen(values[col], VSE_QUERY_MAX) + 1;
					row[col].valueSize = (int32_t)strSize;
					row[col].valuePtr = vseMalloc(strSize);
					if (row[col].valuePtr != NULL) {
						vseMemset(row[col].valuePtr, 0, strSize);
						sceClibStrncpy((char *)row[col].valuePtr, values[col], strSize);
					} else {
						g_vseCallbackError = VSE_ERR_NO_MEMORY;
					}
				} else if (row[col].valueType == VSE_VALUE_TYPE_INT) {
					row[col].valueSize = 4;
					row[col].valuePtr = vseMalloc(4);
					if (row[col].valuePtr != NULL) {
						int32_t parsed = vseAtoi(values[col]);
						vseMemcpy(row[col].valuePtr, &parsed, sizeof(parsed));
					} else {
						g_vseCallbackError = VSE_ERR_NO_MEMORY;
					}
				}
			}
		}
	}

	g_vseRowCursor++;
	if (g_vseRowCursor == g_vseRowCount)
		sceKernelSetEventFlag(g_vseEventFlag, VSE_EVENT_DONE_BIT);
	return 0;
}

int module_start(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	return 0;
}

int module_stop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	sqlite3_shutdown();
	return 0;
}

int32_t SceVideoSearchEmpr_AAE5CD38(int32_t *outDetailErr)
{
	*outDetailErr = 0;
	if (g_vseInitialized != 0)
		return VSE_ERR_STATE;

	g_vseInitialized = 0;
	g_vseRowCount = 0;
	g_vseColCount = VSE_COLUMN_COUNT;

	void *base = NULL;
	SceUID blockId = sceKernelAllocMemBlock("SceVideoSearchEmpr",
		VSE_MEMBLOCK_TYPE_USER_RW, VSE_MSPACE_SIZE, NULL);
	if (blockId < 0)
		return 0;

	sceKernelGetMemBlockBase(blockId, &base);
	g_vseMspace = sceClibMspaceCreate(base, VSE_MSPACE_SIZE);

	int32_t mountErr = sceAppMgrMmsMount(VSE_MMS_MOUNT_ID, g_vseMountPoint);
	if (mountErr != 0) {
		*outDetailErr = mountErr;
		return VSE_ERR_DB;
	}

	sceClibSnprintf(g_vseDbPath, sizeof(g_vseDbPath), "%s%s", g_vseMountPoint, "/AVContent.db");
	sceClibSnprintf(g_vseTempDir, sizeof(g_vseTempDir), "%s%s", g_vseMountPoint, "/");
	sqlite3_temp_directory = g_vseTempDir;

	int32_t cfgErr = sqlite3_config(SQLITE_CONFIG_MEMSTATUS, 0);
	if (cfgErr != SQLITE_OK) {
		*outDetailErr = cfgErr;
		return VSE_ERR_DB;
	}

	int32_t openErr = sqlite3_open_v2(g_vseDbPath, &g_vseDb,
		SQLITE_OPEN_READONLY | SQLITE_OPEN_PSP2_POWER_LOCK, NULL);
	if (openErr != SQLITE_OK) {
		*outDetailErr = openErr;
		return VSE_ERR_DB;
	}

	g_vseEventFlag = sceKernelCreateEventFlag("sceVideoSerchEmpr", 0, 0, NULL);
	if (g_vseEventFlag >= 0)
		g_vseInitialized = 1;
	else
		g_vseEventFlag = VSE_ERR_EVENT_FLAG;

	return 0;
}

int32_t SceVideoSearchEmpr_95441C59(int32_t *outDetailErr)
{
	*outDetailErr = 0;
	if (g_vseInitialized == 0)
		return VSE_ERR_STATE;

	sqlite3_close(g_vseDb);
	sceKernelDeleteEventFlag(g_vseEventFlag);
	vseFreeRows();
	g_vseInitialized = 0;
	return 0;
}

int32_t SceVideoSearchEmpr_87F6312C(int32_t *outDetailErr)
{
	*outDetailErr = 0;
	if (g_vseInitialized == 0)
		return VSE_ERR_STATE;

	vseFreeRows();

	char *errMsg = NULL;
	unsigned int outBits = 0;
	int32_t countErr = sqlite3_exec(g_vseDb,
		" SELECT COUNT(mrid) FROM  tbl_vpcontent WHERE  content_path_ext = \"MGV\";",
		vseCountCallback, NULL, &errMsg);
	if (countErr != SQLITE_OK) {
		sqlite3_free(errMsg);
		*outDetailErr = countErr;
		return VSE_ERR_DB;
	}
	sceKernelWaitEventFlag(g_vseEventFlag, VSE_EVENT_DONE_BIT, 3U, &outBits, NULL);

	if (g_vseRowCount == 0)
		return 0;

	g_vseRows = (VseColumnRecord **)vseMalloc(
		(size_t)g_vseRowCount * sizeof(VseColumnRecord *));
	if (g_vseRows == NULL)
		return VSE_ERR_NO_MEMORY;

	char query[VSE_QUERY_MAX];
	sceClibMemset(query, 0, sizeof(query));
	SceSize pos = sceClibStrnlen(" SELECT ", sizeof(query));
	sceClibSnprintf(query, sizeof(query), " SELECT ");

	for (uint32_t col = 0; col < VSE_COLUMN_COUNT; col++) {
		SceSize nameLen = sceClibStrnlen(g_vseColumns[col].sqlName, sizeof(query));
		if (col == VSE_COLUMN_COUNT - 1) {
			sceClibSnprintf(&query[pos], sizeof(query), " %s ", g_vseColumns[col].sqlName);
			pos += nameLen + 1;
		} else {
			sceClibSnprintf(&query[pos], sizeof(query), " %s, ", g_vseColumns[col].sqlName);
			pos += nameLen + 2;
		}
	}
	sceClibSnprintf(&query[pos], sizeof(query), " %s ",
		" FROM tbl_vpcontent WHERE content_path_ext = \"MGV\" ;");

	g_vseCallbackError = 0;
	g_vseRowCursor = 0;
	errMsg = NULL;
	unsigned int rowBits = 0;
	int32_t queryErr = sqlite3_exec(g_vseDb, query, vseRowCallback, NULL, &errMsg);
	if (queryErr != SQLITE_OK) {
		sqlite3_free(errMsg);
		*outDetailErr = queryErr;
		return VSE_ERR_DB;
	}
	sceKernelWaitEventFlag(g_vseEventFlag, VSE_EVENT_DONE_BIT, 3U, &rowBits, NULL);

	if (g_vseCallbackError != 0)
		return g_vseCallbackError;
	return 0;
}

int32_t SceVideoSearchEmpr_7B403AC0(uint32_t colIndex, char *outName)
{
	if (colIndex >= (uint32_t)g_vseColCount)
		return VSE_ERR_INVALID_ARG;
	if (outName == NULL)
		return VSE_ERR_INVALID_ARG;
	vseMemset(outName, 0, VSE_NAME_MAX);
	sceClibStrncpy(outName, g_vseColumns[colIndex].displayName, VSE_NAME_MAX);
	return 0;
}

int32_t SceVideoSearchEmpr_AA171468(int32_t *outRowCount)
{
	if (g_vseInitialized == 0)
		return VSE_ERR_STATE;
	if (outRowCount == NULL)
		return VSE_ERR_INVALID_ARG;
	*outRowCount = g_vseRowCount;
	return 0;
}

int32_t SceVideoSearchEmpr_76F93BD3(int32_t *outColCount)
{
	if (g_vseInitialized == 0)
		return VSE_ERR_STATE;
	if (outColCount == NULL)
		return VSE_ERR_INVALID_ARG;
	*outColCount = g_vseColCount;
	return 0;
}

int32_t SceVideoSearchEmpr_A6712C5F(int32_t rowIndex, int32_t colIndex, int32_t *outSize)
{
	if (g_vseInitialized == 0)
		return VSE_ERR_STATE;
	if (outSize == NULL)
		return VSE_ERR_INVALID_ARG;
	if (rowIndex >= g_vseRowCount || colIndex >= g_vseColCount)
		return VSE_ERR_INVALID_ARG;
	*outSize = g_vseRows[rowIndex][colIndex].valueSize;
	return 0;
}

int32_t SceVideoSearchEmpr_BBEDB86A(int32_t rowIndex, int32_t colIndex, VseResultItem *outItem)
{
	if (g_vseInitialized == 0)
		return VSE_ERR_STATE;
	if (outItem == NULL || rowIndex >= g_vseRowCount || colIndex >= g_vseColCount)
		return VSE_ERR_INVALID_ARG;

	for (uint32_t i = 0; i < VSE_NAME_MAX; i++) {
		if (outItem->guardBytes[i] != 0)
			return VSE_ERR_INVALID_ARG;
	}
	for (uint32_t i = 0; i < VSE_NAME_MAX; i++) {
		if (outItem->reservedName[i] != 0)
			return VSE_ERR_INVALID_ARG;
	}

	const VseColumnRecord *cell = &g_vseRows[rowIndex][colIndex];
	vseMemset(outItem->columnName, 0, VSE_NAME_MAX);
	sceClibStrncpy(outItem->columnName, cell->displayName, VSE_NAME_MAX);
	outItem->valueType = cell->valueType;
	if (outItem->bufferSize != cell->valueSize)
		return VSE_ERR_INVALID_ARG;
	vseMemcpy(outItem->valueBuffer, cell->valuePtr, (size_t)cell->valueSize);
	return 0;
}
