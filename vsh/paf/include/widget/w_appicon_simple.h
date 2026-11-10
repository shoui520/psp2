/*
 * paf::ui::AppIconSimple: an icon plane masked by the common icon mask, its highlight frame
 * over it, and the textures they share. The class, TypeName and the functions the export NIDs
 * hash as are Sony's; the members are ours.
 */
#ifndef PAF_WIDGET_W_APPICON_SIMPLE_H
#define PAF_WIDGET_W_APPICON_SIMPLE_H

#include <stdint.h>
#include <graphics/drawobj/plane_obj.h>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

class AppIconSimple : public Widget {
public:
	static const char m_widget_type[];
	static const char *TypeName();

	AppIconSimple(Widget *parent, CreateParam *param);
	virtual ~AppIconSimple();

	virtual int32_t HandleGestureEvent(Event *e);
	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual uint32_t CheckState();
	virtual void UpdateSize();
	virtual void Update();
	virtual void OnTouchCaptureChanged(uint32_t port, uint32_t id, int32_t arg);
	virtual void UpdateAdjustSize();
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);
	virtual int32_t GetTexture(intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);

	/* the widget's mask, else one way when something listens for 0x10000001 and up and down
	   without */
	virtual int32_t GetFocusDirectionMask()
	{
		int32_t mask = Widget::GetFocusDirectionMask();

		if (mask != 0)
			return mask;
		return m_map_e_listener.find(0x10000001) != m_map_e_listener.end() ? 2 : 3;
	}

	/* taps make it shrink and spring back (Sony's name from the export) */
	int32_t EnableTap(bool on_off);
	/* 0, 2 or 3: the icon drops draw object 1; 2 shows it unmasked, 0 framed (Sony's name
	   from the export) */
	void SetAppearance(uint32_t appearance);

	/* the highlight frame (draw object 2) and the icon (draw object 0) */
	graph::PlaneObj *m_frame;
	graph::PlaneObj *m_icon;
	/* draw object 1, which SetAppearance removes */
	graph::DrawObj *m_overlay;
	/* the icon's texture */
	intrusive_ptr<graph::Surface> m_texture;
	/* what EnableTap set */
	bool m_tap_enabled;
};

/* an AppIconSimple's resource parameters: whether taps are enabled, and its appearance */
class AppIconSimpleParam : public WidgetParam {
public:
	static const char m_param_name[];

	AppIconSimpleParam() : m_enable_tap(false), m_appearance(0)
	{
	}

	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Create(int32_t mode);

	bool m_enable_tap;
	uint32_t m_appearance;
};

}
}

#endif
