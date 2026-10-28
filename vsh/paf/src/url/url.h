#ifndef PAF_URL_URL_H
#define PAF_URL_URL_H

#include "../std/string.h"
#include "../common/string_util.h"
#include "uri.h"

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

#endif
