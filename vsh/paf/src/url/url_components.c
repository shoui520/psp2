#include "url.h"

extern char *sce_paf_strchr(const char *str, int ch);
extern int sce_paf_strncmp(const char *left, const char *right, size_t len);

typedef enum UrlStringPart {
	UrlPartBase = 1,
	UrlPartHost = 2,
	UrlPartPassword = 5,
	UrlPartPath = 6,
	UrlPartRelativePath = 9,
	UrlPartUser = 10,
	UrlPartScheme = 12
} UrlStringPart;

typedef enum UrlSuffixPart {
	UrlSuffixParameters = 0,
	UrlSuffixAnchor = 1,
	UrlSuffixQuery = 2,
	UrlSuffixPath = 3
} UrlSuffixPart;

static const char urlComponentEmpty[] = "";
static const char urlDelimiters[4][3] = {
	{ ';', '#', '?' },
	{ '#', ';', '?' },
	{ '?', ';', '#' },
	{ '/', ';', 0 }
};

static inline __attribute__((always_inline))
void UrlStringConstruct(PafString *self, const char *str)
{
	PafStringInitialize(self);
	PafStringAssign(self, str, str ? sce_paf_strlen(str) : 0);
}

static inline __attribute__((always_inline))
void UrlCopyComponent(PafString *result, const char *str)
{
	PafString value;

	UrlStringConstruct(&value, str);
	PafStringCopy(result, &value);
	PafStringDestroy(&value);
}

void UrlGetComponent(PafString *result, const Url *self, int32_t part)
{
	PafString host;

	UrlStringConstruct(result, urlComponentEmpty);
	PafStringInitialize(&host);
	if (!self->m_is_valid)
		return;
	switch (part) {
	case UrlPartBase: {
		PafString field, selected, prefix, scheme_prefix, joined, full;
		const char *hostname;

		UrlStringConstruct(&field, self->m_url.hostname);
		if (field.count)
			UrlStringConstruct(&selected, self->m_url.hostname);
		else
			UrlStringConstruct(&selected, "/");
		PafStringCopy(&host, &selected);
		PafStringDestroy(&selected);
		PafStringDestroy(&field);
		UrlStringConstruct(&field, self->m_url.scheme);
		PafStringInitialize(&prefix);
		PafStringCopy(&prefix, &field);
		PafStringAppendBytes(&prefix, "://", sce_paf_strlen("://"));
		PafStringInitialize(&scheme_prefix);
		PafStringCopy(&scheme_prefix, &prefix);
		PafStringDestroy(&prefix);
		hostname = host.ptr;
		PafStringInitialize(&joined);
		PafStringCopy(&joined, &scheme_prefix);
		PafStringAppendBytes(&joined, hostname, sce_paf_strlen(hostname));
		PafStringInitialize(&full);
		PafStringCopy(&full, &joined);
		PafStringDestroy(&joined);
		PafStringCopy(result, &full);
		PafStringDestroy(&full);
		PafStringDestroy(&scheme_prefix);
		PafStringDestroy(&field);
		break;
	}
	case UrlPartHost:
		UrlCopyComponent(result, self->m_url.hostname);
		break;
	case UrlPartPassword:
		UrlCopyComponent(result, self->m_url.password);
		break;
	case UrlPartPath:
	case UrlPartRelativePath:
		UrlCopyComponent(result, self->m_url.path);
		break;
	case UrlPartUser:
		UrlCopyComponent(result, self->m_url.username);
		break;
	case UrlPartScheme:
		UrlCopyComponent(result, self->m_url.scheme);
		break;
	default:
		PafStringAssign(result, urlComponentEmpty,
				sce_paf_strlen(urlComponentEmpty));
		break;
	}
	PafStringDestroy(&host);
}

static inline __attribute__((always_inline))
size_t UrlFindDelimiter(const PafString *self, const char *delimiter)
{
	char ch = *delimiter;
	size_t remaining = self->count;
	char *start = self->ptr;

	if (remaining) {
		do {
			char *match = sce_paf_strchr(start, ch);

			if (!match)
				break;
			if (!sce_paf_strncmp(match, delimiter, 1))
				return match - self->ptr;
			ch = *delimiter;
			remaining -= match - start + 1;
			start = match + 1;
		} while (remaining);
	}
	return (size_t)-1;
}

void UrlGetSuffix(PafString *result, const Url *self, const CommonString *value,
		  int8_t part)
{
	PafString text, suffix, first, second;
	char begin, end_first, end_second;
	size_t pos;
	const char *delimiters = urlDelimiters[part];

	(void)self;
	CommonStringGetString(&text, value);
	UrlStringConstruct(result, text.ptr);
	PafStringDestroy(&text);
	begin = delimiters[0];
	pos = UrlFindDelimiter(result, &begin);
	if (pos == (size_t)-1) {
		PafStringAssign(result, urlComponentEmpty,
				sce_paf_strlen(urlComponentEmpty));
		return;
	}
	PafStringSubstring(&suffix, result, pos, (size_t)-1);
	PafStringCopy(result, &suffix);
	PafStringDestroy(&suffix);
	end_first = delimiters[1];
	pos = UrlFindDelimiter(result, &end_first);
	if (pos != (size_t)-1) {
		PafStringSubstring(&first, result, 0, pos);
		PafStringCopy(result, &first);
		PafStringDestroy(&first);
	}
	end_second = delimiters[2];
	pos = UrlFindDelimiter(result, &end_second);
	if (pos != (size_t)-1) {
		PafStringSubstring(&second, result, 0, pos);
		PafStringCopy(result, &second);
		PafStringDestroy(&second);
	}
}

static inline __attribute__((always_inline))
void UrlAssignComponent(CommonString *self, const PafString *str)
{
	CommonString value;
	CommonString *src = CommonStringConstructString(&value, str);

	PafStringCopy(&self->m_string, &src->m_string);
	PafWStringCopy(&self->m_wstring, &src->m_wstring);
	CommonStringDestroy(&value);
}

static inline __attribute__((always_inline))
void UrlLoadComponent(CommonString *value, const Url *self, UrlStringPart part)
{
	PafString str;

	UrlGetComponent(&str, self, part);
	UrlAssignComponent(value, &str);
	PafStringDestroy(&str);
}

void UrlGetHostString(PafString *result, const Url *self)
{
	CommonString value;
	PafString text, original;

	CommonStringConstruct(&value);
	CommonStringGetString(&text, &self->m_urlstring);
	UrlStringConstruct(&original, text.ptr);
	PafStringDestroy(&text);
	UrlLoadComponent(&value, self, UrlPartHost);
	CommonStringGetString(result, &value);
	PafStringDestroy(&original);
	CommonStringDestroy(&value);
}

static inline __attribute__((always_inline))
void UrlGetSuffixString(PafString *result, const Url *self, UrlSuffixPart part)
{
	PafString str;
	CommonString value;

	UrlGetSuffix(&str, self, &self->m_urlstring, part);
	CommonStringConstructString(&value, &str);
	PafStringDestroy(&str);
	CommonStringGetString(result, &value);
	CommonStringDestroy(&value);
}

void UrlGetParamaterString(PafString *result, const Url *self)
{
	UrlGetSuffixString(result, self, UrlSuffixParameters);
}

void UrlGetAnchorString(PafString *result, const Url *self)
{
	UrlGetSuffixString(result, self, UrlSuffixAnchor);
}

void UrlGetPasswordString(PafString *result, const Url *self)
{
	CommonString value;

	CommonStringConstruct(&value);
	UrlLoadComponent(&value, self, UrlPartPassword);
	CommonStringGetString(result, &value);
	CommonStringDestroy(&value);
}

void UrlGetPathString(PafString *result, const Url *self)
{
	CommonString value;
	PafString str;

	CommonStringConstruct(&value);
	UrlLoadComponent(&value, self, UrlPartPath);
	UrlGetSuffix(&str, self, &value, UrlSuffixPath);
	UrlAssignComponent(&value, &str);
	PafStringDestroy(&str);
	CommonStringGetString(result, &value);
	CommonStringDestroy(&value);
}

void UrlGetQueryString(PafString *result, const Url *self)
{
	UrlGetSuffixString(result, self, UrlSuffixQuery);
}

void UrlGetUserString(PafString *result, const Url *self)
{
	CommonString value;

	CommonStringConstruct(&value);
	UrlLoadComponent(&value, self, UrlPartUser);
	CommonStringGetString(result, &value);
	CommonStringDestroy(&value);
}

void UrlGetSchemeString(PafString *result, const Url *self)
{
	CommonString value;

	CommonStringConstruct(&value);
	UrlLoadComponent(&value, self, UrlPartScheme);
	CommonStringGetString(result, &value);
	CommonStringDestroy(&value);
}
