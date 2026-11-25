/*
 * SceTwitterDialogPlugin: JSON parser producing LxVariant values.
 */
#ifndef TWITTER_DIALOG_TW_LX_JSON_H
#define TWITTER_DIALOG_TW_LX_JSON_H

#include <stdint.h>
#include <scetypes.h>
#include "../tw_login_dialog/tw_lx.h"

/* Optional streaming callback invoked when an array element is parsed */
class LxJsonArrayCallback {
public:
	virtual ~LxJsonArrayCallback() {}
	virtual SceInt32 OnArrayElement(const char *const *keyPath, SceUInt32 depth,
					paf::list<LxVariant> *arrayList,
					LxVariant *element) = 0;
};

/* Character reader state for LxJsonParser */
typedef SceInt32 (*LxJsonReadCharFn)(int *outCh, void *ctx);

struct LxJsonStream {
	LxJsonReadCharFn readChar;
	void *ctx;
	LxJsonArrayCallback *arrayCallback;
	int currentChar;
	SceInt32 lastError;
	bool hasPushedBack;
	SceUInt32 depth;
	const char *currentKey;
	const char *keyStack[16];
	LxBuffer numberBuf;
	SceSize numberLen;
	LxBuffer stringBuf;
	SceSize stringLen;
};

/* LxVariant map lookup helpers */
const LxVariant &LxVariantGetMember(const LxVariant &obj, const LxString &key);
LxVariant &LxVariantEnsureMember(LxVariant &obj, const LxString &key);

/* JSON parsing entry points */
bool LxJsonParseValue(LxVariant &outValue, LxJsonStream &stream);
bool LxJsonParseString(const char *jsonText, SceSize jsonLen, LxVariant &outValue);

#endif /* TWITTER_DIALOG_TW_LX_JSON_H */
