/*
 * libpaf's IME client (ime/ime.h): the text filter of the IME's input types, and the client of
 * the shell's IME service. sceImeVshOpen connects to the service and keeps the client's state in
 * the caller's work (ImeVshContext); sceImeVshUpdate sends what the caller changed, polls the
 * service's events and passes them to the caller's handler; the configuration and the user
 * dictionary connect for each call. One VSH IME can be open at a time. The file, its local names
 * and the context's fields are ours.
 */
#include <stddef.h>
#include <stdint.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
extern "C" {
#include <ctrl/ctrl.h>
#include <hid/hid.h>
}
#include <ces/libces_ucs.h>
#include <ime/ime.h>
#include <system/system.h>
#include "../../../lowlayer/src/ipmi/client.h"
#include "../../../lowlayer/src/ipmi/ipmi_error.h"

/*
 * The SDK's libSceCes (libces_jis.h, libces_gb.h) as far as the filter uses it, until the
 * tree's ces headers declare it; the profile sheet is libces_ucs.h's.
 */
extern "C" {
typedef union SceCesSJisUcsProfile SceCesSJisUcsProfile;
typedef union SceCesGbUcsProfile SceCesGbUcsProfile;

SceCesSJisUcsProfile *sceCesUcsProfileInitSJis1997Cp932(SceCesUcsProfileSheet *sheet);
int sceCesUtf16ToSJis(const uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Len,
		      const SceCesSJisUcsProfile *p, uint8_t *sjisBuf, uint32_t sjisMax,
		      uint32_t *sjisSize);
SceCesGbUcsProfile *sceCesUcsProfileInitGb18030_2000(SceCesUcsProfileSheet *sheet);
int sceCesGbUcsProfileSetUdaMapping(SceCesGbUcsProfile *p, int udaMapping);
int sceCesGbUcsProfileSet4ByteCharRange(SceCesGbUcsProfile *p, int gb4byteRange);
int sceCesUtf16ToGb(const uint16_t *utf16buf, uint32_t utf16max, uint32_t *utf16Len,
		    const SceCesGbUcsProfile *p, uint8_t *gbBuf, uint32_t gbMax, uint32_t *gbSize);
}

/* the IME service's methods */
enum {
	IME_SERVICE_METHOD_CLOSE = 2,
	IME_SERVICE_METHOD_SET_PARAM = 3,
	IME_SERVICE_METHOD_GET_EVENTS = 4,
	IME_SERVICE_METHOD_UPDATE = 5,
	IME_SERVICE_METHOD_CONFIG = 20,
	IME_SERVICE_METHOD_DIC_GET_WORDS = 33,
	IME_SERVICE_METHOD_DIC_ADD_WORD = 34,
	IME_SERVICE_METHOD_DIC_DELETE_WORD = 35,
	IME_SERVICE_METHOD_DIC_REPLACE_WORD = 36,
	IME_SERVICE_METHOD_DIC_37 = 37,
	IME_SERVICE_METHOD_DIC_38 = 38
};

/* what a connection is for: connect's parameter */
enum {
	IME_SERVICE_CLIENT_VSH = 1,
	IME_SERVICE_CLIENT_CONFIG = 2,
	IME_SERVICE_CLIENT_DIC = 3
};

/* the SDK version a parameter of 0x30 bytes is taken from, and the one a 0 sdkVersion means */
#define IME_VSH_SDK_VERSION_OLD		0x01030000
/* the SDK libpaf was built with (sdk_version.h) */
#define IME_VSH_SDK_VERSION_DEFAULT	SCE_PSP2_SDK_VERSION

#define IME_VSH_PARAM_SIZE_OLD		0x30
/* the languages SCE_IME_LANGUAGE_* and the system IME's two besides them */
#define IME_VSH_SUPPORTED_LANGUAGES	0x300000FFFFFULL
#define IME_VSH_MAX_EVENTS		10
#define IME_VSH_PREEDIT_LENGTH		31
/* the longest text a filter may give for an inserted text */
#define IME_VSH_MAX_FILTERED_LENGTH	90

/* the parameter as the IME service takes it (0x28 bytes) */
struct ImeVshServiceParam {
	uint64_t supportedLanguages;
	uint32_t languagesForced;
	uint32_t type;
	uint32_t option;
	uint32_t serial;
	uint8_t enterLabel;
	uint8_t focusShown;
	uint32_t sdkVersion;
	/* 1: the caller gave no sdkVersion */
	uint32_t flags;
};

/* ImeVshRequest's flags: what changed */
#define IME_VSH_REQUEST_STATE		0x1
#define IME_VSH_REQUEST_CONTEXT		0x2
#define IME_VSH_REQUEST_CARET		0x4
#define IME_VSH_REQUEST_GEOMETRY	0x8

/* what the caller changed since the last update (0x28 bytes) */
struct ImeVshRequest {
	uint32_t flags;
	/* 1: make the preedit text part of the string, 2: the caret left the preedit text */
	uint32_t state;
	/* whether the text before the caret ends a sentence */
	uint32_t sentenceEnd;
	uint32_t caretIndex;
	SceImeVshCaretGeometry geometry;
	uint32_t reserved[3];
};

/* an event as the IME service sends it (0xBC bytes) */
struct ImeVshServiceEvent {
	uint32_t id;
	union {
		SceImeVshRect rect;
		bool showFocus;
		wchar_t text[92];
		struct {
			wchar_t str[32];
			uint32_t caretIndex;
		} preedit;
	} param;
};

/* the client's state, in the caller's work (0x838 bytes) */
struct ImeVshContext {
	/* the service's SCE_IME_EVENT_OPEN came */
	bool opened;
	/* the events before the service's SCE_IME_EVENT_OPEN are dropped */
	bool waitOpen;
	/* the last event was a SCE_IME_VSH_EVENT_INSERT */
	bool inserted;
	/* the text inserted last ended with a space */
	bool insertedSpace;
	ImeVshRequest request;
	bool requestPending;
	void *arg;
	SceImeVshEventHandler handler;
	int32_t (*filter)(wchar_t *out, uint32_t *out_len, const wchar_t *src, uint32_t src_len);
	ImeVshContext *self;
	int32_t eventCount;
	ImeVshServiceEvent events[IME_VSH_MAX_EVENTS];
	SceImeVshParam param;
	uint32_t caretIndex;
	IPMI::Client *client;
	int requestId;
	bool getEventsPending;
	wchar_t preedit[IME_VSH_PREEDIT_LENGTH];
	uint32_t serial;
	bool intercepted;
	uint8_t reserved[7];
};

/* the dictionary's word methods' input */
struct ImeDicWordParam {
	uint64_t language;
	SceImeDicWord word;
};

struct ImeDicReplaceWordParam {
	uint64_t language;
	SceImeDicWord oldWord;
	SceImeDicWord newWord;
};

/* the configuration method's input */
struct ImeConfigParam {
	/* 0: set, 1: get */
	uint32_t operation;
	uint64_t language;
	int32_t id;
	int32_t value;
};

static bool s_imeInitialized;
static bool s_imeVshOpened;
static bool s_imeVshInitialized;
static SceKernelLwMutexWork s_imeMutex;
static uint8_t s_imeState[0x44];
static SceKernelLwMutexWork s_imeVshMutex;
/* the IPMI client's memory: one connection at a time */
static uint8_t s_clientMemory[0x200];

static void imeVshInit(void) __attribute__((noinline));
static void imeVshTerm(void) __attribute__((noinline));

void sceImeInit(void)
{
	if (s_imeInitialized)
		return;

	imeVshInit();
	sceClibMemset(s_imeState, 0, sizeof(s_imeState));
	sceKernelCreateLwMutex(&s_imeMutex, "SceImeMutex", SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO |
			       SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0, NULL);
	s_imeInitialized = true;
}

void sceImeTerm(void)
{
	if (!s_imeInitialized)
		return;

	sceKernelDeleteLwMutex(&s_imeMutex);
	imeVshTerm();
	s_imeInitialized = false;
}

static __attribute__((noinline)) uint32_t imeWcsnlen(const wchar_t *s, uint32_t max)
{
	uint32_t n;

	if (s == NULL)
		return 0;

	for (n = 0; n < max; n++) {
		if (s[n] == 0)
			return n;
	}

	return max;
}

static __attribute__((noinline)) const wchar_t *imeWcschr(const wchar_t *s, wchar_t c)
{
	if (s == NULL)
		return NULL;

	for (; *s != 0; s++) {
		if (*s == c)
			return s;
	}

	return NULL;
}

int32_t sceImeFilterText(wchar_t *out, uint32_t *outLength, uint32_t outMax,
			 const SceImeFilterParam *param, const wchar_t *in, uint32_t inLength)
{
	uint32_t remain, type, count, read;
	uint64_t languages;
	wchar_t first;

	if (out == NULL || outLength == NULL || param == NULL || in == NULL)
		return SCE_IME_ERROR_INVALID_POINTER;

	if (outMax <= 1)
		return SCE_IME_ERROR_INVALID_PARAM;

	type = param->type;
	remain = outMax - 1;
	if (type > SCE_IME_TYPE_MAIL &&
	    (type < SCE_IME_VSH_TYPE_NUMBER || type > SCE_IME_VSH_TYPE_DIGITS))
		return SCE_IME_ERROR_INVALID_PARAM;

	languages = param->supportedLanguages;
	if (languages == 0)
		languages = 0xFFFFF;

	/* the first character the languages need checked: the latin languages have Latin-1 and
	   Latin Extended-A, Russian and the Asian languages ASCII */
	if ((languages & 0xE1BFF) != 0)
		first = 0x17F;
	else if ((languages & 0x1E400) != 0)
		first = 0x7F;
	else
		return SCE_IME_ERROR_INVALID_PARAM;

	count = 0;
	read = 0;
	switch (type) {
	case SCE_IME_TYPE_BASIC_LATIN:
	case SCE_IME_TYPE_MAIL:
	case SCE_IME_VSH_TYPE_BASIC_LATIN:
		while (remain != 0 && read != inLength && *in != 0) {
			wchar_t c = *in++;

			read++;
			if (c >= 0x7F)
				continue;

			remain--;
			*out++ = c;
			count++;
		}
		break;

	case SCE_IME_VSH_TYPE_PHONE_NUMBER:
		while (remain != 0 && read != inLength && *in != 0) {
			wchar_t c = *in++;
			wchar_t symbols[] = { ',', ';', '*', '#', '+', '-', '\n', 0 };

			read++;
			if ((c < '0' || c > '9') && imeWcschr(symbols, c) == NULL)
				continue;

			remain--;
			*out++ = c;
			count++;
		}
		break;

	case SCE_IME_TYPE_NUMBER:
		while (remain != 0 && read != inLength && *in != 0) {
			wchar_t c = *in++;

			read++;
			if ((c < '0' || c > '9') && c != '\r' && c != '\n')
				continue;

			remain--;
			*out++ = c;
			count++;
		}
		break;

	case SCE_IME_TYPE_EXTENDED_NUMBER:
		while (remain != 0 && read != inLength && *in != 0) {
			wchar_t c = *in;
			wchar_t symbols[] = { ',', '.', '-', '\n', 0 };

			in++;
			read++;
			if ((c < '0' || c > '9') && imeWcschr(symbols, c) == NULL)
				continue;

			remain--;
			*out++ = c;
			count++;
		}
		break;

	case SCE_IME_VSH_TYPE_NUMBER:
	case SCE_IME_VSH_TYPE_DIGITS:
		while (remain != 0 && read != inLength && *in != 0) {
			wchar_t c = *in++;

			read++;
			if (c < '0' || c > '9')
				continue;

			remain--;
			*out++ = c;
			count++;
		}
		break;

	default:
		while (remain != 0 && read != inLength && *in != 0) {
			wchar_t c = *in;

			read++;
			in++;
			if (c >= first) {
				wchar_t symbols[] = {
					0x00A1, 0x00BF, 0x00B4, 0x2018, 0x2019, 0x201A, 0x201C,
					0x201D, 0x201E, 0x00B0, 0x2039, 0x203A, 0x00AB, 0x00BB,
					0x00AA, 0x00BA, 0x00D7, 0x00F7, 0x00A4, 0x00A2, 0x20AC,
					0x00A3, 0x00A5, 0x20A9, 0x00A7, 0x00A6, 0x00B5, 0x00AC,
					0x00B7, 0x00B9, 0x00B2, 0x00B3, 0x00BC, 0x00BD, 0x00BE,
					0x2116, 0
				};

				if (imeWcschr(symbols, c) == NULL &&
				    ((languages & SCE_IME_LANGUAGE_KOREAN) == 0 ||
				     ((c < 0x3130 || c > 0x318F) && (c < 0xAC00 || c > 0xD7AF))) &&
				    ((languages & SCE_IME_LANGUAGE_RUSSIAN) == 0 ||
				     c < 0x400 || c > 0x4FF)) {
					wchar_t japanese[] = {
						0x3001, 0x3002, 0x30FC, 0x30FB, 0x300C, 0x300D, 0xFFE5,
						0xFF01, 0x201D, 0xFF03, 0xFF04, 0xFF05, 0xFF06, 0x2019,
						0xFF08, 0xFF09, 0xFF0A, 0xFF0B, 0xFF0C, 0xFF0D, 0xFF0E,
						0xFF0F, 0xFF1A, 0xFF1B, 0xFF1C, 0xFF1D, 0xFF1E, 0xFF1F,
						0xFF20, 0xFF3B, 0xFF3C, 0xFF3D, 0xFF3E, 0xFF3F, 0xFF40,
						0xFF5B, 0xFF5C, 0xFF5D, 0xFF5E, 0
					};
					wchar_t chinese[] = {
						0x3001, 0x3002, 0x300C, 0x300D, 0xFF01, 0x201D, 0xFF03,
						0xFF04, 0xFF05, 0xFF06, 0x2019, 0xFF08, 0xFF09, 0xFF0A,
						0xFF0B, 0xFF0C, 0xFF0D, 0xFF0E, 0xFF0F, 0xFF1A, 0xFF1B,
						0xFF1C, 0xFF1D, 0xFF1E, 0xFF1F, 0xFF20, 0xFF3B, 0xFF3C,
						0xFF3D, 0xFF3E, 0xFF3F, 0xFF40, 0xFF5B, 0xFF5C, 0xFF5D,
						0xFF5E, 0
					};

					if ((languages & (SCE_IME_LANGUAGE_JAPANESE |
							  SCE_IME_LANGUAGE_KOREAN)) == 0 ||
					    ((c < 0xFF10 || c > 0xFF19) &&
					     imeWcschr(japanese, c) == NULL)) {
						if ((languages & (SCE_IME_LANGUAGE_SIMPLIFIED_CHINESE |
								  SCE_IME_LANGUAGE_TRADITIONAL_CHINESE)) == 0 ||
						    ((c < 0xFF10 || c > 0xFF19) &&
						     imeWcschr(chinese, c) == NULL)) {
							SceCesUcsProfileSheet sjisSheet;
							SceCesUcsProfileSheet gbSheet;
							uint8_t sjis[4], gb[4];
							uint32_t used, size;
							uint16_t utf16[2];

							if ((languages & SCE_IME_LANGUAGE_JAPANESE) == 0 ||
							    (utf16[0] = c, utf16[1] = 0,
							     sjis[0] = 0, sjis[1] = 0, sjis[2] = 0, sjis[3] = 0,
							     sceCesUtf16ToSJis(utf16, 2, &used,
									       sceCesUcsProfileInitSJis1997Cp932(&sjisSheet),
									       sjis, 4, &size) != 0)) {
								SceCesGbUcsProfile *gbProfile;

								if ((languages & (SCE_IME_LANGUAGE_SIMPLIFIED_CHINESE |
										  SCE_IME_LANGUAGE_TRADITIONAL_CHINESE)) == 0)
									continue;

								gbProfile = sceCesUcsProfileInitGb18030_2000(&gbSheet);
								sceCesGbUcsProfileSetUdaMapping(gbProfile, 0);
								sceCesGbUcsProfileSet4ByteCharRange(gbProfile, 0);
								utf16[0] = c;
								utf16[1] = 0;
								gb[0] = 0;
								gb[1] = 0;
								gb[2] = 0;
								gb[3] = 0;
								if (sceCesUtf16ToGb(utf16, 2, &used, gbProfile, gb,
										    4, &size) != 0)
									continue;
							}
						}
					}
				}
			}

			remain--;
			*out++ = c;
			count++;
		}
		break;
	}

	out -= count;
	out[count] = 0;
	*outLength = count;

	return SCE_OK;
}

static void imeVshInit(void)
{
	if (s_imeVshInitialized)
		return;

	s_imeVshOpened = false;
	sceKernelCreateLwMutex(&s_imeVshMutex, "SceImeVshMutex", SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO |
			       SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0, NULL);
	s_imeVshInitialized = true;
}

static void imeVshTerm(void)
{
	if (!s_imeVshInitialized)
		return;

	s_imeVshInitialized = false;
	sceKernelDeleteLwMutex(&s_imeVshMutex);
}

/* checks the caller's parameter and makes the service's from it */
static int32_t imeVshCheckParam(ImeVshServiceParam *out, const SceImeVshParam *param)
{
	uint64_t languages;
	uint32_t sdkVersion, i;

	if (param->size != sizeof(SceImeVshParam))
		return SCE_IME_ERROR_INVALID_SIZE;

	if (param->work == NULL)
		return SCE_IME_ERROR_INVALID_WORK;

	languages = param->supportedLanguages;
	if (((languages ^ IME_VSH_SUPPORTED_LANGUAGES) & languages) != 0)
		return SCE_IME_ERROR_INVALID_SUPPORTED_LANGUAGES;

	if (param->languagesForced != SCE_TRUE && param->languagesForced != SCE_FALSE)
		return SCE_IME_ERROR_INVALID_PARAM;

	if (param->type > SCE_IME_TYPE_MAIL &&
	    (param->type < SCE_IME_VSH_TYPE_NUMBER || param->type > SCE_IME_VSH_TYPE_DIGITS))
		return SCE_IME_ERROR_INVALID_TYPE;

	if (param->handler == NULL)
		return SCE_IME_ERROR_INVALID_HANDLER;

	if (((uintptr_t)param->work & 3) != 0)
		return SCE_IME_ERROR_INVALID_WORK;

	if (param->enterLabel > (uint32_t)SCE_IME_ENTER_LABEL_GO)
		return SCE_IME_ERROR_INVALID_ENTER_LABEL;

	out->languagesForced = param->languagesForced;
	out->type = param->type;
	out->supportedLanguages = languages;
	out->option = param->option;
	out->enterLabel = param->enterLabel;
	out->focusShown = param->focusShown;
	sdkVersion = param->sdkVersion;
	if (sdkVersion == 0) {
		out->flags |= 1;
		sdkVersion = IME_VSH_SDK_VERSION_DEFAULT;
	}
	out->sdkVersion = sdkVersion;

	for (i = 0; i < sizeof(param->reserved0); i++) {
		if (*param->reserved0 != 0)
			return SCE_IME_ERROR_INVALID_RESERVED;
	}

	for (i = 0; i < sizeof(param->reserved); i++) {
		if (*param->reserved != 0)
			return SCE_IME_ERROR_INVALID_RESERVED;
	}

	return SCE_OK;
}

/* the caller's parameter, a 0x30-byte one made one of 0x40 bytes */
static inline void imeVshCopyParam(SceImeVshParam *out, const SceImeVshParam *param)
{
	if (param->size == IME_VSH_PARAM_SIZE_OLD) {
		sceClibMemset(out, 0, sizeof(*out));
		sceClibMemcpy_safe(out, param, IME_VSH_PARAM_SIZE_OLD);
		out->size = sizeof(*out);
		out->enterLabel = SCE_IME_ENTER_LABEL_DEFAULT;
		out->sdkVersion = IME_VSH_SDK_VERSION_OLD;
	} else {
		sceClibMemcpy_safe(out, param, sizeof(*out));
	}
}

/* whether the caller is in the foreground: a game, or an application that is activated */
static inline bool imeVshIsActive(void)
{
	SceAppMgrAppStatus status;

	if (sceAppMgrIsGameProgram() != 0)
		return true;

	sceAppMgrGetStatusByName(NULL, &status);

	return status.activated != 0;
}

int32_t sceImeVshSetParam(void *ime, const SceImeVshParam *param)
{
	ImeVshContext *ctx = (ImeVshContext *)ime;
	ImeVshServiceParam serviceParam;
	SceImeVshParam copy;
	int32_t ret = SCE_OK;
	int32_t err;

	ret = sceKernelLockLwMutex(&s_imeVshMutex, 1, NULL);
	if (ret >= 0) {
		if (!s_imeVshOpened)
			ret = SCE_IME_ERROR_NOT_OPENED;
		else if (ctx == NULL)
			ret = SCE_IME_VSH_ERROR_INVALID_HANDLE;
		else if (!imeVshIsActive())
			ret = SCE_OK;
		else {
			ret = SCE_OK;
			imeVshCopyParam(&copy, param);
			err = imeVshCheckParam(&serviceParam, &copy);
			if (err < 0) {
				ret = err;
			} else {
				sceClibMemcpy_safe(&ctx->param, &copy, sizeof(copy));
				ctx->arg = copy.arg;
				ctx->handler = copy.handler;
				ctx->filter = copy.filter;
				err = ctx->client->invokeSyncMethod(IME_SERVICE_METHOD_SET_PARAM,
								    &serviceParam,
								    sizeof(serviceParam), &ret,
								    NULL, NULL, 0);
				if (err < 0) {
					ret = err;
				} else {
					sceClibMemset(ctx->preedit, 0, sizeof(ctx->preedit));
					ctx->waitOpen = true;
				}
			}
		}
	}

	err = ret;
	sceKernelUnlockLwMutex(&s_imeVshMutex, 1);

	return err;
}

/* connects to the IME service */
static int32_t imeVshConnect(IPMI::Client **ppClient, int32_t clientType, SceSize maxOutSize)
{
	IPMI::Client::Config config;
	IPMI::Client *client;
	int32_t result = 0;
	int32_t param;
	int32_t ret, err;

	*ppClient = NULL;
	sceClibMemset(config.serviceName, 0, sizeof(config.serviceName));
	sceClibMemcpy_safe(config.serviceName, "SceImeService", sizeof("SceImeService"));
	config.budget = IPMI::USER_BUDGET;
	if (maxOutSize != 0)
		config.maxAsyncCallOutSize = maxOutSize;

	if (IPMI::Client::getBufferSize(&config) > sizeof(s_clientMemory))
		return SCE_IME_ERROR_NO_MEMORY;

	ret = IPMI::Client::create(&client, &config, NULL, s_clientMemory);
	if (ret != 0)
		return ret;

	param = clientType;
	ret = client->connect(&param, sizeof(param), &result);
	if (ret != 0) {
		switch (ret) {
		case IPMI_ERROR_CONNECTION_REFUSED:
			err = SCE_IME_VSH_ERROR_BUSY;
			break;
		case IPMI_ERROR_SERVER_NOT_FOUND:
			err = SCE_IME_VSH_ERROR_SERVICE_NOT_FOUND;
			break;
		default:
			err = SCE_IME_ERROR_INTERNAL;
			break;
		}
		client->destroy();
		return err;
	}

	*ppClient = client;

	return SCE_OK;
}

/* disconnects from the IME service and destroys the client */
static inline int32_t imeVshDisconnect(IPMI::Client *client)
{
	int32_t ret;

	ret = client->disconnect();
	if (ret < 0)
		return ret;

	ret = client->destroy();
	if (ret < 0)
		return ret;

	return SCE_OK;
}

int32_t sceImeVshOpen(void **ime, const SceImeVshParam *param)
{
	ImeVshServiceParam serviceParam;
	SceImeVshParam copy;
	ImeVshContext *ctx;
	int intercept = 0;
	int32_t ret;

	ret = sceKernelLockLwMutex(&s_imeVshMutex, 1, NULL);
	if (ret >= 0) {
		if (s_imeVshOpened) {
			ret = SCE_IME_ERROR_ALREADY_OPENED;
		} else {
			imeVshCopyParam(&copy, param);
			ret = imeVshCheckParam(&serviceParam, &copy);
			if (ret >= 0) {
				ctx = (ImeVshContext *)copy.work;
				ctx->self = ctx;
				sceClibMemset(ctx, 0, SCE_IME_VSH_WORK_SIZE);
				sceClibMemcpy_safe(&ctx->param, &copy, sizeof(copy));
				if (imeVshConnect(&ctx->client, IME_SERVICE_CLIENT_VSH, 0) < 0) {
					ret = SCE_IME_ERROR_CONNECTION_FAILED;
				} else {
					ctx->serial = 0;
					ctx->arg = copy.arg;
					ctx->handler = copy.handler;
					ctx->filter = copy.filter;
					ctx->getEventsPending = false;
					ctx->caretIndex = 0;
					s_imeVshOpened = true;
					ctx->opened = false;
					ctx->waitOpen = true;
					ctx->eventCount = 0;
					ctx->inserted = false;
					ctx->insertedSpace = false;
					ctx->requestPending = false;
					sceClibMemset(&ctx->request, 0, sizeof(ctx->request));
					if (!s_imeVshOpened)
						ret = SCE_IME_ERROR_NOT_OPENED;
					else if (ctx == NULL)
						ret = SCE_IME_VSH_ERROR_INVALID_HANDLE;
					else if (!imeVshIsActive())
						ret = SCE_OK;
					else
						ret = SCE_OK;
					sceCtrlGetButtonIntercept(&intercept);
					if (intercept == 0) {
						ctx->intercepted = true;
						sceCtrlSetButtonIntercept(1);
						sceHidKeyboardSetIntercept(1);
					}
					*ime = ctx;
				}
			}
		}
	}

	sceKernelUnlockLwMutex(&s_imeVshMutex, 1);

	return ret;
}

/* passes an event of the service to the caller's handler */
static void imeVshDispatchEvent(ImeVshContext *ctx, ImeVshServiceEvent *e)
{
	SceImeVshEvent event;
	wchar_t filtered[IME_VSH_MAX_FILTERED_LENGTH + 32];
	bool open = false;

	sceClibMemset(&event, 0, sizeof(event));
	SceImeVshEventParam param;
	sceClibMemset(&param, 0, sizeof(param));
	event.id = e->id;
	switch (e->id) {
	case SCE_IME_EVENT_OPEN:
		open = true;
		ctx->opened = true;
		/* fall through */
	case SCE_IME_EVENT_CHANGE_SIZE:
		param.rect = e->param.rect;
		break;

	case SCE_IME_EVENT_UPDATE_TEXT:
	case SCE_IME_EVENT_UPDATE_CARET:
		break;

	case SCE_IME_EVENT_PRESS_CLOSE:
		param.showFocus = e->param.showFocus;
		break;

	case SCE_IME_VSH_EVENT_UPDATE_PREEDIT:
		param.text.str = e->param.preedit.str;
		param.text.caretIndex = e->param.preedit.caretIndex;
		ctx->caretIndex = param.text.caretIndex;
		sceClibMemcpy_safe(ctx->preedit, e->param.preedit.str, sizeof(ctx->preedit));
		break;

	case SCE_IME_VSH_EVENT_COMMIT:
	case SCE_IME_VSH_EVENT_INSERT: {
		int32_t (*filter)(wchar_t *, uint32_t *, const wchar_t *, uint32_t) = ctx->filter;

		param.text.str = e->param.text;
		if (filter != NULL) {
			uint32_t length = IME_VSH_MAX_FILTERED_LENGTH;

			if (filter(filtered, &length, e->param.text,
					imeWcsnlen(e->param.text, 30)) != 0) {
				if (length > IME_VSH_MAX_FILTERED_LENGTH)
					length = IME_VSH_MAX_FILTERED_LENGTH;
				sceClibMemcpy_safe(e->param.text, filtered, length * sizeof(wchar_t));
				e->param.text[length] = 0;
			}
		}
		if (e->id == SCE_IME_VSH_EVENT_INSERT) {
			const wchar_t *s = param.text.str;
			wchar_t last = 0;
			uint32_t i;

			for (i = 0; i < 30; i++) {
				wchar_t c = *s;

				if (c == 0)
					break;
				last = c;
				s++;
			}
			ctx->insertedSpace = last == ' ';
			ctx->inserted = true;
		}
		sceClibMemset(ctx->preedit, 0, sizeof(ctx->preedit));
		break;
	}

	case SCE_IME_VSH_EVENT_CLEAR:
		sceClibMemset(ctx->preedit, 0, sizeof(ctx->preedit));
		break;

	case SCE_IME_VSH_EVENT_DELETE:
		param.text.str = ctx->preedit;
		param.text.caretIndex = ctx->caretIndex;
		break;

	case SCE_IME_VSH_EVENT_SENTENCE_SPACE:
		if (!ctx->insertedSpace)
			return;
		event.id = SCE_IME_VSH_EVENT_BACKSPACE;
		break;

	default:
		break;
	}

	event.param = param;
	ctx->handler(ctx->arg, &event);
	if (open) {
		if (ctx->preedit[0] != 0) {
			sceClibMemset(&event, 0, sizeof(event));
			event.param.text.str = ctx->preedit;
			event.id = SCE_IME_VSH_EVENT_COMMIT;
			ctx->handler(ctx->arg, &event);
		}
		sceClibMemset(ctx->preedit, 0, sizeof(ctx->preedit));
	}
}

int32_t sceImeVshUpdate(void *ime)
{
	ImeVshContext *ctx = (ImeVshContext *)ime;
	int32_t ret = SCE_OK;
	int32_t count;
	SceSize outSize;
	ImeVshServiceParam serviceParam;
	ImeVshServiceEvent *e;
	IPMI::Client *client;
	int32_t err, n;

	ret = sceKernelLockLwMutex(&s_imeVshMutex, 1, NULL);
	if (ret < 0)
		goto out;

	if (!s_imeVshOpened) {
		ret = SCE_IME_ERROR_NOT_OPENED;
		goto out;
	}

	if (ctx == NULL) {
		ret = SCE_IME_VSH_ERROR_INVALID_HANDLE;
		goto out;
	}

	if (!imeVshIsActive()) {
		ret = SCE_OK;
		goto out;
	}

	ret = SCE_OK;
	sceClibMemset(&serviceParam, 0, sizeof(serviceParam));
	serviceParam.serial = ctx->serial;
	err = imeVshCheckParam(&serviceParam, &ctx->param);
	if (err < 0) {
		ret = err;
		goto out;
	}

	client = ctx->client;
	if (ctx->requestPending) {
		err = client->invokeSyncMethod(IME_SERVICE_METHOD_UPDATE, &ctx->request,
					       sizeof(ctx->request), &ret, NULL, NULL, 0);
		if (err < 0) {
			ret = err;
			goto out;
		}
		sceClibMemset(&ctx->request, 0, sizeof(ctx->request));
		ctx->requestPending = false;
		client = ctx->client;
	}

	count = 0;
	if (ctx->getEventsPending) {
		err = client->tryGetResult(ctx->requestId, &count, ctx->events, &outSize,
					   sizeof(ctx->events));
		if (err == IPMI_ERROR_WOULD_BLOCK)
			goto received;
		if (err < 0) {
			count = err;
			goto received;
		}
		ctx->getEventsPending = false;
	}

	err = client->invokeAsyncMethod(IME_SERVICE_METHOD_GET_EVENTS, &serviceParam,
					sizeof(serviceParam), &ctx->requestId, NULL);
	if (err < 0) {
		n = err;
		goto check;
	}
	ctx->getEventsPending = true;

received:
	n = count;
check:
	if (n < 0) {
		ret = SCE_IME_ERROR_CONNECTION_FAILED;
		goto out;
	}

	ctx->eventCount = n;
	for (e = ctx->events; ctx->eventCount > 0; e++) {
		if (!s_imeVshOpened)
			break;
		ctx->eventCount--;
		if (ctx->waitOpen) {
			if (e->id != SCE_IME_EVENT_OPEN)
				continue;
			ctx->waitOpen = false;
		}
		imeVshDispatchEvent(ctx, e);
	}

out:
	err = ret;
	sceKernelUnlockLwMutex(&s_imeVshMutex, 1);

	return err;
}

int32_t sceImeVshClearPreedit(void *ime)
{
	ImeVshContext *ctx = (ImeVshContext *)ime;
	int32_t ret;

	ret = sceKernelLockLwMutex(&s_imeVshMutex, 1, NULL);
	if (ret >= 0) {
		if (!s_imeVshOpened)
			ret = SCE_IME_ERROR_NOT_OPENED;
		else if (ctx == NULL)
			ret = SCE_IME_VSH_ERROR_INVALID_HANDLE;
		else if (!imeVshIsActive())
			ret = SCE_OK;
		else {
			ctx->requestPending = true;
			ctx->request.flags |= IME_VSH_REQUEST_STATE;
			ret = SCE_OK;
			ctx->request.state = 1;
		}
	}

	sceKernelUnlockLwMutex(&s_imeVshMutex, 1);

	return ret;
}

int32_t sceImeVshConfirmPreedit(void *ime)
{
	ImeVshContext *ctx = (ImeVshContext *)ime;
	int32_t ret;

	ret = sceKernelLockLwMutex(&s_imeVshMutex, 1, NULL);
	if (ret >= 0) {
		if (!s_imeVshOpened)
			ret = SCE_IME_ERROR_NOT_OPENED;
		else if (ctx == NULL)
			ret = SCE_IME_VSH_ERROR_INVALID_HANDLE;
		else if (!imeVshIsActive())
			ret = SCE_OK;
		else {
			ctx->requestPending = true;
			ctx->request.flags |= IME_VSH_REQUEST_STATE;
			ret = SCE_OK;
			ctx->request.state = 2;
		}
	}

	sceKernelUnlockLwMutex(&s_imeVshMutex, 1);

	return ret;
}

int32_t sceImeVshSetCaretGeometry(void *ime, const SceImeVshCaretGeometry *geometry)
{
	ImeVshContext *ctx = (ImeVshContext *)ime;
	int32_t ret;

	ret = sceKernelLockLwMutex(&s_imeVshMutex, 1, NULL);
	if (ret >= 0) {
		if (!s_imeVshOpened)
			ret = SCE_IME_ERROR_NOT_OPENED;
		else if (ctx == NULL)
			ret = SCE_IME_VSH_ERROR_INVALID_HANDLE;
		else if (!imeVshIsActive())
			ret = SCE_OK;
		else {
			ctx->request.geometry.x = geometry->x;
			ret = SCE_OK;
			ctx->request.geometry.y = geometry->y;
			ctx->request.geometry.height = geometry->height;
			ctx->request.flags |= IME_VSH_REQUEST_GEOMETRY;
			ctx->requestPending = true;
		}
	}

	sceKernelUnlockLwMutex(&s_imeVshMutex, 1);

	return ret;
}

int32_t sceImeVshUpdateContext(void *ime, const wchar_t *text, uint32_t caret)
{
	ImeVshContext *ctx = (ImeVshContext *)ime;
	int32_t ret, i;
	uint32_t sentenceEnd;

	ret = sceKernelLockLwMutex(&s_imeVshMutex, 1, NULL);
	if (ret >= 0) {
		if (!s_imeVshOpened)
			ret = SCE_IME_ERROR_NOT_OPENED;
		else if (ctx == NULL)
			ret = SCE_IME_VSH_ERROR_INVALID_HANDLE;
		else if (!imeVshIsActive())
			ret = SCE_OK;
		else {
			i = caret - 1;
			ret = SCE_OK;
			if (ctx->inserted)
				ctx->inserted = false;
			else
				ctx->insertedSpace = false;
			if (caret != 0 && text[i] != ' ')
				ctx->insertedSpace = false;

			/* the text before the caret ends a sentence: it is empty, or its last
			   character but spaces ends a sentence or a line */
			sentenceEnd = 1;
			for (; i >= 0; i--) {
				wchar_t c = text[i];

				if (c == 0xBF || c == '?' || c == '.' || c == '!')
					break;
				if (c != ' ') {
					if (c != '\r' && c != '\n')
						sentenceEnd = 0;
					break;
				}
			}
			ctx->request.sentenceEnd = sentenceEnd;
			ctx->request.flags |= IME_VSH_REQUEST_CONTEXT;
			ctx->requestPending = true;
		}
	}

	sceKernelUnlockLwMutex(&s_imeVshMutex, 1);

	return ret;
}

int32_t ScePafLowlayer_5CE62E49(void *ime, uint32_t index)
{
	ImeVshContext *ctx = (ImeVshContext *)ime;
	int32_t ret;

	ret = sceKernelLockLwMutex(&s_imeVshMutex, 1, NULL);
	if (ret >= 0) {
		if (!s_imeVshOpened)
			ret = SCE_IME_ERROR_NOT_OPENED;
		else if (ctx == NULL)
			ret = SCE_IME_VSH_ERROR_INVALID_HANDLE;
		else if (!imeVshIsActive())
			ret = SCE_OK;
		else {
			ret = SCE_OK;
			if (ctx->caretIndex != index) {
				ctx->request.caretIndex = index;
				ctx->request.flags |= IME_VSH_REQUEST_CARET;
				ctx->requestPending = true;
			}
		}
	}

	sceKernelUnlockLwMutex(&s_imeVshMutex, 1);

	return ret;
}

int32_t sceImeVshClose(void *ime)
{
	ImeVshContext *ctx = (ImeVshContext *)ime;
	ImeVshServiceParam serviceParam;
	int32_t ret;
	int32_t result;
	SceSize outSize;
	int32_t err;

	ret = sceKernelLockLwMutex(&s_imeVshMutex, 1, NULL);
	if (ret >= 0) {
		if (!s_imeVshOpened) {
			ret = SCE_IME_ERROR_NOT_OPENED;
		} else if (ctx == NULL) {
			ret = SCE_IME_VSH_ERROR_INVALID_HANDLE;
		} else {
			if (ctx->intercepted) {
				ctx->intercepted = false;
				sceCtrlSetButtonIntercept(0);
				sceHidKeyboardSetIntercept(0);
			}
			sceClibMemset(&serviceParam, 0, sizeof(serviceParam));
			serviceParam.serial = ctx->serial;
			err = ctx->client->invokeSyncMethod(IME_SERVICE_METHOD_CLOSE, &serviceParam,
							    sizeof(serviceParam), &ret,
							    NULL, NULL, 0);
			if (err < 0) {
				ret = err;
			} else {
				if (ctx->getEventsPending) {
					/* wait for the events asked for last */
					for (;;) {
						int32_t r = ctx->client->tryGetResult(ctx->requestId, &result,
										      ctx->events, &outSize,
										      sizeof(ctx->events));

						if (r == SCE_OK || r != IPMI_ERROR_WOULD_BLOCK)
							break;
						sceKernelDelayThread(1000);
					}
					ctx->getEventsPending = false;
				}
				if (imeVshDisconnect(ctx->client) < 0)
					ret = SCE_IME_ERROR_CONNECTION_FAILED;
				sceClibMemset(ctx, 0, sizeof(*ctx));
				s_imeVshOpened = false;
			}
		}
	}

	sceKernelUnlockLwMutex(&s_imeVshMutex, 1);

	return ret;
}

int32_t sceImeConfigSet(uint64_t language, int32_t id, int32_t value)
{
	IPMI::Client *client;
	ImeConfigParam param;
	int result;
	int32_t ret, err;

	ret = imeVshConnect(&client, IME_SERVICE_CLIENT_CONFIG, 0);
	if (ret < 0)
		return ret;

	param.operation = 0;
	param.id = id;
	param.value = value;
	param.language = language;
	err = client->invokeSyncMethod(IME_SERVICE_METHOD_CONFIG, &param, sizeof(param), &result,
				       NULL, NULL, 0);
	if (err >= 0)
		err = result;
	ret = SCE_OK;
	if (err < 0)
		ret = err;

	err = imeVshDisconnect(client);
	if (err < 0)
		ret = err;

	return ret;
}

int32_t sceImeConfigGet(uint64_t language, int32_t id, int32_t *value)
{
	IPMI::Client *client;
	ImeConfigParam param;
	SceSize outSize;
	int result;
	int32_t ret, err;

	ret = imeVshConnect(&client, IME_SERVICE_CLIENT_CONFIG, 0);
	if (ret < 0)
		return ret;

	param.id = id;
	ret = SCE_OK;
	param.operation = 1;
	param.language = language;
	param.value = 0;
	err = client->invokeSyncMethod(IME_SERVICE_METHOD_CONFIG, &param, sizeof(param), &result,
				       value, &outSize, sizeof(*value));
	if (err >= 0) {
		if (outSize == sizeof(*value))
			err = result;
		else
			err = SCE_IME_VSH_ERROR_INVALID_RESPONSE;
	}
	if (err < 0)
		ret = err;

	err = imeVshDisconnect(client);
	if (err < 0)
		ret = err;

	return ret;
}

/* calls a user dictionary method of the IME service and waits for its result */
static int32_t imeDicInvokeMethod(int32_t clientType, SceUInt32 methodId, const void *in,
				  SceSize inSize, int *pResult, void *out, SceSize *pOutSize,
				  SceSize outSize)
{
	IPMI::Client *client;
	int requestId;
	int32_t ret, err;

	ret = imeVshConnect(&client, clientType, 0x2000);
	if (ret >= 0) {
		ret = client->invokeAsyncMethod(methodId, in, inSize, &requestId, NULL);
		if (ret >= 0) {
			while ((err = client->tryGetResult(requestId, pResult, out, pOutSize,
							   outSize)) != 0) {
				if (err != IPMI_ERROR_WOULD_BLOCK) {
					ret = err;
					goto done;
				}
				sceKernelDelayThread(1000);
			}
			ret = SCE_OK;
done:
			err = imeVshDisconnect(client);
			if (err < 0)
				ret = err;
		}
	}

	return ret;
}

int32_t sceImeDicGetWords(uint64_t language, void *buffer)
{
	int result;
	SceSize outSize;
	int32_t ret, err;

	if (s_imeVshOpened == true)
		return SCE_IME_VSH_ERROR_BUSY;

	result = 0;
	ret = imeDicInvokeMethod(IME_SERVICE_CLIENT_DIC, IME_SERVICE_METHOD_DIC_GET_WORDS,
				 &language, sizeof(language), &result, buffer, &outSize, 5200);
	err = result;
	if (ret < 0)
		err = ret;

	return err;
}

int32_t sceImeDicAddWord(uint64_t language, const SceImeDicWord *word)
{
	ImeDicWordParam param;
	int result = 0;
	int32_t ret, err;

	if (s_imeVshOpened == true)
		return SCE_IME_VSH_ERROR_BUSY;

	param.language = language;
	param.word = *word;
	ret = imeDicInvokeMethod(IME_SERVICE_CLIENT_DIC, IME_SERVICE_METHOD_DIC_ADD_WORD, &param,
				 sizeof(param), &result, NULL, NULL, 0);
	err = result;
	if (ret < 0)
		err = ret;

	return err;
}

int32_t sceImeDicDeleteWord(uint64_t language, const SceImeDicWord *word)
{
	ImeDicWordParam param;
	int result = 0;
	int32_t ret, err;

	if (s_imeVshOpened == true)
		return SCE_IME_VSH_ERROR_BUSY;

	param.language = language;
	param.word = *word;
	ret = imeDicInvokeMethod(IME_SERVICE_CLIENT_DIC, IME_SERVICE_METHOD_DIC_DELETE_WORD, &param,
				 sizeof(param), &result, NULL, NULL, 0);
	err = result;
	if (ret < 0)
		err = ret;

	return err;
}

int32_t sceImeDicReplaceWord(uint64_t language, const SceImeDicWord *oldWord,
			     const SceImeDicWord *newWord)
{
	ImeDicReplaceWordParam param;
	int result = 0;
	int32_t ret, err;

	if (s_imeVshOpened == true)
		return SCE_IME_VSH_ERROR_BUSY;

	param.language = language;
	param.oldWord = *oldWord;
	param.newWord = *newWord;
	ret = imeDicInvokeMethod(IME_SERVICE_CLIENT_DIC, IME_SERVICE_METHOD_DIC_REPLACE_WORD, &param,
				 sizeof(param), &result, NULL, NULL, 0);
	err = result;
	if (ret < 0)
		err = ret;

	return err;
}

int32_t ScePafLowlayer_8B6B2EA3(void)
{
	int result;
	int32_t ret, err;

	if (s_imeVshOpened == true)
		return SCE_IME_VSH_ERROR_BUSY;

	result = 0;
	ret = imeDicInvokeMethod(IME_SERVICE_CLIENT_DIC, IME_SERVICE_METHOD_DIC_37, NULL, 0, &result,
				 NULL, NULL, 0);
	err = result;
	if (ret < 0)
		err = ret;

	return err;
}

int32_t ScePafLowlayer_7982DC0B(void)
{
	int result;
	int32_t ret, err;

	if (s_imeVshOpened == true)
		return SCE_IME_VSH_ERROR_BUSY;

	result = 0;
	ret = imeDicInvokeMethod(IME_SERVICE_CLIENT_DIC, IME_SERVICE_METHOD_DIC_38, NULL, 0, &result,
				 NULL, NULL, 0);
	err = result;
	if (ret < 0)
		err = ret;

	return err;
}
