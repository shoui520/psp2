#ifndef PAF_URL_URL_H
#define PAF_URL_URL_H

#include "../std/string.h"
#include "uri.h"

/* The C view of paf::common::String (include/common/string_util.h) until url is C++. */
typedef struct CommonString {
	PafString m_string;
	PafWString m_wstring;
} CommonString;

typedef struct Url Url;
typedef struct UrlVTable {
	Url *(*Destroy)(Url *self);
	void (*Delete)(Url *self);
} UrlVTable;

typedef struct UrlClass {
	int32_t offset_to_top;
	const void *type_info;
	UrlVTable methods;
} UrlClass;

struct Url {
	const UrlVTable *vtable;
	SceHttpUriElement m_url;
	void *m_pool;
	unsigned char m_is_valid;
	unsigned char m_is_file;
	CommonString m_urlstring;
};

typedef char UrlSizeCheck[sizeof(Url) == 80 ? 1 : -1];
typedef char UrlPoolOffsetCheck[offsetof(Url, m_pool) == 48 ? 1 : -1];
typedef char UrlStringOffsetCheck[offsetof(Url, m_urlstring) == 56 ? 1 : -1];
typedef char UrlClassSizeCheck[sizeof(UrlClass) == 16 ? 1 : -1];

#define UrlConstruct ScePafMisc_B998FE26
#define UrlConstructBase ScePafMisc_4C6B761F
#define UrlConstructString ScePafMisc_67BA6456
#define UrlConstructStringBase ScePafMisc_987D8B51
#define UrlGetUrlString ScePafMisc_9120E3DA
#define UrlReleasePool ScePafMisc_529F328C
#define UrlDestroy ScePafMisc_269A36D7
#define UrlDelete ScePafMisc_D19653CB
#define UrlDestroyBase ScePafMisc_445AB346
#define UrlAssignString ScePafMisc_00CF14FB
#define UrlConstructWString ScePafMisc_6FAAC9C5
#define UrlConstructWStringBase ScePafMisc_5A5BB7D4
#define UrlConstructCopy ScePafMisc_56625D15
#define UrlConstructCopyBase ScePafMisc_FEB5AE77
#define UrlAssignWString ScePafMisc_34716241
#define UrlAssign ScePafMisc_8F7AEE8C
#define UrlGetUrlWString ScePafMisc_52C507C3

extern const UrlClass urlClass;
int UrlParseUrl(Url *self);
int UrlInitialize(Url *self, const PafString *url, unsigned char initialize_pool);
void UrlGetComponent(PafString *result, const Url *self, int32_t part);
void UrlGetSuffix(PafString *result, const Url *self, const CommonString *value,
		  int8_t part);
Url *UrlConstruct(Url *self);
Url *UrlConstructBase(Url *self);
Url *UrlConstructString(Url *self, const PafString *url);
Url *UrlConstructStringBase(Url *self, const PafString *url);
void UrlGetUrlString(PafString *result, const Url *self);
int UrlReleasePool(Url *self);
Url *UrlDestroy(Url *self);
void UrlDelete(Url *self);
Url *UrlDestroyBase(Url *self);
Url *UrlAssignString(Url *self, const PafString *url);
Url *UrlConstructWString(Url *self, const PafWString *url);
Url *UrlConstructWStringBase(Url *self, const PafWString *url);
Url *UrlConstructCopy(Url *self, const Url *url);
Url *UrlConstructCopyBase(Url *self, const Url *url);
Url *UrlAssignWString(Url *self, const PafWString *url);
Url *UrlAssign(Url *self, const Url *url);
void UrlGetUrlWString(PafWString *result, const Url *self);

#define UrlIsValid ScePafMisc_0E846E5C
#define UrlGetSchemeString ScePafMisc_4D885C6E
#define UrlGetHostString ScePafMisc_0ACE5EBA
#define UrlGetPort ScePafMisc_D00CCC22
#define UrlGetUserString ScePafMisc_DD45D368
#define UrlGetPasswordString ScePafMisc_BF130361
#define UrlGetPathString ScePafMisc_1E5D19F2
#define UrlGetQueryString ScePafMisc_63446442
#define UrlGetAnchorString ScePafMisc_23F9DD51
#define UrlGetParamaterString ScePafMisc_ADCFF4C7

unsigned char UrlIsValid(const Url *self);
void UrlGetSchemeString(PafString *result, const Url *self);
void UrlGetHostString(PafString *result, const Url *self);
int32_t UrlGetPort(const Url *self);
void UrlGetUserString(PafString *result, const Url *self);
void UrlGetPasswordString(PafString *result, const Url *self);
void UrlGetPathString(PafString *result, const Url *self);
void UrlGetQueryString(PafString *result, const Url *self);
void UrlGetAnchorString(PafString *result, const Url *self);
void UrlGetParamaterString(PafString *result, const Url *self);

#define UrlGetBaseUrlString ScePafMisc_5598485B
#define UrlGetBaseUrlWString ScePafMisc_B0A064AE
#define UrlGetHostWString ScePafMisc_7DEBD39D
#define UrlGetParamaterWString ScePafMisc_635A26CE
#define UrlGetAnchorWString ScePafMisc_1A33F18C
#define UrlGetPasswordWString ScePafMisc_7FE63098
#define UrlGetPathWString ScePafMisc_CC331BFF
#define UrlGetQueryWString ScePafMisc_F94D3CD2
#define UrlGetRelativePathString ScePafMisc_2D4F45CA
#define UrlGetRelativePathWString ScePafMisc_DED512ED
#define UrlGetUserWString ScePafMisc_4352F231
#define UrlCopyString ScePafMisc_14C9C3D8
#define UrlCopyWString ScePafMisc_2E04CDAE
#define UrlGetSchemeWString ScePafMisc_7E3F524A

void UrlGetBaseUrlString(PafString *result, const Url *self);
void UrlGetBaseUrlWString(PafWString *result, const Url *self);
void UrlGetHostWString(PafWString *result, const Url *self);
void UrlGetParamaterWString(PafWString *result, const Url *self);
void UrlGetAnchorWString(PafWString *result, const Url *self);
void UrlGetPasswordWString(PafWString *result, const Url *self);
void UrlGetPathWString(PafWString *result, const Url *self);
void UrlGetQueryWString(PafWString *result, const Url *self);
void UrlGetRelativePathString(PafString *result, const Url *self);
void UrlGetRelativePathWString(PafWString *result, const Url *self);
void UrlGetUserWString(PafWString *result, const Url *self);
void UrlCopyString(PafString *result, const Url *self);
void UrlCopyWString(PafWString *result, const Url *self);
void UrlGetSchemeWString(PafWString *result, const Url *self);

#endif
