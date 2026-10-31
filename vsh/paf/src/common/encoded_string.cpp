/*
 * SjisString and EncodedString: a string kept in a narrow form, a UTF-16 form
 * or both, the missing form made from the other the first time it is asked for
 * (common/encoded_string.h). SjisString converts through libces's Shift_JIS
 * (CP932) profile, set up on first use; EncodedString through UTF-8.
 */
#include <common/encoded_string.h>
#include <std/new>
#include <std/string.h>
#include <ces/libces_ucs.h>
#include <ces/libces_sbcs.h>
#include <ces/libces_jis.h>
#include <ces/libces_mbcs.h>

using paf::string;
using paf::wstring;
using paf::common::SjisString;
using paf::common::EncodedString;

static SceCesSJisUcsProfile *s_sjis_profile;
static SceCesUcsProfileSheet s_sjis_sheet;

/* SjisString */

static inline void SjisAssignSjis(SjisString *self, const char *sjis)
{
	ScePafMisc_8FDF9295(self);
	self->m_sjis = new string(sjis);
}

static inline void SjisAssignString(SjisString *self, const string &sjis)
{
	ScePafMisc_8FDF9295(self);
	self->m_sjis = new string(sjis);
}

static inline void SjisAssignUtf16(SjisString *self, const wchar_t *src)
{
	ScePafMisc_8FDF9295(self);
	self->m_wstring = new wstring(src);
	self->m_sjis = ScePafMisc_BE18253B(self->m_wstring->c_str(), self->m_sjis);
}

static inline void SjisAssignWString(SjisString *self, const wstring &src)
{
	ScePafMisc_8FDF9295(self);
	self->m_sjis = ScePafMisc_BE18253B(src.c_str(), self->m_sjis);
}

static inline const char *SjisGetSjis(SjisString *self)
{
	if (self->m_sjis == NULL) {
		if (self->m_wstring == NULL)
			return NULL;
		self->m_sjis = ScePafMisc_BE18253B(self->m_wstring->c_str(), NULL);
		if (self->m_sjis == NULL)
			return NULL;
	}
	return self->m_sjis->c_str();
}

static inline const wchar_t *SjisGetUtf16(SjisString *self)
{
	if (self->m_wstring == NULL) {
		if (self->m_sjis == NULL)
			return NULL;
		self->m_wstring = ScePafMisc_D27C91C0(self->m_sjis->c_str(), NULL);
		if (self->m_wstring == NULL)
			return NULL;
	}
	return self->m_wstring->c_str();
}

extern "C" SjisString *ScePafMisc_DF4C99BA(SjisString *self)
{
	self->m_wstring = NULL;
	self->m_sjis = NULL;
	return self;
}

extern "C" SjisString *ScePafMisc_68A6B37B(SjisString *self)
{
	self->m_wstring = NULL;
	self->m_sjis = NULL;
	return self;
}

extern "C" SjisString *ScePafMisc_FD631CC2(SjisString *self, const char *sjis)
{
	self->m_sjis = new string(sjis);
	self->m_wstring = NULL;
	return self;
}

extern "C" SjisString *ScePafMisc_CE859024(SjisString *self, const char *sjis)
{
	self->m_sjis = new string(sjis);
	self->m_wstring = NULL;
	return self;
}

extern "C" SjisString *ScePafMisc_AB04B8B7(SjisString *self, const char *sjis)
{
	self->m_sjis = new string(sjis);
	self->m_wstring = NULL;
	return self;
}

extern "C" SjisString *ScePafMisc_EEBED3C6(SjisString *self, const char *sjis)
{
	self->m_sjis = new string(sjis);
	self->m_wstring = NULL;
	return self;
}

extern "C" SjisString *ScePafMisc_49706ED5(SjisString *self, const string &sjis)
{
	self->m_sjis = new string(sjis);
	self->m_wstring = NULL;
	return self;
}

extern "C" SjisString *ScePafMisc_749CA742(SjisString *self, const string &sjis)
{
	self->m_sjis = new string(sjis);
	self->m_wstring = NULL;
	return self;
}

/* UTF-16 to Shift_JIS, into dst or a new string. */
extern "C" string *ScePafMisc_BE18253B(const wchar_t *src, string *dst)
{
	SceCesMbcsUcsContext context;
	uint32_t dst_len, src_len;
	size_t len;

	if (s_sjis_profile == NULL)
		s_sjis_profile = sceCesUcsProfileInitSJis1997Cp932(&s_sjis_sheet);
	sceCesMbcsUcsContextInit(&context, &s_sjis_profile->cesMbcsUcsPrf);
	sceCesUtf16StrGetMbcsLen(&context, (const uint16_t *)src, 0, &src_len, &dst_len);
	len = dst_len;
	if (dst != NULL)
		dst->resize(len);
	else
		dst = new string(len, 0);
	if (len != 0)
		sceCesUtf16StrToMbcsStr(&context, (const uint16_t *)src, 0, &src_len, (uint8_t *)dst->ptr,
					len + 1, &dst_len);
	return dst;
}

extern "C" string *ScePafMisc_3BC42454(const wstring &src, string *dst)
{
	return ScePafMisc_BE18253B(src.c_str(), dst);
}

extern "C" SjisString *ScePafMisc_8FDF9295(SjisString *self)
{
	delete self->m_sjis;
	delete self->m_wstring;
	self->m_sjis = NULL;
	self->m_wstring = NULL;
	return self;
}

extern "C" SjisString *ScePafMisc_15A2CCBE(SjisString *self, const wchar_t *src)
{
	SjisAssignUtf16(self, src);
	return self;
}

extern "C" SjisString *ScePafMisc_4638FBA1(SjisString *self, const wchar_t *src)
{
	self->m_sjis = NULL;
	self->m_wstring = NULL;
	SjisAssignUtf16(self, src);
	return self;
}

extern "C" SjisString *ScePafMisc_0ED06370(SjisString *self, const wchar_t *src)
{
	self->m_sjis = NULL;
	self->m_wstring = NULL;
	SjisAssignUtf16(self, src);
	return self;
}

extern "C" SjisString *ScePafMisc_E3DE6624(SjisString *self, const wstring &src)
{
	SjisAssignWString(self, src);
	return self;
}

extern "C" SjisString *ScePafMisc_376DFF8C(SjisString *self, const wstring &src)
{
	self->m_sjis = NULL;
	self->m_wstring = NULL;
	SjisAssignWString(self, src);
	return self;
}

extern "C" SjisString *ScePafMisc_3BE6AFE6(SjisString *self, const wstring &src)
{
	self->m_sjis = NULL;
	self->m_wstring = NULL;
	SjisAssignWString(self, src);
	return self;
}

extern "C" SjisString *ScePafMisc_C1DE0AC2(SjisString *self, const string &sjis)
{
	SjisAssignString(self, sjis);
	return self;
}

extern "C" SjisString *ScePafMisc_EF52733C(SjisString *self, const SjisString &src)
{
	if (src.m_sjis != NULL)
		SjisAssignString(self, *src.m_sjis);
	else
		ScePafMisc_8FDF9295(self);
	return self;
}

extern "C" SjisString *ScePafMisc_36CF059C(SjisString *self, const SjisString &src)
{
	self->m_sjis = NULL;
	self->m_wstring = NULL;
	if (src.m_sjis != NULL)
		SjisAssignString(self, *src.m_sjis);
	else
		ScePafMisc_8FDF9295(self);
	return self;
}

extern "C" SjisString *ScePafMisc_1B896933(SjisString *self, const SjisString &src)
{
	self->m_sjis = NULL;
	self->m_wstring = NULL;
	if (src.m_sjis != NULL)
		SjisAssignString(self, *src.m_sjis);
	else
		ScePafMisc_8FDF9295(self);
	return self;
}

extern "C" SjisString *ScePafMisc_ACE64B1E(SjisString *self, EncodedString &src)
{
	self->m_sjis = NULL;
	self->m_wstring = NULL;
	SjisAssignUtf16(self, ScePafMisc_53948F03(&src));
	return self;
}

extern "C" SjisString *ScePafMisc_928EBAC4(SjisString *self, EncodedString &src)
{
	self->m_sjis = NULL;
	self->m_wstring = NULL;
	SjisAssignUtf16(self, ScePafMisc_53948F03(&src));
	return self;
}

extern "C" SjisString *ScePafMisc_5BAA3517(SjisString *self, const char *sjis)
{
	SjisAssignSjis(self, sjis);
	return self;
}

extern "C" SjisString *ScePafMisc_AD5E57B0(SjisString *self, const char *sjis)
{
	SjisAssignSjis(self, sjis);
	return self;
}

extern "C" SjisString *ScePafMisc_6DA00D89(SjisString *self, const char *sjis)
{
	if (sjis != NULL) {
		if (self->m_sjis != NULL)
			self->m_sjis->append(sjis);
		else
			SjisAssignSjis(self, sjis);
	}
	return self;
}

extern "C" SjisString *ScePafMisc_0587A58E(SjisString *self, const char *sjis)
{
	return ScePafMisc_6DA00D89(self, sjis);
}

extern "C" SjisString *ScePafMisc_2D5AD1EF(SjisString *self, const string &sjis)
{
	if (sjis.size() == 0)
		return self;
	if (self->m_sjis != NULL)
		self->m_sjis->append(sjis);
	else
		SjisAssignString(self, sjis);
	return self;
}

extern "C" SjisString *ScePafMisc_AD3D3047(SjisString *self)
{
	ScePafMisc_8FDF9295(self);
	return self;
}

extern "C" SjisString *ScePafMisc_4CC6F401(SjisString *self, const wchar_t *src)
{
	if (src != NULL) {
		SjisString converted;

		converted.m_sjis = NULL;
		converted.m_wstring = NULL;
		SjisAssignUtf16(&converted, src);
		if (self->m_sjis != NULL)
			self->m_sjis->append(*converted.m_sjis);
		else
			SjisAssignUtf16(self, src);
		ScePafMisc_8FDF9295(&converted);
	}
	return self;
}

extern "C" SjisString *ScePafMisc_4CD8E682(SjisString *self, const wstring &src)
{
	if (src.size() != 0) {
		if (self->m_sjis == NULL) {
			SjisAssignWString(self, src);
		} else {
			string converted;

			ScePafMisc_BE18253B(src.c_str(), &converted);
			self->m_sjis->append(converted);
		}
	}
	return self;
}

extern "C" SjisString *ScePafMisc_2678FFEB(SjisString *self, const SjisString &src)
{
	if (src.m_sjis == NULL)
		return self;
	if (self->m_sjis != NULL)
		self->m_sjis->append(*src.m_sjis);
	else
		SjisAssignString(self, *src.m_sjis);
	return self;
}

/* Shift_JIS to UTF-16, into dst or a new string. */
extern "C" wstring *ScePafMisc_D27C91C0(const char *src, wstring *dst)
{
	SceCesMbcsUcsContext context;
	uint32_t src_len, dst_len;
	size_t len;

	if (s_sjis_profile == NULL)
		s_sjis_profile = sceCesUcsProfileInitSJis1997Cp932(&s_sjis_sheet);
	sceCesMbcsUcsContextInit(&context, &s_sjis_profile->cesMbcsUcsPrf);
	sceCesMbcsStrGetUtf16Len(&context, (const uint8_t *)src, 0, &src_len, &dst_len);
	len = dst_len;
	if (dst != NULL)
		dst->resize(len);
	else
		dst = new wstring(len, 0);
	if (len != 0)
		sceCesMbcsStrToUtf16Str(&context, (const uint8_t *)src, 0, &src_len, (uint16_t *)dst->ptr,
					len + 1, &dst_len);
	return dst;
}

extern "C" wstring *ScePafMisc_F080858F(const string &src, wstring *dst)
{
	return ScePafMisc_D27C91C0(src.c_str(), dst);
}

extern "C" const char *ScePafMisc_8193E3E1(SjisString *self)
{
	return SjisGetSjis(self);
}

extern "C" const wchar_t *ScePafMisc_C3E27C0A(SjisString *self)
{
	return SjisGetUtf16(self);
}

/* The Shift_JIS form's size in bytes with its NUL. */
extern "C" size_t ScePafMisc_EDE4297A(SjisString *self)
{
	if (self->m_sjis == NULL) {
		if (self->m_wstring == NULL)
			return 0;
		self->m_sjis = ScePafMisc_BE18253B(self->m_wstring->c_str(), NULL);
		if (self->m_sjis == NULL)
			return 0;
	}
	return self->m_sjis->size() + 1;
}

/* The UTF-16 form's size in bytes with its NUL; 0 while there is none. */
extern "C" size_t ScePafMisc_C160806C(const SjisString *self)
{
	if (self->m_wstring == NULL)
		return 0;
	return self->m_wstring->size() * sizeof(wchar_t) + sizeof(wchar_t);
}

extern "C" string ScePafMisc_A05DD777(const SjisString *self)
{
	if (self->m_sjis != NULL)
		return *self->m_sjis;

	string converted;

	ScePafMisc_BE18253B(self->m_wstring->c_str(), &converted);
	return converted;
}

extern "C" wstring ScePafMisc_F77AD7C3(const SjisString *self)
{
	if (self->m_wstring != NULL)
		return *self->m_wstring;

	wstring converted;

	ScePafMisc_D27C91C0(self->m_sjis->c_str(), &converted);
	return converted;
}

extern "C" bool ScePafMisc_5C6CCB4A(SjisString *self, SjisString &other)
{
	const char *left = SjisGetSjis(self);
	const char *right = SjisGetSjis(&other);
	size_t len = self->m_sjis->size();

	if (len != other.m_sjis->size())
		return false;
	if (len != 0)
		return sce_paf_strcmp(left, right) == 0;
	return true;
}

extern "C" bool ScePafMisc_10271650(SjisString *self, EncodedString &other)
{
	const wchar_t *left = SjisGetUtf16(self);
	const wchar_t *right = ScePafMisc_53948F03(&other);
	size_t len = self->m_wstring->size();

	if (len != (ScePafMisc_183DD5C3(&other) * sizeof(wchar_t) + sizeof(wchar_t)) / sizeof(wchar_t) - 1)
		return false;
	if (len != 0)
		return sce_paf_wcscmp(left, right) == 0;
	return true;
}

extern "C" bool ScePafMisc_41A5D96E(SjisString *self, const wstring &other)
{
	const wchar_t *left = SjisGetUtf16(self);
	const wchar_t *right = other.c_str();
	size_t len = self->m_wstring->size();

	if (len != other.size())
		return false;
	if (len != 0)
		return sce_paf_wcscmp(left, right) == 0;
	return true;
}

extern "C" bool ScePafMisc_B15E3890(SjisString *self, const wchar_t *other)
{
	const wchar_t *left = SjisGetUtf16(self);

	if (self->m_wstring->size() != 0)
		return sce_paf_wcscmp(left, other) == 0;
	return *other == 0;
}

extern "C" SjisString *ScePafMisc_4B6BCFAF(SjisString *self)
{
	ScePafMisc_8FDF9295(self);
	return self;
}

/* EncodedString */

static inline void EncSetString(EncodedString *self, const string &src, int encoding)
{
	ScePafMisc_AF142E26(self);
	if (encoding == EncodedString::Encoding_Utf16) {
		self->m_wstring = ScePafMisc_A6ABC0B2(src.c_str(), self->m_wstring);
	} else if (self->m_string != NULL) {
		self->m_string->assign(src);
	} else {
		self->m_string = new string(src);
	}
	self->m_encoding = EncodedString::Encoding_Utf8;
}

static inline void EncSetWString(EncodedString *self, const wstring &src, int encoding)
{
	ScePafMisc_AF142E26(self);
	if (encoding == EncodedString::Encoding_Utf8) {
		self->m_string = ScePafMisc_E690F751(src.c_str(), self->m_string);
		self->m_encoding = EncodedString::Encoding_Utf8;
	} else {
		if (self->m_wstring != NULL)
			self->m_wstring->assign(src);
		else
			self->m_wstring = new wstring(src);
		self->m_encoding = EncodedString::Encoding_Utf16;
	}
}

static inline const wchar_t *EncGetUtf16(EncodedString *self)
{
	if (self->m_wstring == NULL) {
		if (self->m_string == NULL)
			return NULL;
		self->m_wstring = ScePafMisc_A6ABC0B2(self->m_string->c_str(), NULL);
		if (self->m_wstring == NULL)
			return NULL;
	}
	return self->m_wstring->c_str();
}

static inline size_t EncGetUtf16Length(EncodedString *self)
{
	if (self->m_wstring == NULL) {
		if (self->m_string == NULL)
			return 0;
		self->m_wstring = ScePafMisc_A6ABC0B2(self->m_string->c_str(), NULL);
		if (self->m_wstring == NULL)
			return 0;
	}
	return self->m_wstring->size();
}

static inline const char *EncGetUtf8(EncodedString *self)
{
	if (self->m_string == NULL) {
		if (self->m_wstring == NULL)
			return NULL;
		self->m_string = ScePafMisc_E690F751(self->m_wstring->c_str(), NULL);
		if (self->m_string == NULL)
			return NULL;
	}
	return self->m_string->c_str();
}

static inline size_t EncGetUtf8Length(EncodedString *self)
{
	if (self->m_string == NULL) {
		if (self->m_wstring == NULL)
			return 0;
		self->m_string = ScePafMisc_E690F751(self->m_wstring->c_str(), NULL);
		if (self->m_string == NULL)
			return 0;
	}
	return self->m_string->size();
}

extern "C" EncodedString *ScePafMisc_D1E648EF(EncodedString *self)
{
	self->m_encoding = EncodedString::Encoding_None;
	self->m_wstring = NULL;
	self->m_string = NULL;
	return self;
}

extern "C" EncodedString *ScePafMisc_A857C48F(EncodedString *self)
{
	self->m_encoding = EncodedString::Encoding_None;
	self->m_wstring = NULL;
	self->m_string = NULL;
	return self;
}

/* UTF-8 to UTF-16, into dst or a new string; an invalid source converts to nothing. */
extern "C" wstring *ScePafMisc_A6ABC0B2(const char *src, wstring *dst)
{
	SceCesUcsContext context;
	uint32_t src_len, dst_len;
	size_t len = 0;

	sceCesUcsContextInit(&context);
	if (sceCesUtf8StrGetUtf16Len(&context, (const uint8_t *)src, 0, &src_len, &dst_len) == 0)
		len = dst_len;
	if (dst != NULL)
		dst->resize(len);
	else
		dst = new wstring(len, 0);
	if (len != 0)
		sceCesUtf8StrToUtf16Str(&context, (const uint8_t *)src, 0, &src_len, (uint16_t *)dst->ptr,
					len + 1, &dst_len);
	return dst;
}

extern "C" EncodedString *ScePafMisc_AF142E26(EncodedString *self)
{
	self->m_encoding = EncodedString::Encoding_None;
	delete self->m_string;
	delete self->m_wstring;
	self->m_string = NULL;
	self->m_wstring = NULL;
	return self;
}

extern "C" EncodedString *ScePafMisc_86181366(EncodedString *self, const char *src, int encoding)
{
	ScePafMisc_AF142E26(self);
	if (encoding == EncodedString::Encoding_Utf16) {
		if (src != NULL) {
			self->m_wstring = ScePafMisc_A6ABC0B2(src, self->m_wstring);
			self->m_encoding = EncodedString::Encoding_Utf16;
		}
	} else {
		if (self->m_string != NULL)
			self->m_string->assign(src);
		else
			self->m_string = new string(src);
		self->m_encoding = EncodedString::Encoding_Utf8;
	}
	return self;
}

extern "C" EncodedString *ScePafMisc_C11CB5E4(EncodedString *self, const char *src, int encoding)
{
	self->m_encoding = EncodedString::Encoding_None;
	self->m_string = NULL;
	self->m_wstring = NULL;
	ScePafMisc_86181366(self, src, encoding);
	return self;
}

extern "C" EncodedString *ScePafMisc_E2506D74(EncodedString *self, const char *src, int encoding)
{
	self->m_encoding = EncodedString::Encoding_None;
	self->m_string = NULL;
	self->m_wstring = NULL;
	ScePafMisc_86181366(self, src, encoding);
	return self;
}

extern "C" EncodedString *ScePafMisc_E13019F7(EncodedString *self, const char *src, int encoding)
{
	self->m_encoding = EncodedString::Encoding_None;
	self->m_string = NULL;
	self->m_wstring = NULL;
	ScePafMisc_86181366(self, src, encoding);
	return self;
}

extern "C" EncodedString *ScePafMisc_2F0BF4FF(EncodedString *self, const char *src, int encoding)
{
	self->m_encoding = EncodedString::Encoding_None;
	self->m_string = NULL;
	self->m_wstring = NULL;
	ScePafMisc_86181366(self, src, encoding);
	return self;
}

/* UTF-16 to UTF-8, into dst or a new string; an invalid source converts to nothing. */
extern "C" string *ScePafMisc_E690F751(const wchar_t *src, string *dst)
{
	SceCesUcsContext context;
	uint32_t src_len, dst_len;
	size_t len = 0;

	sceCesUcsContextInit(&context);
	if (sceCesUtf16StrGetUtf8Len(&context, (const uint16_t *)src, 0, &src_len, &dst_len) == 0)
		len = dst_len;
	if (dst != NULL)
		dst->resize(len);
	else
		dst = new string(len, 0);
	if (len != 0)
		sceCesUtf16StrToUtf8Str(&context, (const uint16_t *)src, 0, &src_len, (uint8_t *)dst->ptr,
					len + 1, &dst_len);
	return dst;
}

extern "C" EncodedString *ScePafMisc_6DBCED34(EncodedString *self, const wchar_t *src, int encoding)
{
	ScePafMisc_AF142E26(self);
	if (encoding == EncodedString::Encoding_Utf8) {
		if (src != NULL) {
			self->m_string = ScePafMisc_E690F751(src, self->m_string);
			self->m_encoding = EncodedString::Encoding_Utf8;
		}
	} else {
		if (self->m_wstring != NULL)
			self->m_wstring->assign(src);
		else
			self->m_wstring = new wstring(src);
		self->m_encoding = EncodedString::Encoding_Utf16;
	}
	return self;
}

extern "C" EncodedString *ScePafMisc_DCBA2BD4(EncodedString *self, const wchar_t *src, int encoding)
{
	self->m_encoding = EncodedString::Encoding_None;
	self->m_string = NULL;
	self->m_wstring = NULL;
	ScePafMisc_6DBCED34(self, src, encoding);
	return self;
}

extern "C" EncodedString *ScePafMisc_E07B9A18(EncodedString *self, const wchar_t *src, int encoding)
{
	self->m_encoding = EncodedString::Encoding_None;
	self->m_string = NULL;
	self->m_wstring = NULL;
	ScePafMisc_6DBCED34(self, src, encoding);
	return self;
}

extern "C" wstring *ScePafMisc_5B9FC644(const string &src, wstring *dst)
{
	return ScePafMisc_A6ABC0B2(src.c_str(), dst);
}

/* The encoding is left as UTF-8 whichever form was asked for. */
extern "C" EncodedString *ScePafMisc_E75B0F78(EncodedString *self, const string &src, int encoding)
{
	EncSetString(self, src, encoding);
	return self;
}

extern "C" EncodedString *ScePafMisc_09BAD2FF(EncodedString *self, const string &src, int encoding)
{
	self->m_encoding = EncodedString::Encoding_None;
	self->m_string = NULL;
	self->m_wstring = NULL;
	EncSetString(self, src, encoding);
	return self;
}

extern "C" EncodedString *ScePafMisc_069F8E86(EncodedString *self, const string &src, int encoding)
{
	self->m_encoding = EncodedString::Encoding_None;
	self->m_string = NULL;
	self->m_wstring = NULL;
	EncSetString(self, src, encoding);
	return self;
}

extern "C" string *ScePafMisc_25C129DF(const wstring &src, string *dst)
{
	return ScePafMisc_E690F751(src.c_str(), dst);
}

extern "C" EncodedString *ScePafMisc_EA362AC1(EncodedString *self, const wstring &src, int encoding)
{
	EncSetWString(self, src, encoding);
	return self;
}

extern "C" EncodedString *ScePafMisc_7709D1B2(EncodedString *self, const wstring &src, int encoding)
{
	self->m_encoding = EncodedString::Encoding_None;
	self->m_string = NULL;
	self->m_wstring = NULL;
	EncSetWString(self, src, encoding);
	return self;
}

extern "C" EncodedString *ScePafMisc_51BA74C0(EncodedString *self, const wstring &src, int encoding)
{
	self->m_encoding = EncodedString::Encoding_None;
	self->m_string = NULL;
	self->m_wstring = NULL;
	EncSetWString(self, src, encoding);
	return self;
}

/* Encoding_None keeps the form src was last set in, else the one src has. */
extern "C" EncodedString *ScePafMisc_351C9F8E(EncodedString *self, const EncodedString &src,
					     int encoding)
{
	ScePafMisc_AF142E26(self);
	switch (encoding) {
	case EncodedString::Encoding_Utf8:
		if (src.m_string != NULL) {
			EncSetString(self, *src.m_string, EncodedString::Encoding_Utf8);
		} else if (src.m_wstring != NULL) {
			self->m_string = ScePafMisc_E690F751(src.m_wstring->c_str(), self->m_string);
			self->m_encoding = EncodedString::Encoding_Utf8;
		}
		break;
	case EncodedString::Encoding_Utf16:
		if (src.m_wstring != NULL) {
			EncSetWString(self, *src.m_wstring, EncodedString::Encoding_Utf16);
		} else if (src.m_string != NULL) {
			self->m_wstring = ScePafMisc_A6ABC0B2(src.m_string->c_str(), self->m_wstring);
			self->m_encoding = EncodedString::Encoding_Utf16;
		}
		break;
	default:
		if (src.m_encoding == EncodedString::Encoding_Utf16) {
			if (src.m_wstring != NULL)
				EncSetWString(self, *src.m_wstring, EncodedString::Encoding_Utf16);
			else if (src.m_string != NULL)
				EncSetString(self, *src.m_string, EncodedString::Encoding_Utf8);
		} else {
			if (src.m_string != NULL)
				EncSetString(self, *src.m_string, EncodedString::Encoding_Utf8);
			else if (src.m_wstring != NULL)
				EncSetWString(self, *src.m_wstring, EncodedString::Encoding_Utf16);
		}
		break;
	}
	return self;
}

extern "C" EncodedString *ScePafMisc_6A59DFB6(EncodedString *self, const EncodedString &src,
					     int encoding)
{
	self->m_encoding = EncodedString::Encoding_None;
	self->m_string = NULL;
	self->m_wstring = NULL;
	ScePafMisc_351C9F8E(self, src, encoding);
	return self;
}

extern "C" EncodedString *ScePafMisc_543FDEEF(EncodedString *self, const EncodedString &src,
					     int encoding)
{
	self->m_encoding = EncodedString::Encoding_None;
	self->m_string = NULL;
	self->m_wstring = NULL;
	ScePafMisc_351C9F8E(self, src, encoding);
	return self;
}

extern "C" EncodedString *ScePafMisc_3C879D5F(EncodedString *self, const char *src, int encoding)
{
	return ScePafMisc_86181366(self, src, encoding);
}

extern "C" EncodedString *ScePafMisc_B45A7521(EncodedString *self)
{
	ScePafMisc_AF142E26(self);
	return self;
}

extern "C" size_t ScePafMisc_183DD5C3(EncodedString *self)
{
	return EncGetUtf16Length(self);
}

extern "C" const wchar_t *ScePafMisc_53948F03(EncodedString *self)
{
	return EncGetUtf16(self);
}

extern "C" const char *ScePafMisc_A7C6AF5C(EncodedString *self)
{
	return EncGetUtf8(self);
}

/* Keeps only the form of encoding, making it first if needed; Encoding_None clears. */
extern "C" void ScePafMisc_BDFDD0D9(EncodedString *self, int encoding)
{
	switch (encoding) {
	case EncodedString::Encoding_Utf8:
		if (self->m_string == NULL) {
			if (self->m_wstring == NULL)
				return;
			self->m_string = ScePafMisc_E690F751(self->m_wstring->c_str(), NULL);
		}
		if (self->m_wstring != NULL) {
			delete self->m_wstring;
			self->m_wstring = NULL;
		}
		break;
	case EncodedString::Encoding_Utf16:
		if (self->m_wstring == NULL) {
			if (self->m_string == NULL)
				return;
			self->m_wstring = ScePafMisc_A6ABC0B2(self->m_string->c_str(), NULL);
		}
		if (self->m_string != NULL) {
			delete self->m_string;
			self->m_string = NULL;
		}
		break;
	default:
		ScePafMisc_AF142E26(self);
		break;
	}
}

extern "C" EncodedString *ScePafMisc_9BA007B9(EncodedString *self, const char *src)
{
	if (src == NULL)
		return self;
	ScePafMisc_BDFDD0D9(self, self->m_encoding);
	switch (self->m_encoding) {
	case EncodedString::Encoding_None:
		return ScePafMisc_86181366(self, src, EncodedString::Encoding_Utf8);
	case EncodedString::Encoding_Utf16: {
		EncodedString converted;

		converted.m_encoding = EncodedString::Encoding_None;
		converted.m_string = NULL;
		converted.m_wstring = NULL;
		ScePafMisc_86181366(&converted, src, EncodedString::Encoding_Utf8);
		ScePafMisc_BDFDD0D9(&converted, self->m_encoding);
		self->m_wstring->append(EncGetUtf16(&converted), EncGetUtf16Length(&converted));
		ScePafMisc_AF142E26(&converted);
		break;
	}
	default:
		self->m_string->append(src);
		break;
	}
	return self;
}

extern "C" EncodedString *ScePafMisc_0955141F(EncodedString *self, const char *src)
{
	return ScePafMisc_9BA007B9(self, src);
}

extern "C" size_t ScePafMisc_40753A45(EncodedString *self)
{
	return EncGetUtf8Length(self);
}

extern "C" EncodedString *ScePafMisc_A32583BE(EncodedString *self, const wchar_t *src)
{
	if (src == NULL)
		return self;
	ScePafMisc_BDFDD0D9(self, self->m_encoding);
	switch (self->m_encoding) {
	case EncodedString::Encoding_None:
		return ScePafMisc_6DBCED34(self, src, EncodedString::Encoding_Utf16);
	case EncodedString::Encoding_Utf8: {
		EncodedString converted;

		converted.m_encoding = EncodedString::Encoding_None;
		converted.m_string = NULL;
		converted.m_wstring = NULL;
		ScePafMisc_6DBCED34(&converted, src, EncodedString::Encoding_Utf16);
		ScePafMisc_BDFDD0D9(&converted, self->m_encoding);
		self->m_string->append(EncGetUtf8(&converted), EncGetUtf8Length(&converted));
		ScePafMisc_AF142E26(&converted);
		break;
	}
	default:
		self->m_wstring->append(src);
		break;
	}
	return self;
}

extern "C" EncodedString *ScePafMisc_292D117C(EncodedString *self, const string &src)
{
	if (src.size() == 0)
		return self;
	ScePafMisc_BDFDD0D9(self, self->m_encoding);
	switch (self->m_encoding) {
	case EncodedString::Encoding_None:
		EncSetString(self, src, EncodedString::Encoding_Utf8);
		break;
	case EncodedString::Encoding_Utf16: {
		EncodedString converted;

		converted.m_encoding = EncodedString::Encoding_None;
		converted.m_string = NULL;
		converted.m_wstring = NULL;
		EncSetString(&converted, src, EncodedString::Encoding_Utf8);
		ScePafMisc_BDFDD0D9(&converted, self->m_encoding);
		self->m_wstring->append(EncGetUtf16(&converted), EncGetUtf16Length(&converted));
		ScePafMisc_AF142E26(&converted);
		break;
	}
	default:
		self->m_string->append(src);
		break;
	}
	return self;
}

extern "C" EncodedString *ScePafMisc_B616002F(EncodedString *self, const wstring &src)
{
	if (src.size() == 0)
		return self;
	ScePafMisc_BDFDD0D9(self, self->m_encoding);
	switch (self->m_encoding) {
	case EncodedString::Encoding_None:
		EncSetWString(self, src, EncodedString::Encoding_Utf16);
		break;
	case EncodedString::Encoding_Utf8: {
		EncodedString converted;

		converted.m_encoding = EncodedString::Encoding_None;
		converted.m_string = NULL;
		converted.m_wstring = NULL;
		EncSetWString(&converted, src, EncodedString::Encoding_Utf16);
		ScePafMisc_BDFDD0D9(&converted, self->m_encoding);
		self->m_string->append(EncGetUtf8(&converted), EncGetUtf8Length(&converted));
		ScePafMisc_AF142E26(&converted);
		break;
	}
	default:
		self->m_wstring->append(src);
		break;
	}
	return self;
}

extern "C" EncodedString *ScePafMisc_E7E54E04(EncodedString *self, const EncodedString &src)
{
	if (src.m_encoding == EncodedString::Encoding_None)
		return self;
	ScePafMisc_BDFDD0D9(self, self->m_encoding);
	switch (self->m_encoding) {
	case EncodedString::Encoding_None:
		return ScePafMisc_351C9F8E(self, src, EncodedString::Encoding_None);
	case EncodedString::Encoding_Utf8:
		if (src.m_string != NULL) {
			self->m_string->append(*src.m_string);
		} else if (src.m_wstring != NULL) {
			string converted;

			ScePafMisc_E690F751(src.m_wstring->c_str(), &converted);
			self->m_string->append(converted);
		}
		break;
	default:
		if (src.m_wstring != NULL) {
			self->m_wstring->append(*src.m_wstring);
		} else {
			wstring converted;

			ScePafMisc_A6ABC0B2(src.m_string->c_str(), &converted);
			self->m_wstring->append(converted);
		}
		break;
	}
	return self;
}

extern "C" string ScePafMisc_F20BB6B1(const EncodedString *self)
{
	if (self->m_string != NULL)
		return *self->m_string;

	string converted;

	ScePafMisc_E690F751(self->m_wstring->c_str(), &converted);
	return converted;
}

extern "C" wstring ScePafMisc_1BE491D4(const EncodedString *self)
{
	if (self->m_wstring != NULL)
		return *self->m_wstring;

	wstring converted;

	ScePafMisc_A6ABC0B2(self->m_string->c_str(), &converted);
	return converted;
}

extern "C" bool ScePafMisc_98BF4D32(EncodedString *self, EncodedString &other)
{
	const wchar_t *left = EncGetUtf16(self);
	const wchar_t *right = EncGetUtf16(&other);
	size_t len = EncGetUtf16Length(self);

	if (len != EncGetUtf16Length(&other))
		return false;
	if (len != 0)
		return sce_paf_wcscmp(left, right) == 0;
	return true;
}

extern "C" bool ScePafMisc_27AD8159(EncodedString *self, const wstring &other)
{
	const wchar_t *left = EncGetUtf16(self);
	const wchar_t *right = other.c_str();
	size_t len = EncGetUtf16Length(self);

	if (len != other.size())
		return false;
	if (len != 0)
		return sce_paf_wcscmp(left, right) == 0;
	return true;
}

extern "C" bool ScePafMisc_6F11BD71(EncodedString *self, const wchar_t *other)
{
	const wchar_t *left = EncGetUtf16(self);

	if (EncGetUtf16Length(self) != 0)
		return sce_paf_wcscmp(left, other) == 0;
	return *other == 0;
}

extern "C" bool ScePafMisc_F4176C33(EncodedString *self, const char *other)
{
	const char *left = EncGetUtf8(self);

	if (EncGetUtf8Length(self) != 0)
		return sce_paf_strcmp(left, other) == 0;
	return *other == 0;
}

/* The UTF-8 form, made empty when there is no string at all. */
extern "C" string *ScePafMisc_51E1E267(EncodedString *self)
{
	if (self->m_string == NULL) {
		if (self->m_wstring == NULL) {
			ScePafMisc_86181366(self, "", EncodedString::Encoding_Utf8);
			return self->m_string;
		}
		self->m_string = ScePafMisc_E690F751(self->m_wstring->c_str(), NULL);
	}
	return self->m_string;
}

extern "C" wstring *ScePafMisc_B672E71C(EncodedString *self)
{
	if (self->m_wstring == NULL) {
		if (self->m_string == NULL) {
			ScePafMisc_6DBCED34(self, L"", EncodedString::Encoding_Utf16);
			return self->m_wstring;
		}
		self->m_wstring = ScePafMisc_A6ABC0B2(self->m_string->c_str(), NULL);
	}
	return self->m_wstring;
}

extern "C" EncodedString *ScePafMisc_B9C99123(EncodedString *self)
{
	ScePafMisc_AF142E26(self);
	return self;
}
