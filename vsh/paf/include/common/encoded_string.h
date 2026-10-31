/*
 * Two strings that keep a narrow and a UTF-16 form, each made from the other
 * when it is first asked for. No Sony name for either class is known (their
 * ScePafMisc exports hash from no candidate spelling), so the names are ours
 * and the exports keep their NID link names.
 *
 * SjisString: Shift_JIS (CP932) and UTF-16.
 * EncodedString: UTF-8 and UTF-16, with the encoding it was last set in.
 */
#ifndef PAF_COMMON_ENCODED_STRING_H
#define PAF_COMMON_ENCODED_STRING_H

#include <stddef.h>
#include <stdint.h>
#include <std/string>

namespace paf {
namespace common {

class EncodedString;

class SjisString {
public:
	string *m_sjis;
	wstring *m_wstring;
};

class EncodedString {
public:
	enum Encoding {
		Encoding_None = 0,
		Encoding_Utf8 = 1,
		Encoding_Utf16 = 2
	};

	int32_t m_encoding;
	string *m_string;
	wstring *m_wstring;
};

}
}

extern "C" {

/* SjisString */
paf::common::SjisString *ScePafMisc_DF4C99BA(paf::common::SjisString *self);
paf::common::SjisString *ScePafMisc_68A6B37B(paf::common::SjisString *self);
paf::common::SjisString *ScePafMisc_FD631CC2(paf::common::SjisString *self, const char *sjis);
paf::common::SjisString *ScePafMisc_CE859024(paf::common::SjisString *self, const char *sjis);
paf::common::SjisString *ScePafMisc_AB04B8B7(paf::common::SjisString *self, const char *sjis);
paf::common::SjisString *ScePafMisc_EEBED3C6(paf::common::SjisString *self, const char *sjis);
paf::common::SjisString *ScePafMisc_49706ED5(paf::common::SjisString *self, const paf::string &sjis);
paf::common::SjisString *ScePafMisc_749CA742(paf::common::SjisString *self, const paf::string &sjis);
paf::string *ScePafMisc_BE18253B(const wchar_t *src, paf::string *dst);
paf::string *ScePafMisc_3BC42454(const paf::wstring &src, paf::string *dst);
paf::common::SjisString *ScePafMisc_8FDF9295(paf::common::SjisString *self);
paf::common::SjisString *ScePafMisc_15A2CCBE(paf::common::SjisString *self, const wchar_t *src);
paf::common::SjisString *ScePafMisc_4638FBA1(paf::common::SjisString *self, const wchar_t *src);
paf::common::SjisString *ScePafMisc_0ED06370(paf::common::SjisString *self, const wchar_t *src);
paf::common::SjisString *ScePafMisc_E3DE6624(paf::common::SjisString *self, const paf::wstring &src);
paf::common::SjisString *ScePafMisc_376DFF8C(paf::common::SjisString *self, const paf::wstring &src);
paf::common::SjisString *ScePafMisc_3BE6AFE6(paf::common::SjisString *self, const paf::wstring &src);
paf::common::SjisString *ScePafMisc_C1DE0AC2(paf::common::SjisString *self, const paf::string &sjis);
paf::common::SjisString *ScePafMisc_EF52733C(paf::common::SjisString *self,
					     const paf::common::SjisString &src);
paf::common::SjisString *ScePafMisc_36CF059C(paf::common::SjisString *self,
					     const paf::common::SjisString &src);
paf::common::SjisString *ScePafMisc_1B896933(paf::common::SjisString *self,
					     const paf::common::SjisString &src);
paf::common::SjisString *ScePafMisc_ACE64B1E(paf::common::SjisString *self,
					     paf::common::EncodedString &src);
paf::common::SjisString *ScePafMisc_928EBAC4(paf::common::SjisString *self,
					     paf::common::EncodedString &src);
paf::common::SjisString *ScePafMisc_5BAA3517(paf::common::SjisString *self, const char *sjis);
paf::common::SjisString *ScePafMisc_AD5E57B0(paf::common::SjisString *self, const char *sjis);
paf::common::SjisString *ScePafMisc_6DA00D89(paf::common::SjisString *self, const char *sjis);
paf::common::SjisString *ScePafMisc_0587A58E(paf::common::SjisString *self, const char *sjis);
paf::common::SjisString *ScePafMisc_2D5AD1EF(paf::common::SjisString *self, const paf::string &sjis);
paf::common::SjisString *ScePafMisc_AD3D3047(paf::common::SjisString *self);
paf::common::SjisString *ScePafMisc_4CC6F401(paf::common::SjisString *self, const wchar_t *src);
paf::common::SjisString *ScePafMisc_4CD8E682(paf::common::SjisString *self, const paf::wstring &src);
paf::common::SjisString *ScePafMisc_2678FFEB(paf::common::SjisString *self,
					     const paf::common::SjisString &src);
paf::wstring *ScePafMisc_D27C91C0(const char *src, paf::wstring *dst);
paf::wstring *ScePafMisc_F080858F(const paf::string &src, paf::wstring *dst);
const char *ScePafMisc_8193E3E1(paf::common::SjisString *self);
const wchar_t *ScePafMisc_C3E27C0A(paf::common::SjisString *self);
size_t ScePafMisc_EDE4297A(paf::common::SjisString *self);
size_t ScePafMisc_C160806C(const paf::common::SjisString *self);
paf::string ScePafMisc_A05DD777(const paf::common::SjisString *self);
paf::wstring ScePafMisc_F77AD7C3(const paf::common::SjisString *self);
bool ScePafMisc_5C6CCB4A(paf::common::SjisString *self, paf::common::SjisString &other);
bool ScePafMisc_10271650(paf::common::SjisString *self, paf::common::EncodedString &other);
bool ScePafMisc_41A5D96E(paf::common::SjisString *self, const paf::wstring &other);
bool ScePafMisc_B15E3890(paf::common::SjisString *self, const wchar_t *other);
paf::common::SjisString *ScePafMisc_4B6BCFAF(paf::common::SjisString *self);

/* EncodedString */
paf::common::EncodedString *ScePafMisc_D1E648EF(paf::common::EncodedString *self);
paf::common::EncodedString *ScePafMisc_A857C48F(paf::common::EncodedString *self);
paf::wstring *ScePafMisc_A6ABC0B2(const char *src, paf::wstring *dst);
paf::common::EncodedString *ScePafMisc_AF142E26(paf::common::EncodedString *self);
paf::common::EncodedString *ScePafMisc_86181366(paf::common::EncodedString *self, const char *src,
						int encoding);
paf::common::EncodedString *ScePafMisc_C11CB5E4(paf::common::EncodedString *self, const char *src,
						int encoding);
paf::common::EncodedString *ScePafMisc_E2506D74(paf::common::EncodedString *self, const char *src,
						int encoding);
paf::common::EncodedString *ScePafMisc_E13019F7(paf::common::EncodedString *self, const char *src,
						int encoding);
paf::common::EncodedString *ScePafMisc_2F0BF4FF(paf::common::EncodedString *self, const char *src,
						int encoding);
paf::string *ScePafMisc_E690F751(const wchar_t *src, paf::string *dst);
paf::common::EncodedString *ScePafMisc_6DBCED34(paf::common::EncodedString *self, const wchar_t *src,
						int encoding);
paf::common::EncodedString *ScePafMisc_DCBA2BD4(paf::common::EncodedString *self, const wchar_t *src,
						int encoding);
paf::common::EncodedString *ScePafMisc_E07B9A18(paf::common::EncodedString *self, const wchar_t *src,
						int encoding);
paf::wstring *ScePafMisc_5B9FC644(const paf::string &src, paf::wstring *dst);
paf::common::EncodedString *ScePafMisc_E75B0F78(paf::common::EncodedString *self,
						const paf::string &src, int encoding);
paf::common::EncodedString *ScePafMisc_09BAD2FF(paf::common::EncodedString *self,
						const paf::string &src, int encoding);
paf::common::EncodedString *ScePafMisc_069F8E86(paf::common::EncodedString *self,
						const paf::string &src, int encoding);
paf::string *ScePafMisc_25C129DF(const paf::wstring &src, paf::string *dst);
paf::common::EncodedString *ScePafMisc_EA362AC1(paf::common::EncodedString *self,
						const paf::wstring &src, int encoding);
paf::common::EncodedString *ScePafMisc_7709D1B2(paf::common::EncodedString *self,
						const paf::wstring &src, int encoding);
paf::common::EncodedString *ScePafMisc_51BA74C0(paf::common::EncodedString *self,
						const paf::wstring &src, int encoding);
paf::common::EncodedString *ScePafMisc_351C9F8E(paf::common::EncodedString *self,
						const paf::common::EncodedString &src,
						int encoding);
paf::common::EncodedString *ScePafMisc_6A59DFB6(paf::common::EncodedString *self,
						const paf::common::EncodedString &src,
						int encoding);
paf::common::EncodedString *ScePafMisc_543FDEEF(paf::common::EncodedString *self,
						const paf::common::EncodedString &src,
						int encoding);
paf::common::EncodedString *ScePafMisc_3C879D5F(paf::common::EncodedString *self, const char *src,
						int encoding);
paf::common::EncodedString *ScePafMisc_B45A7521(paf::common::EncodedString *self);
size_t ScePafMisc_183DD5C3(paf::common::EncodedString *self);
const wchar_t *ScePafMisc_53948F03(paf::common::EncodedString *self);
const char *ScePafMisc_A7C6AF5C(paf::common::EncodedString *self);
void ScePafMisc_BDFDD0D9(paf::common::EncodedString *self, int encoding);
paf::common::EncodedString *ScePafMisc_9BA007B9(paf::common::EncodedString *self, const char *src);
paf::common::EncodedString *ScePafMisc_0955141F(paf::common::EncodedString *self, const char *src);
size_t ScePafMisc_40753A45(paf::common::EncodedString *self);
paf::common::EncodedString *ScePafMisc_A32583BE(paf::common::EncodedString *self, const wchar_t *src);
paf::common::EncodedString *ScePafMisc_292D117C(paf::common::EncodedString *self,
						const paf::string &src);
paf::common::EncodedString *ScePafMisc_B616002F(paf::common::EncodedString *self,
						const paf::wstring &src);
paf::common::EncodedString *ScePafMisc_E7E54E04(paf::common::EncodedString *self,
						const paf::common::EncodedString &src);
paf::string ScePafMisc_F20BB6B1(const paf::common::EncodedString *self);
paf::wstring ScePafMisc_1BE491D4(const paf::common::EncodedString *self);
bool ScePafMisc_98BF4D32(paf::common::EncodedString *self, paf::common::EncodedString &other);
bool ScePafMisc_27AD8159(paf::common::EncodedString *self, const paf::wstring &other);
bool ScePafMisc_6F11BD71(paf::common::EncodedString *self, const wchar_t *other);
bool ScePafMisc_F4176C33(paf::common::EncodedString *self, const char *other);
paf::string *ScePafMisc_51E1E267(paf::common::EncodedString *self);
paf::wstring *ScePafMisc_B672E71C(paf::common::EncodedString *self);
paf::common::EncodedString *ScePafMisc_B9C99123(paf::common::EncodedString *self);

}

#endif
