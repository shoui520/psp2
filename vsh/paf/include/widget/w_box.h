/*
 * paf::ui::Box: a plane that lays its children out by hand, in a column or in a row, sizes them
 * to fit it and itself to fit them. The classes, enums, functions and the strings of the
 * resource parameters are Sony's (export NIDs, ScePaf-RE's header after them, the parameter
 * names); the names of the members not named by a parameter are ours.
 */
#ifndef PAF_WIDGET_W_BOX_H
#define PAF_WIDGET_W_BOX_H

#include <stdint.h>
#include <widget/core/layouter.h>
#include <widget/w_plane.h>

namespace paf {
namespace ui {

class Box : public Plane {
public:
	enum LayoutType {
		LAYOUT_MANUAL = 0,
		LAYOUT_VERTICAL = 1,
		LAYOUT_HORIZONTAL = 2
	};

	enum ChildrenAdjust {
		CHILDREN_ADJUST_NONE = 0,
		CHILDREN_ADJUST_CONTENT_MAX = 1,
		CHILDREN_ADJUST_FIXED = 2,
		CHILDREN_ADJUST_FIT = 3,
		CHILDREN_ADJUST_MAX = 4
	};

	enum SpaceAdjust {
		SPACE_ADJUST_NONE = 0,
		SPACE_ADJUST_FIT = 1
	};

	enum ChildrenAdjustState {
		CHILDREN_ADJUST_INIT = 0,
		CHILDREN_ADJUST_WAIT = 1,
		CHILDREN_ADJUST_SET = 2,
		CHILDREN_ADJUST_DONE = 3
	};

	class LayoutVars {
	public:
		math::v4 min_size;
		math::v4 max_size;
		SpaceAdjust space_adjust;
		float space;
		float top_margin;
		float bottom_margin;
		float left_margin;
		float right_margin;
		/* the align its children take, 3 to leave theirs */
		uint32_t children_align_x;
		uint32_t children_align_y;
		bool boundary_mode_x;
		bool boundary_mode_y;
	};

	/* what lays a child out at the position, instead of SetPos_core (name ours) */
	typedef void (*SetChildPosCB)(Box *box, Widget *w, math::v4 pos);

	static const char m_widget_type[];
	/* the LayoutVars a box and its layout parameter start with (name ours) */
	static LayoutVars s_default_layout_vars;
	static const char *TypeName();

	Box(Widget *parent, CreateParam *param);
	virtual ~Box();

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual uint32_t CheckState();
	virtual uint32_t ChangeState(uint32_t state, int32_t ev_type);
	virtual bool RemoveChild(Widget *obj);
	virtual void RegisterChild(Widget *obj, Widget *sibling);
	virtual int32_t SetSize_core_force(const math::v4 &v, uint32_t notify_layout, bool force);
	virtual void UpdateSize();
	virtual void Update();
	virtual void UpdateAfterChildren();
	virtual math::v4 calc_origin_offset(const math::v4 &size, const math::v4 &offs,
					    uint32_t ax, uint32_t ay, uint32_t az);
	virtual math::v4 calc_anchor_offset(const math::v4 &size, const math::v4 &offs,
					    uint32_t ax, uint32_t ay, uint32_t az);
	virtual void UpdateAdjustSize();
	virtual void UpdateAdjustChildSize(Widget *child);

	void SetLayoutType(LayoutType layout_type);
	float GetFitSize(uint32_t align, float pos, float box_size, float box_origin_offset,
			 float margin1_size, float margin2_size) const;
	void AdjustChildSize(Widget *w, const math::v4 &max_child_size,
			     const math::v4 &total_child_size, uint32_t num_children,
			     uint32_t num_fit_children_x, uint32_t num_fit_children_y,
			     ChildrenAdjust children_adjust_x, ChildrenAdjust children_adjust_y);
	ChildrenAdjustState GetNextAdjustChildrenState(bool after_children_onupdate,
						       ChildrenAdjustState state,
						       ChildrenAdjust mode);
	void AdjustChildrenSize(bool after_children_onupdate);
	void UpdateLayoutSizePre(bool only_for_adjust_none);
	void UpdateLayoutSizePost(const math::v4 &boundary_size);
	math::v4 GetChildrenInfo();
	void UpdateBoxSize();
	void SetChildrenAdjustX(ChildrenAdjust mode, float width);
	void SetChildrenAdjustY(ChildrenAdjust mode, float height);
	void SetLayoutVars(const LayoutVars &layout_vars);
	void SetChildPos(Widget *w, math::v4 pos);
	void AddToExclusiveList(Widget *w);
	void DeleteFromExclusiveList(Widget *w);
	void GetBoundary(math::v4 *size, math::v4 *offs) const;

	void GetChildrenAdjustX(ChildrenAdjust *mode, float *width) const
	{
		*mode = m_children_adjust_x;
		if (width != NULL)
			*width = m_children_width;
	}

	void GetChildrenAdjustY(ChildrenAdjust *mode, float *height) const
	{
		*mode = m_children_adjust_y;
		if (height != NULL)
			*height = m_children_height;
	}

	LayoutType GetLayoutType() const
	{
		return m_layout_type;
	}

	const LayoutVars &GetLayoutVars() const
	{
		return m_layout_vars;
	}

	/* a child it lays out: not in its exclusive list, not held (name ours) */
	static bool IsLayoutChild(const Widget *w)
	{
		return w != NULL && !w->m_parent_protected && w->m_hold_count <= 0;
	}

	/* shown when it first goes to state 0 */
	bool m_auto_show : 1;
	SetChildPosCB m_cb_set_child_pos;
	/* the size its children are laid out in, the size of their boundary and its center */
	math::v4 m_layout_size;
	math::v4 m_boundary_size;
	math::v4 m_boundary_offs;
	/* its adjust as UpdateAdjustSize saw it last */
	uint32_t m_last_adjust_x : 4;
	uint32_t m_last_adjust_y : 4;
	ChildrenAdjustState m_children_adjust_state_x;
	ChildrenAdjustState m_children_adjust_state_y;
	ChildrenAdjust m_children_adjust_x;
	ChildrenAdjust m_children_adjust_y;
	float m_children_width;
	float m_children_height;
	LayoutType m_layout_type;
	LayoutVars m_layout_vars;

private:
	float calc_offset(uint32_t orign, float layout_size, float boundary_size);
	uint32_t anchor_to_orign(uint32_t anchor);
};

/* lays a box's children out each update, then sizes the box */
class BoxLayouter : public Layouter {
public:
	BoxLayouter() : Layouter(NULL, NULL)
	{
	}

	virtual ~BoxLayouter()
	{
	}

	virtual int32_t OnUpdate_core()
	{
		if (IsDirty(0xff)) {
			m_widget->ChangeState(3, -1);
			m_box = m_widget != NULL && m_widget->IsInherit(Box::m_widget_type) ?
					(Box *)m_widget : NULL;
			if (m_box != NULL && m_box->m_prepare_count == 0) {
				Layout();
				m_box->UpdateBoxSize();
				m_dirty = 0;
			}
		}
		return 0;
	}

	/* places the children (name ours) */
	virtual void Layout() = 0;

	Box *m_box;
};

/* a box's resource parameters: the names of the children it does not lay out */
class BoxParam : public PlaneParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	BoxParam();
	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Create(int32_t mode);

	/* names parted by commas, spaces or tabs */
	const char *m_exclusive_list;
};

/* a box's layout parameters: its layout, its LayoutVars and how it adjusts its children */
class BoxLayoutParam : public WidgetLayoutParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	BoxLayoutParam();
	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);

	Box::LayoutType m_layout_type;
	math::v4 m_min_size;
	math::v4 m_max_size;
	Box::SpaceAdjust m_space_adjust;
	float m_space;
	float m_top_margin;
	float m_bottom_margin;
	float m_left_margin;
	float m_right_margin;
	uint32_t m_children_align_x;
	uint32_t m_children_align_y;
	Box::ChildrenAdjust m_children_adjust_x;
	Box::ChildrenAdjust m_children_adjust_y;
	float m_children_width;
	float m_children_height;
	bool m_boundary_mode_x;
	bool m_boundary_mode_y;
};

}
}

#endif
