#include "string_util.h"
#include "char_encoding.h"

static const char commonStringEmpty[] = "";
static const uint16_t commonWStringEmpty[] = { 0 };

CommonString *CommonStringDestroy(CommonString *self)
{
	if (self->m_wstring.ptr != &PafWStringEmpty)
		ScePafStdc_1B77082E(self->m_wstring.ptr);
	self->m_wstring.ptr = (uint16_t *)-1;
	self->m_wstring.count = (size_t)-1;
	PafStringDestroy(&self->m_string);
	return self;
}

CommonString *CommonStringConstruct(CommonString *self)
{
	PafStringInitialize(&self->m_string);
	PafStringAssign(&self->m_string, commonStringEmpty,
			sce_paf_strlen(commonStringEmpty));
	self->m_wstring.count = 0;
	self->m_wstring.ptr = &PafWStringEmpty;
	PafWStringAssign(&self->m_wstring, commonWStringEmpty,
			 sce_paf_wcslen(commonWStringEmpty));
	return self;
}

CommonString *CommonStringConstructBase(CommonString *self)
{
	return CommonStringConstruct(self);
}

CommonString *CommonStringConstructChar(CommonString *self, const char *str)
{
	PafStringInitialize(&self->m_string);
	PafStringAssign(&self->m_string, str, str ? sce_paf_strlen(str) : 0);
	self->m_wstring.ptr = &PafWStringEmpty;
	self->m_wstring.count = 0;
	PafWStringAssign(&self->m_wstring, commonWStringEmpty,
			 sce_paf_wcslen(commonWStringEmpty));
	return self;
}

CommonString *CommonStringConstructCharBase(CommonString *self, const char *str)
{
	return CommonStringConstructChar(self, str);
}

CommonString *CommonStringConstructWideChar(CommonString *self, const uint16_t *str)
{
	PafStringInitialize(&self->m_string);
	PafStringAssign(&self->m_string, commonStringEmpty,
			sce_paf_strlen(commonStringEmpty));
	self->m_wstring.count = 0;
	self->m_wstring.ptr = &PafWStringEmpty;
	PafWStringAssign(&self->m_wstring, str, str ? sce_paf_wcslen(str) : 0);
	return self;
}

CommonString *CommonStringConstructWideCharBase(CommonString *self, const uint16_t *str)
{
	return CommonStringConstructWideChar(self, str);
}

CommonString *CommonStringConstructString(CommonString *self, const PafString *src)
{
	PafStringInitialize(&self->m_string);
	PafStringCopy(&self->m_string, src);
	self->m_wstring.count = 0;
	self->m_wstring.ptr = &PafWStringEmpty;
	PafWStringAssign(&self->m_wstring, commonWStringEmpty,
			 sce_paf_wcslen(commonWStringEmpty));
	return self;
}

CommonString *CommonStringConstructStringBase(CommonString *self, const PafString *src)
{
	return CommonStringConstructString(self, src);
}

CommonString *CommonStringConstructWString(CommonString *self, const PafWString *src)
{
	PafStringInitialize(&self->m_string);
	PafStringAssign(&self->m_string, commonStringEmpty,
			sce_paf_strlen(commonStringEmpty));
	self->m_wstring.count = 0;
	self->m_wstring.ptr = &PafWStringEmpty;
	PafWStringCopy(&self->m_wstring, src);
	return self;
}

CommonString *CommonStringConstructWStringBase(CommonString *self, const PafWString *src)
{
	return CommonStringConstructWString(self, src);
}

CommonString *CommonStringDestroyBase(CommonString *self)
{
	return CommonStringDestroy(self);
}

void CommonStringGetString(PafString *result, const CommonString *self)
{
	PafStringInitialize(result);
	PafStringCopy(result, &self->m_string);
	if (!result->count && self->m_wstring.count)
		Utf16StringToUtf8Default(&self->m_wstring, result);
}

void CommonStringGetWString(PafWString *result, const CommonString *self)
{
	result->ptr = &PafWStringEmpty;
	result->count = 0;
	PafWStringCopy(result, &self->m_wstring);
	if (!result->count && self->m_string.count)
		Utf8StringToUtf16(&self->m_string, result, 0);
}
