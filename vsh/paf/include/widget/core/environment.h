/*
 * paf::ui::Environment, as far as converted C++ uses it: the widget system's
 * state for one framework.
 */
#ifndef PAF_WIDGET_CORE_ENVIRONMENT_H
#define PAF_WIDGET_CORE_ENVIRONMENT_H

#include <stddef.h>
#include <stdint.h>
#include <std/memory>
#include <math/ngp/v4.h>
#include <math/rect.h>
#include <versatileparam/idparam.h>

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
	virtual ~Screen();

	static Screen *GetRootScreen()
	{
		return s_root_screen;
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

/* the screen resolutions the layouts are made for, and the one to use */
class EnvironmentParam {
public:
	EnvironmentParam(void *resolution_table = NULL);
	virtual ~EnvironmentParam();
	/* the layout resolution for a screen of 1920x1088 or more */
	virtual uint32_t GetResolutionIdHD(uint32_t width, uint32_t height, int32_t orientation);
	/* the layout resolution for a screen type below 4 */
	virtual uint32_t GetResolutionId(uint32_t type, int32_t orientation);
	virtual EnvironmentParam *Clone() const;

	/* the largest resolution the display fits */
	void GetScreenSize(uint32_t *width, uint32_t *height);

	uint8_t reserved[0x10];
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
	void ShowFocus(int32_t port);
	/* the display area scale from the system settings, once */
	void ApplyDisplayAreaSettings();

	uint8_t reserved[0xd4];
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
	uint8_t reserved4[0x2c12];
	/* each frame waits for the GPU once it is drawn (Framework::Option_4) */
	bool m_finish_after_draw;
	uint8_t reserved5[0x35];
};

}
}

/* Exports with no name found for their NIDs. */
extern "C" {
/* constructs the Environment at mem */
paf::ui::Environment *ScePafWidget_393F0EF0(void *mem, paf::ui::EnvironmentParam *param,
					    int32_t screen_orientation_mode);
/* whether the buttons move the focus */
void ScePafWidget_75FB31F1(paf::ui::Environment *env, bool enable);
void ScePafWidget_B4A4660B(paf::ui::Environment *env, int32_t enable, int32_t port, int32_t mode);
/* whether the port's focus may be shown */
bool ScePafWidget_A6B659BB(paf::ui::Environment *env, int32_t port);
}

#endif
