/*
 * paf::common::transition: the animations that show and hide a widget, by
 * type, and the time each takes by default.
 *
 * Exported by NID only, so the functions are C functions under their link
 * names; ScePaf-RE's names (which hash to no NID) are given with each.
 */
#ifndef PAF_COMMON_TRANSITION_H
#define PAF_COMMON_TRANSITION_H

#include <stdint.h>

namespace paf {

namespace ui {
class Widget;
}

namespace common {
namespace transition {

enum Type {
	Type_FadeinFast = 0,
	Type_Popup1 = 1,
	Type_Popup2 = 2,
	Type_SlideFromBottom1 = 3,
	Type_SlideFromRight1 = 4,
	Type_FadeinSlow = 5,
	Type_SlideFromTop1 = 6,
	Type_SlideFromTop2 = 7,
	Type_SlideFromBottom2 = 8,
	Type_SlideFromBottom3 = 9,
	Type_Popup3 = 0xA,
	Type_Popup4 = 0xB,
	Type_Popup5 = 0xD,
	Type_SlideFromTop3 = 0xE,
	Type_SlideFromBottom4 = 0xF,
	Type_SlideFromLeft1 = 0x10,
	Type_SlideFromRight2 = 0x11,
	Type_SlideFromTop4 = 0x12,
	Type_SlideFromBottom5 = 0x13,
	Type_SlideFromLeft2 = 0x14,
	Type_SlideFromRight3 = 0x15,
	Type_3D_SlideFromFront = 0x16,
	Type_3D_SlideToBack1 = 0x17,
	Type_18 = 0x18,
	Type_3D_SlideToBack2 = 0x19,
	Type_Reset = 0xFF
};

}
}
}

extern "C" {
/* GetDefaultDelayReverse(Type, ui::Widget *) */
float ScePafCommon_C85E4F3A(paf::common::transition::Type type, paf::ui::Widget *widget);
/* Do(float parameter, ui::Widget *, Type, bool auto_delete, bool ignore_default_delay): shows */
float ScePafCommon_6CF3A3C8(float parameter, paf::ui::Widget *widget,
			    paf::common::transition::Type effect, bool auto_delete,
			    bool ignore_default_delay);
/* GetDefaultDelay(Type) */
float ScePafCommon_DE5322EB(paf::common::transition::Type type);
/* DoReverse(float parameter, ui::Widget *, Type, bool auto_delete, bool ignore_default_delay): hides */
float ScePafCommon_B48C9BDC(float parameter, paf::ui::Widget *widget,
			    paf::common::transition::Type effect, bool auto_delete,
			    bool ignore_default_delay);
}

#endif
