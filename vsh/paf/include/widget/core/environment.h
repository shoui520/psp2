/*
 * paf::ui::Environment, as far as converted C++ uses it: the widget system's
 * state for one framework.
 */
#ifndef PAF_WIDGET_CORE_ENVIRONMENT_H
#define PAF_WIDGET_CORE_ENVIRONMENT_H

#include <stddef.h>
#include <stdint.h>
#include <std/memory>
#include <std/string>
#include <std/list>
#include <math/ngp/v4.h>
#include <math/rect.h>
#include <versatileparam/idparam.h>
#include <graphics/surface/surface.h>

namespace paf {
namespace graph {
class Surface;
class DisplaySurface;
}

namespace ui {

/*
 * A render target: its clear colour, viewport and scissor, and its surface. The
 * viewport and scissor are rectangles here, where older builds kept four ints
 * each (ScePaf-RE's vp_* and sp_*).
 */
class Screen {
public:
	Screen();
	Screen(intrusive_ptr<graph::Surface> surface);
	virtual ~Screen();

	static Screen *GetRootScreen()
	{
		return s_root_screen;
	}

	/* the surface made the target of the draws that follow, and the draws finished */
	void OnTarget();
	void OnFinish();

	intrusive_ptr<graph::Surface> GetSurface()
	{
		return m_surface;
	}

	int32_t padding;
	math::v4 clear_color;
	bool is_auto_delete;
	bool is_cleared;
	math::rect vp;
	math::rect sp;
	int32_t clear_policy;
	intrusive_ptr<graph::Surface> m_surface;
	uint8_t m_whandle[0xc];
	int32_t reserved[2];

	static Screen *s_root_screen;
	/* where widgets whose camera does not roll the screen are drawn from */
	static math::v4 s_draw_offset;
	/* the root screen's surface */
	static graph::DisplaySurface *s_root_surface;
};

class Widget;
class SceneResolutionList;

/*
 * What the resource of a resolution is read with: the name of its strings' and
 * textures' variant and its value (names ours).
 */
class ResolutionResource {
public:
	ResolutionResource() : m_name(s_empty_name), m_value(0)
	{
	}

	string m_name;
	uint32_t m_value;
};

/* the screen resolutions the layouts are made for, and the one to use */
class EnvironmentParam {
public:
	EnvironmentParam(SceneResolutionList *resolution_table = NULL);
	virtual ~EnvironmentParam();
	/* the layout resolution for a screen of 1920x1088 or more */
	virtual uint32_t GetResolutionIdHD(uint32_t width, uint32_t height, int32_t orientation);
	/* the layout resolution for a screen type below 4 */
	virtual uint32_t GetResolutionId(uint32_t type, int32_t orientation);
	virtual EnvironmentParam *Clone() const;

	/* the largest resolution the display fits */
	void GetScreenSize(uint32_t *width, uint32_t *height);

	SceneResolutionList *m_resolution_list;
	uint8_t reserved[0xc];
};

class Environment {
public:
	typedef int32_t (*SoundCallback)(const char *buffer1, uint32_t size1, const char *buffer2,
					 uint32_t size2, int32_t samplerate, float vol,
					 bool ignore_init, void *data);
	typedef Widget *(*CreateWidgetCallback)(Widget *parent, const char *type,
						const IDParam &id, const IDParam &style,
						void *data);

	virtual ~Environment();

	/* whether nothing the widgets still run keeps the framework from ending */
	bool IsTermAvailable();
	/* lets the widgets end: m_term_requested once nothing keeps them */
	int32_t RequestTerm();
	/* a frame's steps, in the order the Framework runs them */
	void OnUpdatePre();
	void EventLoop();
	void OnUpdatePost();
	void ShowFocus(int32_t port);
	/* the display area scale from the system settings, once */
	void ApplyDisplayAreaSettings();

	uint8_t reserved[0xb0];
	/* the Framework's InitParam::screen_orientation_mode and env_param */
	int32_t m_screen_orientation_mode;
	uint8_t reserved0[4];
	EnvironmentParam *m_env_param;
	uint8_t reserved7[0x15];
	/* set by RequestTerm */
	bool m_term_requested;
	uint8_t reserved1[2];
	/* the Framework's InitParam::env_option */
	bool m_env_option;
	uint8_t reserved2[3];
	SoundCallback m_sound_callback;
	void *m_sound_callback_data;
	CreateWidgetCallback m_create_widget_callback;
	void *m_create_widget_callback_data;
	uint8_t reserved3[0x68];
	/* set before the focus is first shown */
	int32_t m_focus_mode;
	uint8_t reserved4[0x1c8];
	/* the scenes, updated each frame (name after ScePaf-RE) */
	list<Widget *> m_scenes_onupdate;
	/* the screen SetScreen gives */
	Screen *m_screen;
	uint8_t reserved5[0x2a3a];
	/* each frame waits for the GPU once it is drawn (Framework::Option_4) */
	bool m_finish_after_draw;
	uint8_t reserved6[0x35];
};

}
}

/* Exports with no name found for their NIDs. */
extern "C" {
/* the resource variant of the parameter's resolution with the ID, or the empty one */
paf::ui::ResolutionResource ScePafWidget_E963EAEE(const paf::ui::EnvironmentParam *param,
						  uint32_t id);
/* constructs the Environment at mem */
paf::ui::Environment *ScePafWidget_393F0EF0(void *mem, paf::ui::EnvironmentParam *param,
					    int32_t screen_orientation_mode);
/* whether the buttons move the focus */
void ScePafWidget_75FB31F1(paf::ui::Environment *env, bool enable);
void ScePafWidget_B4A4660B(paf::ui::Environment *env, int32_t enable, int32_t port, int32_t mode);
/* whether the port's focus may be shown */
bool ScePafWidget_A6B659BB(paf::ui::Environment *env, int32_t port);
/* updates the input devices */
int32_t ScePafWidget_909850C4(void);
/* sets the focus frame texture no (below 9); with none, reads all of them from the common resource */
void ScePafWidget_B6C64486(const paf::intrusive_ptr<paf::graph::Surface> &texture, int32_t no);
/* begins a frame on the root surface */
void ScePafWidget_9D4A5A0E(paf::ui::Environment *env);
void ScePafWidget_DC1B19DB(paf::ui::Environment *env);
/* ends the frame on the root surface */
void ScePafWidget_851911CB(paf::ui::Environment *env);
/* shows the frame drawn; below 0 on failure */
int32_t ScePafWidget_F67517A0(paf::ui::Environment *env);
void ScePafWidget_8E61FE0C(paf::ui::Environment *env);
/* always 0 */
int32_t ScePafWidget_998F8F48(void);
/* loads the effects whose bits are set from the folder */
int32_t ScePafWidget_BA6EB0C6(uint32_t effects, const paf::string &dir);
}

#endif
