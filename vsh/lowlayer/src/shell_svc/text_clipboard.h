#ifndef TEXT_CLIPBOARD_H
#define TEXT_CLIPBOARD_H

#include <kernel.h>

/* The text clipboard's SceShellUtil exports (text_clipboard.cpp). */
extern "C" {
int SceShellUtil_C4810C56(const SceWChar16 *pText, SceUInt32 length);
int SceShellUtil_1B186905(SceWChar16 *pText, SceUInt32 maxLength, SceUInt32 *pLength);
int SceShellUtil_D0DDEDBC(void);
}

/* module_start and module_stop's: create (the shell) or open the storage, and close it. */
int textClipboardInitialize(bool isShell);
int textClipboardFinalize(void);

#endif
