/*
 * SceMsgDialogPlugin's common functions: the check of the caller's SDK version, the size and
 * %1..%11 formatters, and the countries' number formats. Sony linked the same code into safemode
 * and most of the system software's applications; no Sony record names the file or its
 * functions (the names are ours).
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <modulemgr/modulemgr_syscall.h>
#include <std/string.h>
#include <std/string>
#include "msg_dialog_plugin.h"

using namespace paf;

/* TODO(Tamsin): declared by vsh/paf/include/std/stdio.h once it has it */
extern "C" int sce_paf_swprintf(wchar_t *buffer, size_t bufsz, const wchar_t *format, ...);

/* the SDK versions from which the caller sees the newer dialogs: 1.500 and 2.100 */
#define MSG_DIALOG_SDK_VERSION_LEVEL1	0x01500000
#define MSG_DIALOG_SDK_VERSION_LEVEL2	0x02100000

/* the largest sizes FormatSize counts in KB and in MB (KiB) */
#define MSG_DIALOG_SIZE_KB_MAX		9999
#define MSG_DIALOG_SIZE_MB_MAX		(9999 * 1024)

/* a country's number format: the system language it is for, its decimal point and the separator
   of its thousands */
struct MsgDialogNumberFormat {
	string country;
	SceInt32 language;
	string decimalPoint;
	string groupSeparator;
};

static SceUInt32 s_sdkVersion;
static SceUInt32 s_allowedSdkVersion;
static int s_sdkLevel;

static MsgDialogNumberFormat s_numberFormats[] = {
	{ "JP", 0, ".", "," },
	{ "US", 1, ".", "," },
	{ "CA", 1, ".", "," },
	{ "CA", 2, ",", " " },
	{ "MX", 3, ".", "," },
	{ "MX", 1, ".", "," },
	{ "BR", 17, ",", "." },
	{ "BR", 1, ".", "," },
	{ "CL", 3, ",", " " },
	{ "CL", 1, ".", "," },
	{ "PE", 3, ",", " " },
	{ "PE", 1, ".", "," },
	{ "AR", 3, ",", " " },
	{ "AR", 1, ".", "," },
	{ "CO", 3, ",", " " },
	{ "CO", 1, ".", "," },
	{ "GB", 18, ".", "," },
	{ "IE", 18, ".", "," },
	{ "BE", 6, ",", " " },
	{ "BE", 2, ",", " " },
	{ "LU", 4, ",", "." },
	{ "LU", 2, ",", " " },
	{ "NL", 6, ",", "." },
	{ "FR", 2, ",", " " },
	{ "DE", 4, ",", "." },
	{ "AT", 4, ",", "." },
	{ "CH", 4, ",", " " },
	{ "CH", 2, ",", " " },
	{ "CH", 5, ",", " " },
	{ "IT", 5, ",", "." },
	{ "PT", 7, ",", "." },
	{ "DK", 14, ",", " " },
	{ "DK", 18, ".", "," },
	{ "FI", 12, ",", " " },
	{ "FI", 18, ".", "," },
	{ "NO", 15, ",", " " },
	{ "NO", 18, ".", "," },
	{ "SE", 13, ",", " " },
	{ "SE", 18, ".", "," },
	{ "AU", 18, ".", "," },
	{ "NZ", 18, ".", "," },
	{ "ES", 3, ",", "." },
	{ "RU", 8, ",", " " },
	{ "AE", 18, ".", "," },
	{ "ZA", 18, ".", "," },
	{ "PL", 16, ",", " " },
	{ "PL", 18, ".", "," },
	{ "GR", 18, ".", "," },
	{ "SA", 18, ".", "," },
	{ "CZ", 18, ".", "," },
	{ "TR", 18, ".", "," },
	{ "TR", 19, ",", "." },
	{ "IN", 18, ".", "," },
	{ "HR", 18, ".", "," },
	{ "SI", 18, ".", "," },
	{ "KW", 18, ".", "," },
	{ "IL", 18, ".", "," },
	{ "UA", 8, ",", " " },
	{ "BG", 18, ".", "," },
	{ "RO", 18, ".", "," },
	{ "HU", 18, ".", "," },
	{ "QA", 18, ".", "," },
	{ "SK", 18, ".", "," },
	{ "BH", 18, ".", "," },
	{ "LB", 18, ".", "," },
	{ "OM", 18, ".", "," },
	{ "MT", 18, ".", "," },
	{ "IS", 18, ".", "," },
	{ "CY", 18, ".", "," },
	{ "HK", 10, ".", "," },
	{ "HK", 18, ".", "," },
	{ "HK", 11, ".", "," },
	{ "TW", 10, ".", "," },
	{ "TW", 18, ".", "," },
	{ "SG", 18, ".", "," },
	{ "MY", 18, ".", "," },
	{ "ID", 18, ".", "," },
	{ "TH", 18, ".", "," },
	{ "KR", 9, ".", "," },
};

/* what Format puts for %A..%Z and %a..%z: nothing while it is not set */
const wchar_t *(*g_msgDialogFormatLetter)(wchar_t letter);

/* checks the caller's SDK version against the system's, and keeps its level */
int MsgDialogCheckSdkVersion(SceUInt32 sdkVersion)
{
	SceUInt32 allowed;
	int level;

	if (s_sdkVersion == 0) {
		s_sdkVersion = SceLibKernel_9F793F84();
		if (s_sdkVersion == 0)
			return MSG_DIALOG_PLUGIN_ERROR_NO_SDK_VERSION;
	}
	allowed = s_allowedSdkVersion;
	if (allowed == 0) {
		allowed = sceKernelGetAllowedSdkVersionOnSystem();
		s_allowedSdkVersion = allowed;
		if (allowed == 0)
			return MSG_DIALOG_PLUGIN_ERROR_NO_ALLOWED_SDK_VERSION;
	}
	if (allowed < sdkVersion)
		return MSG_DIALOG_PLUGIN_ERROR_SDK_VERSION_TOO_NEW;

	level = 0;
	if (sdkVersion >= MSG_DIALOG_SDK_VERSION_LEVEL1) {
		level = 2;
		if (sdkVersion < MSG_DIALOG_SDK_VERSION_LEVEL2)
			level = 1;
	}
	s_sdkLevel = level;
	return SCE_OK;
}

int MsgDialogGetSdkLevel(void)
{
	return s_sdkLevel;
}

/* a size in KiB as "%d KB", "%d MB" or "%d GB" */
void MsgDialogFormatSize(wstring *out, SceUInt32 sizeKiB)
{
	wchar_t buf[16];

	if (sizeKiB <= MSG_DIALOG_SIZE_KB_MAX)
		sce_paf_swprintf(buf, 16, L"%d KB", sizeKiB);
	else if (sizeKiB <= MSG_DIALOG_SIZE_MB_MAX)
		sce_paf_swprintf(buf, 16, L"%d MB", sizeKiB >> 10);
	else
		sce_paf_swprintf(buf, 16, L"%d GB", sizeKiB >> 20);
	out->assign(buf, sce_paf_wcslen(buf));
}

/*
 * The format with %1..%9, %(10) and %(11) replaced by the arguments, %% by %, and a letter by
 * what g_msgDialogFormatLetter gives for it; a % at the end stops it.
 */
wstring MsgDialogFormat(const wchar_t *format, const wchar_t *arg1, const wchar_t *arg2,
			const wchar_t *arg3, const wchar_t *arg4, const wchar_t *arg5,
			const wchar_t *arg6, const wchar_t *arg7, const wchar_t *arg8,
			const wchar_t *arg9, const wchar_t *arg10, const wchar_t *arg11)
{
	const wchar_t *args[11] = { arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10,
				    arg11 };
	wstring out;
	const wchar_t *p = format;
	const wchar_t *percent;
	const wchar_t *arg;
	int count = 0;

	while ((percent = sce_paf_wcschr(p, L'%')) != NULL) {
		wchar_t c;

		out.append(p, percent - p);
		c = percent[1];
		if (c == L'%') {
			out.append(1, L'%');
		} else if (c >= L'1' && c <= L'9') {
			arg = args[c - L'1'];
			out.append(arg, sce_paf_wcslen(arg));
		} else if ((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z')) {
			if (g_msgDialogFormatLetter != NULL) {
				arg = g_msgDialogFormatLetter(c);
				out.append(arg, sce_paf_wcslen(arg));
			}
		} else if (c == L'(') {
			if (percent[2] == L'1' && percent[3] >= L'0' && percent[3] <= L'1' &&
			    percent[4] == L')') {
				arg = args[percent[3] - L'0' + 9];
				out.append(arg, sce_paf_wcslen(arg));
				p = percent + 5;
				count++;
				continue;
			}
		} else if (c == L'\0') {
			return out;
		}
		p = percent + 2;
		count++;
	}
	if (count != 0)
		out.append(p, sce_paf_wcslen(p));
	else
		out.assign(format);
	return out;
}
