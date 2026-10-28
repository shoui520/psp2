#include "url.h"

extern void *ScePafStdc_D8CC1AFE(size_t size);
extern void ScePafStdc_5C468294(void *ptr);
extern char *sce_paf_strchr(const char *str, int ch);
extern int sce_paf_strncmp(const char *left, const char *right, size_t len);

const UrlClass urlClass = { 0, NULL, { UrlDestroy, UrlDelete } };

static const char urlEmpty[] = "";

int UrlParseUrl(Url *self)
{
	PafString value;
	SceSize require, actual;
	int empty, ret;
	const char *scheme;
	size_t len, found = (size_t)-1;

	self->m_is_valid = 0;
	CommonStringGetString(&value, &self->m_urlstring);
	empty = !value.count;
	PafStringDestroy(&value);
	if (empty)
		return 0;
	CommonStringGetString(&value, &self->m_urlstring);
	ret = UrlUriParse(NULL, value.ptr, NULL, &require, 0);
	PafStringDestroy(&value);
	if (ret)
		return 0;
	if (self->m_pool) {
		ScePafStdc_5C468294(self->m_pool);
		self->m_pool = NULL;
	}
	self->m_pool = ScePafStdc_D8CC1AFE(4 * require);
	CommonStringGetString(&value, &self->m_urlstring);
	ret = UrlUriParse(&self->m_url, value.ptr, self->m_pool, &actual, require);
	PafStringDestroy(&value);
	if (ret) {
		if (self->m_pool) {
			ScePafStdc_5C468294(self->m_pool);
			self->m_pool = NULL;
		}
		return 0;
	}
	scheme = self->m_url.scheme;
	PafStringInitialize(&value);
	PafStringAssign(&value, scheme, scheme ? sce_paf_strlen(scheme) : 0);
	len = sce_paf_strlen("file");
	if (!len) {
		found = 0;
	} else if (value.count && len <= value.count) {
		size_t remaining = value.count;
		char *start = value.ptr;

		do {
			char *match = sce_paf_strchr(start, 'f');

			if (!match)
				break;
			if (!sce_paf_strncmp(match, "file", len)) {
				found = match - value.ptr;
				break;
			}
			remaining -= match - start + 1;
			start = match + 1;
		} while (remaining);
	}
	PafStringDestroy(&value);
	if (found != (size_t)-1)
		self->m_is_file = 1;
	self->m_is_valid = 1;
	return 1;
}

int UrlInitialize(Url *self, const PafString *url, unsigned char initialize_pool)
{
	CommonString empty, value;
	CommonString *src;

	self->m_is_file = 0;
	self->m_is_valid = 0;
	src = CommonStringConstructChar(&empty, urlEmpty);
	PafStringCopy(&self->m_urlstring.m_string, &src->m_string);
	PafWStringCopy(&self->m_urlstring.m_wstring, &src->m_wstring);
	CommonStringDestroy(&empty);
	sce_paf_memset(&self->m_url, 0, sizeof(self->m_url));
	if (initialize_pool)
		self->m_pool = NULL;
	if (url->count) {
		src = CommonStringConstructString(&value, url);
		PafStringCopy(&self->m_urlstring.m_string, &src->m_string);
		PafWStringCopy(&self->m_urlstring.m_wstring, &src->m_wstring);
		CommonStringDestroy(&value);
		UrlParseUrl(self);
	}
	return 1;
}

Url *UrlConstruct(Url *self)
{
	PafString empty;

	self->vtable = &urlClass.methods;
	CommonStringConstruct(&self->m_urlstring);
	PafStringInitialize(&empty);
	PafStringAssign(&empty, urlEmpty, sce_paf_strlen(urlEmpty));
	UrlInitialize(self, &empty, 1);
	PafStringDestroy(&empty);
	return self;
}

Url *UrlConstructBase(Url *self)
{
	return UrlConstruct(self);
}

Url *UrlConstructString(Url *self, const PafString *url)
{
	self->vtable = &urlClass.methods;
	CommonStringConstruct(&self->m_urlstring);
	UrlInitialize(self, url, 1);
	return self;
}

Url *UrlConstructStringBase(Url *self, const PafString *url)
{
	return UrlConstructString(self, url);
}

void UrlGetUrlString(PafString *result, const Url *self)
{
	CommonString value;

	CommonStringConstruct(&value);
	PafStringCopy(&value.m_string, &self->m_urlstring.m_string);
	PafWStringCopy(&value.m_wstring, &self->m_urlstring.m_wstring);
	CommonStringGetString(result, &value);
	CommonStringDestroy(&value);
}

int UrlReleasePool(Url *self)
{
	if (self->m_pool) {
		ScePafStdc_5C468294(self->m_pool);
		self->m_pool = NULL;
	}
	return 0;
}

Url *UrlDestroy(Url *self)
{
	void *pool = self->m_pool;

	self->vtable = &urlClass.methods;
	if (pool) {
		ScePafStdc_5C468294(pool);
		self->m_pool = NULL;
	}
	CommonStringDestroy(&self->m_urlstring);
	return self;
}

void UrlDelete(Url *self)
{
	UrlDestroy(self);
	ScePafStdc_1B77082E(self);
}

Url *UrlDestroyBase(Url *self)
{
	return UrlDestroy(self);
}

Url *UrlAssignString(Url *self, const PafString *url)
{
	UrlInitialize(self, url, 0);
	return self;
}

int32_t UrlGetPort(const Url *self)
{
	return self->m_url.port;
}

unsigned char UrlIsValid(const Url *self)
{
	return self->m_is_valid;
}
