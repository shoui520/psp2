/*
 * near_auto_checkin's parameter: "@", the mode (MODE_*, Sony's names) and what the mode takes.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string>
#include <std/vector>
#include <common/string_util.h>
#include "near_checkin_lib.h"

using namespace paf;

/* a number in decimal */
static inline string NearNumberString(SceInt64 value)
{
	char buf[64];

	sce_paf_snprintf(buf, sizeof(buf), "%lld", value);
	return string(buf);
}

/* near_auto_checkin's parameter: "@", the mode and what the mode takes */
void NearMakeAutoCheckinParam(string &param, SceInt32 mode, const void *data)
{
	param.clear();
	param += "@";
	switch (mode) {
	case NearCheckinLib::MODE_AUTO_CHECKIN:
		param += "MODE_AUTO_CHECKIN";
		break;
	case NearCheckinLib::MODE_MANUAL_CHECKIN_LIVEAREA:
		param += "MODE_MANUAL_CHECKIN_LIVEAREA";
		break;
	case NearCheckinLib::MODE_MANUAL_CHECKIN_TOPMENU:
		param += "MODE_MANUAL_CHECKIN_TOPMENU";
		break;
	case NearCheckinLib::MODE_MANUAL_CHECKIN_PUBLIC:
		param += "MODE_MANUAL_CHECKIN_PUBLIC";
		break;
	case NearCheckinLib::MODE_MANUAL_CHECKIN_FRIEND:
		param += "MODE_MANUAL_CHECKIN_FRIEND";
		break;
	case NearCheckinLib::MODE_MANUAL_POST:
		param += "MODE_MANUAL_POST";
		param += ",";
		if (data != NULL) {
			const NearManualPostParam *post = (const NearManualPostParam *)data;
			int count;

			param.append(NearNumberString(post->id));
			param += ",";
			param += post->option1 ? "1" : "0";
			param += ",";
			param += post->option2 ? "1" : "0";
			param += ",";
			param += post->option3 ? "1" : "0";
			param += ",";
			count = post->items.size();
			param.append(NearNumberString(count));
			param += ",";
			for (int i = 0; i < count; i++) {
				common::String str;

				str.SetFormattedString("%llX",
						       ((vector<SceUInt64> &)post->items).at(i));
				param.append(str.GetString());
				param += ",";
			}
			count = post->gifts.size();
			param.append(NearNumberString(count));
			param += ",";
			for (int i = 0; i < count; i++) {
				common::String str;

				str.SetFormattedString("%llX",
						       ((vector<SceUInt64> &)post->gifts).at(i));
				param.append(str.GetString());
				param += ",";
			}
		}
		break;
	case NearCheckinLib::MODE_MANUAL_POST_EXIT:
		param += "MODE_MANUAL_POST_EXIT";
		break;
	case NearCheckinLib::MODE_CHECKIN_EXIT:
		param += "MODE_CHECKIN_EXIT";
		break;
	case NearCheckinLib::MODE_SDK_CHECKIN:
		param += "MODE_SDK_CHECKIN";
		param += ",";
		if (data != NULL) {
			param.append(*(const string *)data);
			param += ",";
		}
		break;
	case NearCheckinLib::MODE_SDK_CHECKIN_RETRY:
		param += "MODE_SDK_CHECKIN_RETRY";
		param += ",";
		if (data != NULL) {
			param.append(*(const string *)data);
			param += ",";
		}
		break;
	case NearCheckinLib::MODE_SDK_ITEM_DL:
		param += "MODE_SDK_ITEM_DL";
		param += ",";
		if (data != NULL) {
			const NearGiftDiscoveringId *id = (const NearGiftDiscoveringId *)data;
			common::String str;

			param.append(id->name);
			param += ",";
			param.append(id->code);
			param += ",";
			str.SetFormattedString("%llX", id->number);
			param.append(str.GetString());
			param += ",";
		}
		break;
	default:
		param = "";
		break;
	}
}

const char paf::s_empty_name[] = "";
