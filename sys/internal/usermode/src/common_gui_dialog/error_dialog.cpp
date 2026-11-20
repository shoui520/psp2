/*
 * The error dialog: an error code's text from the library's table (the plugin's string for the
 * table's resource ID unless the caller gives one), the error's code after it, its details in a
 * list dialog behind a button, the system update a button starts, and the error history. No
 * binary names this file, its classes, tables or members; the names are ours.
 */
#include <stdint.h>
#include <std/new>
#include <std/stdio.h>
#include <std/string.h>
#include <system/system.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include <error/error.h>
#include "dialog.h"

/* Sony's code here inlines the item IDs' destructor and calls their copy */
PAF_VECTOR_INLINE_DTOR(unsigned int)
PAF_VECTOR_NOINLINE_COPY(unsigned int)

using namespace paf;
using namespace paf::ui;

/* the table's flags: the error goes to the error history; its code follows its text */
#define ERROR_DIALOG_POST_HISTORY	0x2
#define ERROR_DIALOG_SHOW_CODE		0x4

/* the most details a table's list or the error history holds */
#define ERROR_DIALOG_MAX_DETAILS	10
#define ERROR_DIALOG_MAX_HISTORY_IDS	15

/* the buttons of the dialog's layouts: its details, and the system update */
#define ERROR_DIALOG_BUTTON_DETAILS	10
#define ERROR_DIALOG_BUTTON_UPDATE	11

/* the default text, the details' heading, the two texts that ask for a system update, and the
   sound of the dialog's closing (resource IDs) */
#define ERROR_DIALOG_DEFAULT_TEXT	0xC51AA4A4
#define ERROR_DIALOG_DETAILS_HEADING	0x2F0C3670
#define ERROR_DIALOG_UPDATE_TEXT	0xE8E8C3D2
#define ERROR_DIALOG_UPDATE_TEXT2	0x6578516F
#define ERROR_DIALOG_SOUND		0xD74A54E5

/* sceAppMgrLaunchAppByUri's flags for the settings' updater */
#define ERROR_DIALOG_UPDATER_FLAGS	0x60000

/* an error code's resource ID and flags */
struct ErrorDialogEntry {
	SceUInt32 code;
	SceUInt32 messageId;
	SceUInt32 flags;
};

/* an error code's details: their resource IDs, up to a zero */
struct ErrorDialogDetails {
	SceUInt32 code;
	const SceUInt32 *messageIds;
};

/* a resource ID and the one the PlayStation TV shows in its place */
struct ErrorDialogDolceId {
	SceUInt32 id;
	SceUInt32 dolceId;
};

/* the error codes, in order */
static const ErrorDialogEntry s_errors[] = {
	{ 0x80108005, 0x929BA9CE, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80108007, 0x929BA9CE, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x8010801C, 0xADAF0EBA, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80108023, 0xDF801FBC, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80182010, 0x59D5975F, ERROR_DIALOG_SHOW_CODE },
	{ 0x80182209, 0x59D5975F, ERROR_DIALOG_SHOW_CODE },
	{ 0x80410123, 0xA5412FDD, ERROR_DIALOG_SHOW_CODE },
	{ 0x80410133, 0xF529C1FD, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80410135, 0xF529C1FD, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80410136, 0x56911DD9, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x8041013C, 0xA5412FDD, ERROR_DIALOG_SHOW_CODE },
	{ 0x8041013D, 0xF529C1FD, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80410140, 0xF529C1FD, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80410141, 0xF529C1FD, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x804101A3, 0xFFC2FAAB, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x804101DF, 0x139C1684, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x804101E1, 0x439A4511, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x804101E2, 0x95A908A1, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x804101E3, 0x139C1684, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x804101E4, 0x139C1684, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x804101E5, 0x139C1684, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x804101E6, 0x139C1684, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x804101E7, 0x139C1684, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x804101E8, 0x139C1684, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x804101E9, 0x139C1684, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80411412, 0xDC60454C, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80411601, 0xDC60454C, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80411602, 0xDC60454C, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80411603, 0xDC60454C, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80411604, 0xDC60454C, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80411605, 0xDC60454C, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80411607, 0xDC60454C, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80411608, 0xDC60454C, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x8041160C, 0xDC60454C, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80411681, 0xA9FEC161, 0 },
	{ 0x80411682, 0xFD709328, 0 },
	{ 0x80412108, 0xDC60454C, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x8041210B, 0xDC60454C, ERROR_DIALOG_SHOW_CODE },
	{ 0x8041210C, 0xDC60454C, ERROR_DIALOG_SHOW_CODE },
	{ 0x80412112, 0xA9FEC161, 0 },
	{ 0x80412113, 0xFD709328, 0 },
	{ 0x80412114, 0x94802D34, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x8041211B, 0xDC60454C, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x8041211C, 0xDC60454C, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x8041211E, 0xA5412FDD, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80412196, 0x94802D34, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x804121CF, 0x4ECF42BA, 0 },
	{ 0x8041280D, 0xCFA85A67, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80431064, 0x5EB9A66F, ERROR_DIALOG_SHOW_CODE },
	{ 0x80431068, 0xA5412FDD, ERROR_DIALOG_SHOW_CODE },
	{ 0x80431075, 0x59D5975F, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80432060, 0xFFC2FAAB, ERROR_DIALOG_SHOW_CODE },
	{ 0x80435062, 0xA5412FDD, ERROR_DIALOG_SHOW_CODE },
	{ 0x80436001, 0x139C1684, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80436002, 0x439A4511, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80436003, 0x95A908A1, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80436004, 0x139C1684, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80436005, 0x139C1684, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80436006, 0x139C1684, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80436007, 0x139C1684, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80436008, 0x139C1684, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80436009, 0x139C1684, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x8043600A, 0x139C1684, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x8054001F, 0x4CC21C81, ERROR_DIALOG_POST_HISTORY },
	{ 0x80540020, 0xE6446368, ERROR_DIALOG_POST_HISTORY },
	{ 0x80540021, 0xA1EB3F2F, ERROR_DIALOG_POST_HISTORY },
	{ 0x80540022, 0xEF8264A0, ERROR_DIALOG_POST_HISTORY },
	{ 0x80540026, 0x546D7C0E, ERROR_DIALOG_POST_HISTORY },
	{ 0x80540027, 0x54B013F8, ERROR_DIALOG_POST_HISTORY },
	{ 0x80540028, 0xB28EEB5D, ERROR_DIALOG_POST_HISTORY },
	{ 0x80540029, 0xE6446368, ERROR_DIALOG_POST_HISTORY },
	{ 0x8054002A, 0xB28EEB5D, ERROR_DIALOG_POST_HISTORY },
	{ 0x80550400, 0x31B24123, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80550401, 0x43E94314, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80550402, 0x72A1B782, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80550403, 0x43E94314, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80550412, 0xE8E8C3D2, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80550421, 0xD7D2840B, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80550427, 0x391BA5FA, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80550430, 0x6FF4BFE4, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80550431, 0x6FF4BFE4, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80550440, 0xC1DCF0FA, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80550441, 0xAFAAB7E2, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x8055044F, 0xFB8F2D1C, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80550460, 0xEF8B3350, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80550508, 0xAFAAB7E2, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x8055050A, 0x3893A86A, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80550802, 0x56911DD9, ERROR_DIALOG_SHOW_CODE },
	{ 0x8055080A, 0x6FF4BFE4, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80550811, 0x6FF4BFE4, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x805508A0, 0xD679B275, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x805508A1, 0x57F1E4C6, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x805508A2, 0xD679B275, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x805508A3, 0xD679B275, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x805508A4, 0x57F1E4C6, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x805508A5, 0xD679B275, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x805508AA, 0xD679B275, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x805508AB, 0x57F1E4C6, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x805508AC, 0xD679B275, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x805508AD, 0xD679B275, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x805508AE, 0x57F1E4C6, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x805508AF, 0xD679B275, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x805508B1, 0xD679B275, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x805508B2, 0x57F1E4C6, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x805508B3, 0xD679B275, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80550F17, 0x56911DD9, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80551002, 0x56911DD9, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80551006, 0x59D5975F, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80551208, 0xA5412FDD, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80551213, 0xA5412FDD, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80551214, 0xA5412FDD, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80551216, 0xA5412FDD, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80551226, 0x56911DD9, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80551227, 0xA5412FDD, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80551410, 0xA5412FDD, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80551512, 0x2657A2DC, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80552007, 0xA5412FDD, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80552008, 0xA5412FDD, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80552009, 0xA5412FDD, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x8055200A, 0xA5412FDD, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x8055890F, 0xD679B275, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80558920, 0xD679B275, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80558921, 0x57F1E4C6, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80558A0A, 0x6FF4BFE4, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80558A11, 0x6FF4BFE4, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80558A64, 0xD679B275, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x80559413, 0xF529C1FD, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x8055A814, 0x391BA5FA, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x8220020B, 0x7D59BF0C, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x82200340, 0x43E94314, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x8220120B, 0x7D59BF0C, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x82201340, 0x43E94314, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x8220220B, 0x7D59BF0C, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x82202340, 0x43E94314, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x8220420B, 0x7D59BF0C, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x82204340, 0x43E94314, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x8220520B, 0x7D59BF0C, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x82205340, 0x43E94314, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x8220620B, 0x7D59BF0C, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x82206340, 0x43E94314, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x8230CF03, 0x43E94314, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x8230CF3E, 0x7D59BF0C, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x82E00014, 0xD7D2840B, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x82E0001B, 0x6FF4BFE4, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x82E0001C, 0x6FF4BFE4, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x82E0001D, 0x391BA5FA, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x82E00064, 0xEF8B3350, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x82E00067, 0xC1DCF0FA, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x82E01042, 0xFB8F2D1C, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x82E01050, 0x6FF4BFE4, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x82E1019A, 0x57F1E4C6, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
	{ 0x82E101F7, 0xD679B275, ERROR_DIALOG_POST_HISTORY | ERROR_DIALOG_SHOW_CODE },
};

static const SceUInt32 s_details80000002[] = {
	0x64E52B14, 0xCFA85A67, 0x92232D9E, 0
};

static const SceUInt32 s_details80540021[] = {
	0x7EBE508C, 0
};

static const SceUInt32 s_details80411412[] = {
	0x0AA1805A, 0x430C46AC, 0x06C0A07A, 0xA1802098, 0xBE3193AB, 0
};

static const SceUInt32 s_details80550F17[] = {
	0x0AA1805A, 0x430C46AC, 0x67A9D53D, 0
};

static const SceUInt32 s_details80550802[] = {
	0x0AA1805A, 0x430C46AC, 0x67A9D53D, 0
};

static const SceUInt32 s_details80182010[] = {
	0x0AA1805A, 0x430C46AC, 0x67A9D53D, 0x3D75D774, 0xBDCCFF2E, 0x06C0A07A, 0xA1802098,
	0xBE3193AB, 0
};

static const SceUInt32 s_details80551208[] = {
	0x0AA1805A, 0x430C46AC, 0x67A9D53D, 0xBDCCFF2E, 0x06C0A07A, 0xA1802098, 0xBE3193AB, 0
};

static const SceUInt32 s_details80551002[] = {
	0x67A9D53D, 0
};

static const SceUInt32 s_details80108005[] = {
	0xBE3193AB, 0
};

static const ErrorDialogDetails s_errorDetails[] = {
	{ 0x80000002, s_details80000002 },
	{ 0x80108005, s_details80108005 },
	{ 0x80108007, s_details80108005 },
	{ 0x8010801C, s_details80108005 },
	{ 0x80182010, s_details80182010 },
	{ 0x80182209, s_details80182010 },
	{ 0x80410123, s_details80182010 },
	{ 0x80410136, s_details80182010 },
	{ 0x8041013C, s_details80182010 },
	{ 0x8041013D, s_details80182010 },
	{ 0x804101DF, s_details80182010 },
	{ 0x804101E1, s_details80182010 },
	{ 0x804101E2, s_details80182010 },
	{ 0x804101E3, s_details80182010 },
	{ 0x804101E4, s_details80182010 },
	{ 0x804101E5, s_details80182010 },
	{ 0x804101E6, s_details80182010 },
	{ 0x804101E7, s_details80182010 },
	{ 0x804101E8, s_details80182010 },
	{ 0x804101E9, s_details80182010 },
	{ 0x80411412, s_details80411412 },
	{ 0x80411601, s_details80411412 },
	{ 0x80411602, s_details80411412 },
	{ 0x80411603, s_details80411412 },
	{ 0x80411604, s_details80411412 },
	{ 0x80411605, s_details80411412 },
	{ 0x80411607, s_details80411412 },
	{ 0x80411608, s_details80411412 },
	{ 0x8041160C, s_details80411412 },
	{ 0x80412108, s_details80182010 },
	{ 0x8041210B, s_details80411412 },
	{ 0x8041210C, s_details80411412 },
	{ 0x8041211B, s_details80411412 },
	{ 0x8041211C, s_details80411412 },
	{ 0x80431064, s_details80182010 },
	{ 0x80431068, s_details80182010 },
	{ 0x80431075, s_details80182010 },
	{ 0x80435062, s_details80182010 },
	{ 0x80436001, s_details80182010 },
	{ 0x80436002, s_details80182010 },
	{ 0x80436003, s_details80182010 },
	{ 0x80436004, s_details80182010 },
	{ 0x80436005, s_details80182010 },
	{ 0x80436006, s_details80182010 },
	{ 0x80436007, s_details80182010 },
	{ 0x80436008, s_details80182010 },
	{ 0x80436009, s_details80182010 },
	{ 0x8043600A, s_details80182010 },
	{ 0x80540021, s_details80540021 },
	{ 0x80550802, s_details80550802 },
	{ 0x80550F17, s_details80550F17 },
	{ 0x80551002, s_details80551002 },
	{ 0x80551208, s_details80551208 },
	{ 0x80551213, s_details80551208 },
	{ 0x80551214, s_details80551208 },
	{ 0x80551216, s_details80551208 },
	{ 0x80551226, s_details80551208 },
	{ 0x80551227, s_details80551208 },
	{ 0x80551410, s_details80551208 },
	{ 0x80552007, s_details80551208 },
	{ 0x80552008, s_details80551208 },
	{ 0x80552009, s_details80551208 },
	{ 0x8055200A, s_details80551208 },
};

/* the texts the PlayStation TV shows in place of the Vita's: the error's text, its details */
static const ErrorDialogDolceId s_dolceTextIds[] = {
	{ 0xA9FEC161, 0xC51AA4A4 },
	{ 0xFD709328, 0x4ECF42BA },
	{ 0, 0 },
};

static const ErrorDialogDolceId s_dolceDetailIds[] = {
	{ 0x06C0A07A, 0xEF8F2AD5 },
	{ 0, 0 },
};

/* the buttons: details and OK, details alone, and the system update and cancel */
static const CommonGuiDialogButton s_detailsButtons[] = {
	{ 12, 0x22CEBD61, ERROR_DIALOG_BUTTON_DETAILS, 0x20413274 },
	{ 13, 0x805BECA6, 2, 0x20413274 },
	{ 0, 0, 0, 0 },
};

static const CommonGuiDialogButton s_detailsButtonsNoOk[] = {
	{ 12, 0x22CEBD61, ERROR_DIALOG_BUTTON_DETAILS, 0x20413274 },
	{ 0, 0, 0, 0 },
};

static const CommonGuiDialogButton s_updateButtons[] = {
	{ 12, 0x6C2B7E72, ERROR_DIALOG_BUTTON_UPDATE, 0x20413274 },
	{ 13, 0xADCD9ADF, 3, 0xC17D7980 },
	{ 0, 0, 0, 0 },
};

/* the layouts with those buttons, on the pages and parts of the library's own */
static CommonGuiDialogLayout s_detailsLayout = {
	SceCommonGuiDialog_22B17F2B.page, SceCommonGuiDialog_22B17F2B.parts, s_detailsButtons
};

static CommonGuiDialogLayout s_detailsLayoutNoOk = {
	SceCommonGuiDialog_AF75CA6D.page, SceCommonGuiDialog_AF75CA6D.parts, s_detailsButtonsNoOk
};

static CommonGuiDialogLayout s_updateLayout = {
	SceCommonGuiDialog_22B17F2B.page, SceCommonGuiDialog_22B17F2B.parts, s_updateButtons
};

/* where the system update starts */
static const char *s_updaterUri = "settings_dlg:updater";

/* a table's entry for the error code (the table in order of its codes), or NULL */
template<class Entry>
static inline const Entry *FindByCode(const Entry *first, const Entry *last, SceUInt32 code)
{
	int32_t count = last - first;

	while (count > 0) {
		int32_t half = count >> 1;
		const Entry *mid = first + half;

		if (mid->code < code) {
			first = mid + 1;
			count -= half + 1;
		} else {
			count = half;
		}
	}
	return first->code == code ? first : NULL;
}

#define TABLE_END(table)	(&(table)[sizeof(table) / sizeof((table)[0])])

/* the resource ID the model shows */
static inline SceUInt32 ModelMessageId(const ErrorDialogDolceId *ids, SceUInt32 id)
{
	if (system::Model::GetType() == system::Model::Type_Dolce) {
		for (; ids->id != 0; ids++) {
			if (ids->id == id)
				return ids->dolceId;
		}
	}
	return id;
}

static inline const wchar_t *GetMessage(Plugin *plugin, SceUInt32 id)
{
	return plugin->GetString(IDParam(s_empty_name, id));
}

/* the text's format with its arguments in place */
static __attribute__((noinline)) void FormatMessage(wstring *out,
						    const CommonGuiMessageDialogText &text)
{
	if (text.args.empty()) {
		*out = text.format;
	} else {
		const wchar_t *args[9];
		uint32_t i;

		for (i = 0; i < 9; i++)
			args[i] = i < text.args.size() ? text.args[i].c_str() : NULL;
		*out = MsgDialogFormat(text.format.c_str(), args[0], args[1], args[2], args[3],
				       args[4], args[5], args[6], args[7], args[8]);
	}
}

/* a narrow string as a wide one, for as long as it lives */
class WideString {
public:
	WideString(const char *text)
	{
		ScePafMisc_C11CB5E4(&m_string, text, common::EncodedString::Encoding_Utf8);
	}

	~WideString()
	{
		ScePafMisc_B45A7521(&m_string);
	}

	const wchar_t *c_str()
	{
		return ScePafMisc_53948F03(&m_string);
	}

	common::EncodedString m_string;
};

/* the parameters filled from the table, and the error's code to show after the text */
class ErrorDialogParam : public CommonGuiMessageDialogParam {
public:
	ErrorDialogParam(const CommonGuiMessageDialogParam &param)
		: CommonGuiMessageDialogParam(param)
	{
	}

	void Fill(CommonGuiMessageDialogOpenHandler *openHandler);
	void PostHistory();

	wstring code;
};

void ErrorDialogParam::Fill(CommonGuiMessageDialogOpenHandler *openHandler)
{
	Plugin *plugin = resourcePlugin;
	const ErrorDialogEntry *entry = FindByCode(s_errors, TABLE_END(s_errors), type);

	if (entry != NULL && messageId == 0)
		messageId = entry->messageId;

	const ErrorDialogDetails *details = FindByCode(s_errorDetails, TABLE_END(s_errorDetails),
							 type);

	if (details != NULL) {
		const SceUInt32 *ids = details->messageIds;
		uint32_t count;
		uint32_t i;

		for (count = 0; count < ERROR_DIALOG_MAX_DETAILS; count++) {
			if (ids[count] == 0)
				break;
		}
		if (messageIds.size() < count)
			messageIds.resize(count);
		for (i = 0; i < count; i++) {
			if (messageIds[i] == 0)
				messageIds[i] = ids[i];
		}
	}
	if (openHandler != NULL) {
		(*openHandler)(this);
		delete openHandler;
	}
	if (buttonCount == 2) {
		if (entry != NULL && (entry->flags & ERROR_DIALOG_POST_HISTORY) == 0)
			buttonCount = 0;
		else
			buttonCount = 1;
	}
	if (message.empty())
		message = GetMessage(plugin, ERROR_DIALOG_DEFAULT_TEXT);
	if (title.format.empty()) {
		if (messageId != 0)
			title.format = GetMessage(plugin, ModelMessageId(s_dolceTextIds, messageId));
		if (title.format.empty())
			title.format = message;
	}
	if (title.format == message)
		defaultButton = 1;
	if (defaultButton == 2) {
		if (entry == NULL || (entry->flags & ERROR_DIALOG_SHOW_CODE) != 0)
			defaultButton = 1;
		else
			defaultButton = 0;
	}
	if (cancelButton == 2)
		cancelButton = 0;

	int32_t parts = defaultButton != 0;

	if (cancelButton != 0)
		parts++;
	if (parts > 0) {
		code += L"(";
		if (defaultButton != 0) {
			char external[18];

			sceErrorGetExternalString(external, type);
			code += WideString(external).c_str();
		}
		if (parts > 1)
			code += L", ";
		if (cancelButton != 0) {
			wchar_t hex[9];

			sce_paf_swprintf(hex, 9, L"%08x", type);
			code += hex;
		}
		code += L")";
	}
	FormatMessage(&title.format, title);
	if (items.size() < messageIds.size())
		items.resize(messageIds.size());

	uint32_t i;

	for (i = 0; i < items.size(); i++) {
		if (items[i].text.format.empty() && i < messageIds.size()) {
			SceUInt32 id = messageIds[i];

			if (id != 0)
				items[i].text.format = GetMessage(plugin,
								  ModelMessageId(s_dolceDetailIds, id));
		}
		FormatMessage(&items[i].text.format, items[i].text);
	}
}

void ErrorDialogParam::PostHistory()
{
	if (buttonCount != 0) {
		ErrorRecord record;
		SceAppMgrAppStatus status;
		uint32_t count;
		uint32_t i;
		int ret;

		sce_paf_memset(&record, 0, sizeof(record));
		record.code = type;

		Utf8String text(title.format);

		sce_paf_snprintf(record.text, 256, "%s", text.c_str());
		count = messageIds.size();
		if (count > ERROR_DIALOG_MAX_HISTORY_IDS)
			count = ERROR_DIALOG_MAX_HISTORY_IDS;
		for (i = 0; i < count; i++)
			record.messageIds[i] = messageIds[i];
		record.messageIdCount = count;
		record.id = option.reserved | (option.flags << 24);
		if (buttonType == -2)
			ret = sceAppMgrGetStatusByName(NULL, &status);
		else
			ret = sceAppMgrGetStatusByAppId(buttonType, &status);
		if (ret == 0)
			sce_paf_strlcpy((char *)record.key, status.appName, sizeof(record.key));
		sceErrorHistoryPostError(&record);
	}
}

/* the base of the message dialog's handlers: no members of its own (Sony's module has its vtable
   and destructors, which nothing calls) */
class MessageDialogHandler : public CommonGuiDialogHandler {
};

/* the handler of a dialog that waits: its result set, and the dialog let go */
class WaitHandler : public MessageDialogHandler {
public:
	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		SceCommonGuiDialog_2CA16BBF(value, button);
		SceCommonGuiDialog_BC311DC0(value);
	}
};

/* the details' handler: the error dialog's own, passed on */
class DetailsHandler : public MessageDialogHandler {
public:
	DetailsHandler(bool wait, CommonGuiDialogHandler *handler)
		: m_wait(wait), m_handler(handler)
	{
	}

	virtual ~DetailsHandler()
	{
		if (m_handler != NULL)
			delete m_handler;
		m_handler = NULL;
	}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		if (m_wait)
			SceCommonGuiDialog_2CA16BBF(value, button);
		else if (m_handler != NULL)
			(*m_handler)(value, button);
		SceCommonGuiDialog_BC311DC0(value);
	}

	bool m_wait;
	CommonGuiDialogHandler *m_handler;
};

/* the error dialog's handler: its details opened in its place, the system update started, or
   the caller's handler called */
class ErrorDialogHandler : public MessageDialogHandler {
public:
	ErrorDialogHandler(const CommonGuiDialogParam &param, bool wait, bool showOkButton,
			   const vector<CommonGuiMessageDialogItem> &items,
			   CommonGuiDialogHandler *handler)
		: m_param(param), m_wait(wait), m_showOkButton(showOkButton), m_items(items),
		  m_handler(handler)
	{
	}

	virtual ~ErrorDialogHandler()
	{
		if (m_handler != NULL)
			delete m_handler;
		m_handler = NULL;
	}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		if (button == ERROR_DIALOG_BUTTON_DETAILS) {
			CommonGuiMessageDetailsParam details;

			CopyDialogParam(&details, &m_param);
			details.soundId = 0;
			vector<CommonGuiMessageDialogItem>(m_items).swap(details.items);
			details.showOkButton = m_showOkButton;
			details.dialogId = value;
			details.handler = new DetailsHandler(m_wait, m_handler);
			m_handler = NULL;
			SceCommonGuiDialog_5F13DF1F(&details);
		} else if (button == ERROR_DIALOG_BUTTON_UPDATE) {
			sceAppMgrLaunchAppByUri(ERROR_DIALOG_UPDATER_FLAGS, s_updaterUri);
		} else {
			if (m_wait)
				SceCommonGuiDialog_2CA16BBF(value, button);
			else if (m_handler != NULL)
				(*m_handler)(value, button);
			SceCommonGuiDialog_BC311DC0(value);
		}
	}

	CommonGuiDialogParam m_param;
	bool m_wait;
	bool m_showOkButton;
	vector<CommonGuiMessageDialogItem> m_items;
	CommonGuiDialogHandler *m_handler;
};

int SceCommonGuiDialog_5F13DF1F(const CommonGuiMessageDetailsParam *param)
{
	vector<CommonGuiMessageDialogItem> items(param->items);

	if (param->itemIds.size() > items.size())
		items.resize(param->itemIds.size());

	CommonGuiListDialogParam list(true);

	CopyDialogParam(&list, param);
	list.type = param->showOkButton ? 1 : 2;
	list.wait = param->wait;
	list.handler = param->handler;
	list.items.resize(items.size() + 1);
	list.items[0].text = GetMessage(param->resourcePlugin, ERROR_DIALOG_DETAILS_HEADING);
	list.items[0].mark = false;

	uint32_t i;

	for (i = 0; i < items.size(); i++) {
		if (items[i].text.format.empty() && i < param->itemIds.size()) {
			SceUInt32 id = param->itemIds[i];

			if (id != 0)
				items[i].text.format = GetMessage(param->resourcePlugin,
								  ModelMessageId(s_dolceDetailIds, id));
		}

		wstring text;

		FormatMessage(&text, items[i].text);
		list.items[i].text += L"\n";
		list.items[i + 1].text = text;
		if (!items[i].buttonLabel.empty()) {
			list.items[i + 1].buttonLabel = items[i].buttonLabel;
			list.items[i + 1].buttonValue = items[i].buttonValue;
		}
	}
	return SceCommonGuiDialog_F3D37CAE(&list);
}

int SceCommonGuiDialog_20B141F1(const CommonGuiMessageDialogParam *param)
{
	ErrorDialogParam error(*param);

	error.Fill(error.openHandler);
	error.PostHistory();

	CommonGuiLayoutDialogParam layoutParam;
	int id;

	CopyDialogParam(&layoutParam, param);
	layoutParam.text = error.title.format + L"\n" + error.code;
	if (error.useLayout) {
		layoutParam.layout = error.layout;
	} else if (error.title.format == GetMessage(param->resourcePlugin, ERROR_DIALOG_UPDATE_TEXT) ||
		   error.title.format == GetMessage(param->resourcePlugin, ERROR_DIALOG_UPDATE_TEXT2)) {
		layoutParam.layout = s_updateLayout;
	} else if (!error.items.empty()) {
		layoutParam.layout = error.modal ? s_detailsLayout : s_detailsLayoutNoOk;
	} else {
		layoutParam.layout = error.modal ? SceCommonGuiDialog_AF75CA6D :
						   SceCommonGuiDialog_7AEA192C;
	}

	CommonGuiDialogHandler *handler = param->handler;

	if (param->closeOnSelect) {
		if (handler != NULL)
			delete handler;
		handler = new WaitHandler();
	}
	layoutParam.handler = new ErrorDialogHandler(*param, param->closeOnSelect, error.modal,
						     error.items, handler);
	id = SceCommonGuiDialog_CCFCD9A6(&layoutParam);
	if (id >= 0) {
		SceCommonGuiDialog_D3F78BA5(id, ERROR_DIALOG_SOUND);
		if (param->closeOnSelect)
			id = SceCommonGuiDialog_D2381B88(id);
	}
	return id;
}

int SceCommonGuiDialog_A6422883(Plugin *plugin, const CommonGuiMessageDialogParam *param,
				CommonGuiDialogHandler *handler,
				CommonGuiMessageDialogOpenHandler *openHandler)
{
	CommonGuiMessageDialogParam message(*param);

	message.resourcePlugin = plugin;
	message.handler = handler;
	message.openHandler = openHandler;
	return SceCommonGuiDialog_20B141F1(&message);
}

int SceCommonGuiDialog_518ABE78(Plugin *plugin, const CommonGuiMessageDialogParam *param,
				CommonGuiMessageDialogOpenHandler *openHandler)
{
	int id = SceCommonGuiDialog_A6422883(plugin, param, new WaitHandler(), openHandler);

	if (id >= 0)
		return SceCommonGuiDialog_D2381B88(id);
	return id;
}

void SceCommonGuiDialog_6FF8219B(const CommonGuiMessageDialogParam *param, wstring *text,
				 wstring *code)
{
	ErrorDialogParam error(*param);

	error.Fill(error.openHandler);
	if (text != NULL)
		*text = error.title.format;
	if (code != NULL)
		*code = error.code;
}
