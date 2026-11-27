/*
 * paf::graph::WebBGObj: tiled background plane object for 2014 PAF WebRenderer.
 */
#ifndef PAF_GRAPHICS_DRAWOBJ_WEB_BG_OBJ_H
#define PAF_GRAPHICS_DRAWOBJ_WEB_BG_OBJ_H

#include <graphics/drawobj/plane_obj.h>
#include <widget/web_rect.h>

namespace paf {
namespace graph {

class WebBGObj : public PlaneObj {
public:
	static const char *DrawObjType();
	static const char m_drawobj_type[];

	WebBGObj(const DrawStyle &style);
	virtual bool IsInherit(const char *type) const;
	virtual const char *GetDrawObjType() const;
	virtual ~WebBGObj();
	virtual void SetSize(math::v3 size);

	int32_t UpdateTexRegion(ui::WebRect rect);
};

}
}

#endif
