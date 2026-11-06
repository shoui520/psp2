/*
 * libpaf's IME client (ScePafLowlayer): the system IME of the shell's IME service
 * ("SceImeService", over IPMI) for the VSH (sceImeVsh*), its configuration (sceImeConfig*), its
 * user dictionary (sceImeDic*) and the text filter of the IME's input types (sceImeFilterText).
 * The export names are Sony's (each NID is the SHA-1 of the name and "PAF"); the parameter's
 * fields are named as the SDK's SceImeParam names them; the type, event and error names and the
 * exports no name was found for are ours.
 */
#ifndef PAF_IME_IME_H
#define PAF_IME_IME_H

#include <stddef.h>
#include <stdint.h>
#include <libime/libime_api.h>
#include <libime/libime_error.h>

/* the IME client's errors besides the SDK's SCE_IME_ERROR_* (names ours) */
/* the IME service is not running */
#define SCE_IME_VSH_ERROR_SERVICE_NOT_FOUND		-2146433103	/* (0x801007B1) */
/* the IME service refused the connection, or the system IME is open */
#define SCE_IME_VSH_ERROR_BUSY				-2146433102	/* (0x801007B2) */
/* the IME service's answer is not the size asked for */
#define SCE_IME_VSH_ERROR_INVALID_RESPONSE		-2146433097	/* (0x801007B7) */
#define SCE_IME_VSH_ERROR_INVALID_HANDLE		-2146433095	/* (0x801007B9) */

/* the IME's input types besides the SDK's SCE_IME_TYPE_* (0 to 5) */
#define SCE_IME_VSH_TYPE_NUMBER			0x100
#define SCE_IME_VSH_TYPE_BASIC_LATIN		0x102
#define SCE_IME_VSH_TYPE_PHONE_NUMBER		0x104
#define SCE_IME_VSH_TYPE_DIGITS			0x105

extern "C" {

/* the IME's events besides the SDK's SCE_IME_EVENT_*: the edits from 0x100 and the caret moves
   from 0x200 are the system IME's own */
enum {
	/* the preedit text replaced */
	SCE_IME_VSH_EVENT_UPDATE_PREEDIT = 0x100,
	/* the preedit text made part of the string */
	SCE_IME_VSH_EVENT_COMMIT = 0x101,
	/* text inserted at the caret */
	SCE_IME_VSH_EVENT_INSERT = 0x102,
	/* the character before the caret deleted */
	SCE_IME_VSH_EVENT_BACKSPACE = 0x103,
	/* everything before the caret deleted */
	SCE_IME_VSH_EVENT_DELETE_TO_START = 0x104,
	/* the string emptied */
	SCE_IME_VSH_EVENT_CLEAR = 0x105,
	/* the character at the caret deleted */
	SCE_IME_VSH_EVENT_DELETE = 0x106,
	/* a space typed after a sentence: a backspace when the text inserted last ended with one */
	SCE_IME_VSH_EVENT_SENTENCE_SPACE = 0x114,
	SCE_IME_VSH_EVENT_CARET_LEFT = 0x200,
	SCE_IME_VSH_EVENT_CARET_RIGHT = 0x201,
	SCE_IME_VSH_EVENT_CARET_UP = 0x202,
	SCE_IME_VSH_EVENT_CARET_DOWN = 0x203
};

/* the IME's window, in screen pixels */
struct SceImeVshRect {
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
};

union SceImeVshEventParam {
	/* SCE_IME_EVENT_OPEN and SCE_IME_EVENT_CHANGE_SIZE */
	SceImeVshRect rect;
	/* the edits: the text, and where the caret goes in the preedit text */
	struct {
		const wchar_t *str;
		uint32_t caretIndex;
	} text;
	/* SCE_IME_EVENT_PRESS_CLOSE: the focus is shown again */
	bool showFocus;
	uint32_t reserved[8];
};

struct SceImeVshEvent {
	uint32_t id;
	SceImeVshEventParam param;
};

typedef void (*SceImeVshEventHandler)(void *arg, const SceImeVshEvent *e);

/* 0x40 bytes; a caller of 0x30 bytes (no sdkVersion) is taken as one of SDK 1.03 */
struct SceImeVshParam {
	uint32_t size;
	uint32_t inputMethod;
	uint64_t supportedLanguages;
	uint32_t languagesForced;
	uint32_t type;
	uint32_t option;
	void *arg;
	SceImeVshEventHandler handler;
	int32_t (*filter)(wchar_t *out, uint32_t *out_len, const wchar_t *src, uint32_t src_len);
	/* the client's state (ImeVshContext), SCE_IME_VSH_WORK_SIZE bytes */
	void *work;
	uint8_t enterLabel;
	/* whether a focus is shown (ScePafWidget_529FB877) */
	uint8_t focusShown;
	uint8_t reserved0[2];
	uint32_t sdkVersion;
	uint8_t reserved[12];
};

/* what sceImeVshOpen clears of the work */
#define SCE_IME_VSH_WORK_SIZE		0x2800

/* where the caret is drawn, in screen pixels */
struct SceImeVshCaretGeometry {
	uint32_t x;
	uint32_t y;
	uint32_t height;
};

/* sceImeFilterText's input type: the languages and the type of SceImeVshParam */
struct SceImeFilterParam {
	uint64_t supportedLanguages;
	uint32_t type;
};

/* a word of the user dictionary: the word and its reading, as the IME service has them */
struct SceImeDicWord {
	wchar_t text[52];
};

void sceImeInit(void);
void sceImeTerm(void);
/* the characters of in that the input type allows, at most outMax - 1 and a NUL, into out */
int32_t sceImeFilterText(wchar_t *out, uint32_t *outLength, uint32_t outMax,
			 const SceImeFilterParam *param, const wchar_t *in, uint32_t inLength);
/* gives the open IME a new parameter */
int32_t sceImeVshSetParam(void *ime, const SceImeVshParam *param);
/* opens the IME with the parameter */
int32_t sceImeVshOpen(void **ime, const SceImeVshParam *param);
/* sends what changed, and passes the IME's events to the handler */
int32_t sceImeVshUpdate(void *ime);
/* makes the preedit text part of the string */
int32_t sceImeVshClearPreedit(void *ime);
/* the caret outside the preedit text */
int32_t sceImeVshConfirmPreedit(void *ime);
int32_t sceImeVshSetCaretGeometry(void *ime, const SceImeVshCaretGeometry *geometry);
/* the text and the caret's index in it */
int32_t sceImeVshUpdateContext(void *ime, const wchar_t *text, uint32_t caret);
/* the caret at the index in the preedit text */
int32_t ScePafLowlayer_5CE62E49(void *ime, uint32_t index);
/* closes the IME */
int32_t sceImeVshClose(void *ime);
int32_t sceImeConfigSet(uint64_t language, int32_t id, int32_t value);
int32_t sceImeConfigGet(uint64_t language, int32_t id, int32_t *value);
int32_t sceImeDicGetWords(uint64_t language, void *buffer);
int32_t sceImeDicAddWord(uint64_t language, const SceImeDicWord *word);
int32_t sceImeDicDeleteWord(uint64_t language, const SceImeDicWord *word);
int32_t sceImeDicReplaceWord(uint64_t language, const SceImeDicWord *oldWord,
			     const SceImeDicWord *newWord);
/* the user dictionary's IME service methods 37 and 38 */
int32_t ScePafLowlayer_8B6B2EA3(void);
int32_t ScePafLowlayer_7982DC0B(void);

}

#endif
