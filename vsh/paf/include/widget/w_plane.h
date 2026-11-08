/*
 * paf::ui::Plane: a widget drawn as one plane object (draw object 0), and its resource
 * parameters. The classes, their functions and TypeName are Sony's (the export NIDs hash as
 * them); the members' names are ours.
 */
#ifndef PAF_WIDGET_W_PLANE_H
#define PAF_WIDGET_W_PLANE_H

#include <stdint.h>
#include <graphics/drawobj/plane_obj.h>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

class Plane : public Widget {
public:
	static const char m_widget_type[];
	static const char *TypeName();

	Plane(Widget *parent, CreateParam *param);
	virtual ~Plane();

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual uint32_t CheckState();
	virtual void UpdateSize();
	virtual void Update();
	virtual void UpdateAdjustSize();
	virtual common::SharedPtr<graph::DrawObj> SwapDrawObj(common::SharedPtr<graph::DrawObj> obj,
							      int32_t id);
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);
	virtual int32_t GetTexture(intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);

	/* Widget's, and the down direction while it listens to event 0x10001 */
	virtual int32_t GetFocusDirectionMask()
	{
		int32_t mask = Widget::GetFocusDirectionMask();

		if (mask == 0 && m_map_e_listener.find(0x10001) != m_map_e_listener.end())
			return 2;
		return mask;
	}

	/* the plane, draw object 0 */
	graph::PlaneObj *m_plane;
};

/* a plane's resource parameters: hit_ignore, which sets the widget's hit detect modes to 2
   (1) or 0 (0); -1 leaves them */
class PlaneParam : public WidgetParam {
public:
	PlaneParam() : m_hit_ignore(-1)
	{
	}

	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);

	int32_t m_hit_ignore;
};

/* a plane's style: its plane object's parameter */
class PlaneStyleParam : public WidgetStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	PlaneStyleParam();
	virtual const char *GetParamName() const;

	graph::PlaneObjParam m_plane_param;
};

}
}

#endif
