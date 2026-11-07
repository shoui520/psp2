/*
 * The library's layouts, the ones it exports and one SceCommonGuiDialog_F3D37CAE uses: each a page
 * of the common resource, the parts its paf::ui::Dialog finds by index, and its buttons. The
 * resource IDs are known only by their hashes. No binary names this file or its objects; the
 * names are ours, the pages and parts after the first layout that uses them.
 */
#include <stdint.h>
#include "layouts.h"

using namespace paf;
using namespace paf::ui;

/* the pages: the plugin, the page and the template (their IDs' hashes) */
const CommonGuiDialogPage g_pageAF75CA6D = {
	{ 0x3A918C2C, 0x438C7285, 0xD3AA1490 }
};

static const CommonGuiDialogPage s_page6CD802FC = {
	{ 0x3A918C2C, 0x438C7285, 0x72F4CDB2 }
};

static const CommonGuiDialogPage s_page22B17F2B = {
	{ 0x3A918C2C, 0x438C7285, 0x318EB055 }
};

static const CommonGuiDialogPage s_page78399261 = {
	{ 0x3A918C2C, 0x438C7285, 0x1876EDB9 }
};

static const CommonGuiDialogPage s_page446F8B49 = {
	{ 0x3A918C2C, 0x438C7285, 0xB99EACE9 }
};

static const CommonGuiDialogPage s_page25BD4084 = {
	{ 0x3A918C2C, 0x438C7285, 0x62B94EAC }
};

static const CommonGuiDialogPage s_page95087497 = {
	{ 0x3A918C2C, 0x438C7285, 0x172BBE55 }
};

static const CommonGuiDialogPage s_pageEA557911 = {
	{ 0x3A918C2C, 0x438C7285, 0x5A7AC6CF }
};

static const CommonGuiDialogPage s_pageA0E0A437 = {
	{ 0x3A918C2C, 0x438C7285, 0x3605C1D0 }
};

static const CommonGuiDialogPage s_page7AEA192C = {
	{ 0x3A918C2C, 0x438C7285, 0x0EAB1063 }
};

const CommonGuiDialogPage g_pageInternal = {
	{ 0x3A918C2C, 0x438C7285, 0x094FBF94 }
};

const CommonGuiDialogPage g_pageE872FB5D = {
	{ 0x3A918C2C, 0x438C7285, 0xC062E36D }
};

const CommonGuiDialogPage g_page1424609E = {
	{ 0x3A918C2C, 0x438C7285, 0xF0CB76BD }
};

const CommonGuiDialogPage g_page3AC06BC6 = {
	{ 0x3A918C2C, 0x438C7285, 0x4A92F174 }
};

static const CommonGuiDialogPage s_pageDDC59760 = {
	{ 0x3A918C2C, 0x438C7285, 0xE9DBC9FD }
};

static const CommonGuiDialogPage s_page3785AD25 = {
	{ 0x3A918C2C, 0x438C7285, 0x633F7C06 }
};

static const CommonGuiDialogPage s_page5C8A2999 = {
	{ 0xFE5EC831, 0x438C7285, 0x91DECE53 }
};

static const CommonGuiDialogPage s_page2A68B4A4 = {
	{ 0xFE5EC831, 0x438C7285, 0x0C7FA69C }
};

static const CommonGuiDialogPage s_pageF8B2D7E1 = {
	{ 0x3A918C2C, 0x438C7285, 0x9AA18618 }
};

static const CommonGuiDialogPage s_page7D8FE85E = {
	{ 0x3A918C2C, 0x438C7285, 0x8F4AD103 }
};

static const CommonGuiDialogPage s_pageC4A714DE = {
	{ 0x3A918C2C, 0x438C7285, 0x42EACCB7 }
};

static const CommonGuiDialogPage s_page52A53037 = {
	{ 0x2D47374E, 0x438C7285, 0x16DA7EF4 }
};

static const CommonGuiDialogPage s_pageB04C8C1A = {
	{ 0x2D47374E, 0x438C7285, 0x1B4BF563 }
};

static const CommonGuiDialogPage s_pageDEDA75E4 = {
	{ 0x3A918C2C, 0x438C7285, 0x72F56B40 }
};

const CommonGuiDialogPage g_page04D71470 = {
	{ 0x3A918C2C, 0x438C7285, 0xF6FC5F73 }
};

/* the buttons: the part, its label, the button the handler is told and its look */
const CommonGuiDialogButton g_okButtons[] = {
	{ 12, 0x805BECA6, 2, 0x20413274 },
	{ 0, 0, 0, 0 },
};

static const CommonGuiDialogButton s_cancelButtons[] = {
	{ 12, 0xADCD9ADF, 3, 0xC17D7980 },
	{ 0, 0, 0, 0 },
};

const CommonGuiDialogButton g_noYesButtons[] = {
	{ 12, 0xA060AED9, 5, 0xC17D7980 },
	{ 13, 0x18A9DA04, 4, 0x20413274 },
	{ 0, 0, 0, 0 },
};

static const CommonGuiDialogButton s_cancelOkButtons[] = {
	{ 12, 0xADCD9ADF, 3, 0xC17D7980 },
	{ 13, 0x805BECA6, 2, 0x20413274 },
	{ 0, 0, 0, 0 },
};

static const CommonGuiDialogButton s_yesNoButtons[] = {
	{ 12, 0x18A9DA04, 4, 0x20413274 },
	{ 13, 0xA060AED9, 5, 0xC17D7980 },
	{ 0, 0, 0, 0 },
};

static const CommonGuiDialogButton s_yesNoCancelButtons[] = {
	{ 12, 0x18A9DA04, 4, 0x20413274 },
	{ 13, 0xA060AED9, 5, 0xC17D7980 },
	{ 14, 0xADCD9ADF, 3, 0xC17D7980 },
	{ 0, 0, 0, 0 },
};

const CommonGuiDialogButton g_04D71470Buttons[] = {
	{ 12, 0x0D47D642, 7, 0x20413274 },
	{ 13, 0xADCD9ADF, 3, 0xC17D7980 },
	{ 23, 0x96DA3F40, 8, 0x6622F138 },
	{ 0, 0, 0, 0 },
};

const CommonGuiDialogButton g_panelButtons[] = {
	{ 8, 0x00000000, 1, 0x25D6D211 },
	{ 0, 0, 0, 0 },
};

const CommonGuiDialogButton g_noButtons[] = {
	{ 0, 0, 0, 0 },
};

/* the parts: each index to its widget's ID */
Dialog::PartsList g_partsAF75CA6D[] = {
	{ 1, IDParam((uint32_t)0xD0791765) },
	{ 12, IDParam((uint32_t)0x5049F5E4) },
	{ 0, IDParam() },
};

static Dialog::PartsList s_parts6CD802FC[] = {
	{ 1, IDParam((uint32_t)0xD0791765) },
	{ 12, IDParam((uint32_t)0x5049F5E4) },
	{ 13, IDParam((uint32_t)0xBD559A97) },
	{ 0, IDParam() },
};

static Dialog::PartsList s_parts78399261[] = {
	{ 1, IDParam((uint32_t)0xD0791765) },
	{ 12, IDParam((uint32_t)0x5049F5E4) },
	{ 13, IDParam((uint32_t)0xBD559A97) },
	{ 14, IDParam((uint32_t)0xBCFBF95C) },
	{ 0, IDParam() },
};

static Dialog::PartsList s_parts446F8B49[] = {
	{ 1, IDParam((uint32_t)0xD0791765) },
	{ 12, IDParam((uint32_t)0x5049F5E4) },
	{ 18, IDParam((uint32_t)0x8EED7923) },
	{ 19, IDParam((uint32_t)0x71D8557B) },
	{ 0, IDParam() },
};

static Dialog::PartsList s_parts95087497[] = {
	{ 1, IDParam((uint32_t)0xD0791765) },
	{ 2, IDParam((uint32_t)0x66EA7A2F) },
	{ 12, IDParam((uint32_t)0x5049F5E4) },
	{ 18, IDParam((uint32_t)0x8EED7923) },
	{ 19, IDParam((uint32_t)0x71D8557B) },
	{ 0, IDParam() },
};

static Dialog::PartsList s_partsEA557911[] = {
	{ 1, IDParam((uint32_t)0xD0791765) },
	{ 2, IDParam((uint32_t)0x66EA7A2F) },
	{ 12, IDParam((uint32_t)0x5049F5E4) },
	{ 13, IDParam((uint32_t)0xBD559A97) },
	{ 18, IDParam((uint32_t)0x8EED7923) },
	{ 19, IDParam((uint32_t)0x71D8557B) },
	{ 0, IDParam() },
};

static Dialog::PartsList s_partsA0E0A437[] = {
	{ 1, IDParam((uint32_t)0xD0791765) },
	{ 18, IDParam((uint32_t)0x8EED7923) },
	{ 19, IDParam((uint32_t)0x71D8557B) },
	{ 0, IDParam() },
};

static Dialog::PartsList s_parts7AEA192C[] = {
	{ 1, IDParam((uint32_t)0xD0791765) },
	{ 0, IDParam() },
};

Dialog::PartsList g_partsInternal[] = {
	{ 9, IDParam((uint32_t)0xE9C61C2C) },
	{ 10, IDParam((uint32_t)0x019BC95D) },
	{ 0, IDParam() },
};

Dialog::PartsList g_partsE872FB5D[] = {
	{ 9, IDParam((uint32_t)0xE9C61C2C) },
	{ 10, IDParam((uint32_t)0x019BC95D) },
	{ 8, IDParam((uint32_t)0xC57612A0) },
	{ 0, IDParam() },
};

Dialog::PartsList g_parts1424609E[] = {
	{ 9, IDParam((uint32_t)0xE9C61C2C) },
	{ 10, IDParam((uint32_t)0x019BC95D) },
	{ 12, IDParam((uint32_t)0x5049F5E4) },
	{ 0, IDParam() },
};

Dialog::PartsList g_parts3AC06BC6[] = {
	{ 9, IDParam((uint32_t)0xE9C61C2C) },
	{ 10, IDParam((uint32_t)0x019BC95D) },
	{ 12, IDParam((uint32_t)0x5049F5E4) },
	{ 13, IDParam((uint32_t)0xBD559A97) },
	{ 0, IDParam() },
};

static Dialog::PartsList s_partsDDC59760[] = {
	{ 11, IDParam((uint32_t)0xD635CD6F) },
	{ 8, IDParam((uint32_t)0xC57612A0) },
	{ 0, IDParam() },
};

static Dialog::PartsList s_parts3785AD25[] = {
	{ 11, IDParam((uint32_t)0xD635CD6F) },
	{ 12, IDParam((uint32_t)0x5049F5E4) },
	{ 0, IDParam() },
};

static Dialog::PartsList s_parts7D8FE85E[] = {
	{ 5, IDParam((uint32_t)0x2A35777A) },
	{ 1, IDParam((uint32_t)0xD0791765) },
	{ 12, IDParam((uint32_t)0x5049F5E4) },
	{ 0, IDParam() },
};

static Dialog::PartsList s_partsC4A714DE[] = {
	{ 5, IDParam((uint32_t)0x2A35777A) },
	{ 1, IDParam((uint32_t)0xD0791765) },
	{ 12, IDParam((uint32_t)0x5049F5E4) },
	{ 13, IDParam((uint32_t)0xBD559A97) },
	{ 0, IDParam() },
};

static Dialog::PartsList s_parts52A53037[] = {
	{ 5, IDParam((uint32_t)0x2A35777A) },
	{ 1, IDParam((uint32_t)0xD0791765) },
	{ 0, IDParam() },
};

static Dialog::PartsList s_partsDEDA75E4[] = {
	{ 1, IDParam((uint32_t)0xD0791765) },
	{ 2, IDParam((uint32_t)0x66EA7A2F) },
	{ 12, IDParam((uint32_t)0x5049F5E4) },
	{ 0, IDParam() },
};

Dialog::PartsList g_parts04D71470[] = {
	{ 1, IDParam((uint32_t)0xD0791765) },
	{ 12, IDParam((uint32_t)0x5049F5E4) },
	{ 13, IDParam((uint32_t)0xBD559A97) },
	{ 23, IDParam((uint32_t)0xEEE519F6) },
	{ 0, IDParam() },
};

/* the layouts */
const CommonGuiDialogLayout SceCommonGuiDialog_AF75CA6D = {
	&g_pageAF75CA6D, g_partsAF75CA6D, g_okButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_EF7F04DD = {
	&g_pageAF75CA6D, g_partsAF75CA6D, s_cancelButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_6CD802FC = {
	&s_page6CD802FC, s_parts6CD802FC, g_noYesButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_22B17F2B = {
	&s_page22B17F2B, s_parts6CD802FC, s_yesNoButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_78399261 = {
	&s_page78399261, s_parts78399261, s_yesNoCancelButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_92752DE2 = {
	&s_page6CD802FC, s_parts6CD802FC, s_cancelOkButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_446F8B49 = {
	&s_page446F8B49, s_parts446F8B49, s_cancelButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_25BD4084 = {
	&s_page25BD4084, s_parts446F8B49, s_cancelButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_95087497 = {
	&s_page95087497, s_parts95087497, s_cancelButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_EA557911 = {
	&s_pageEA557911, s_partsEA557911, s_cancelButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_A0E0A437 = {
	&s_pageA0E0A437, s_partsA0E0A437, g_noButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_7AEA192C = {
	&s_page7AEA192C, s_parts7AEA192C, g_noButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_5C8A2999 = {
	&s_page5C8A2999, s_parts7AEA192C, g_noButtons
};

const CommonGuiDialogLayout g_contentsLayoutNoButtons = {
	&g_pageInternal, g_partsInternal, g_noButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_E872FB5D = {
	&g_pageE872FB5D, g_partsE872FB5D, g_panelButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_1424609E = {
	&g_page1424609E, g_parts1424609E, g_okButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_3AC06BC6 = {
	&g_page3AC06BC6, g_parts3AC06BC6, g_noYesButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_DDC59760 = {
	&s_pageDDC59760, s_partsDDC59760, g_panelButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_3785AD25 = {
	&s_page3785AD25, s_parts3785AD25, g_okButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_2A68B4A4 = {
	&s_page2A68B4A4, s_parts7AEA192C, g_noButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_F8B2D7E1 = {
	&s_pageF8B2D7E1, g_partsAF75CA6D, s_cancelButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_7D8FE85E = {
	&s_page7D8FE85E, s_parts7D8FE85E, g_okButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_C4A714DE = {
	&s_pageC4A714DE, s_partsC4A714DE, s_cancelOkButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_52A53037 = {
	&s_page52A53037, s_parts52A53037, g_noButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_B04C8C1A = {
	&s_pageB04C8C1A, s_parts7D8FE85E, g_okButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_DEDA75E4 = {
	&s_pageDEDA75E4, s_partsDEDA75E4, g_okButtons
};

const CommonGuiDialogLayout SceCommonGuiDialog_04D71470 = {
	&g_page04D71470, g_parts04D71470, g_04D71470Buttons
};

