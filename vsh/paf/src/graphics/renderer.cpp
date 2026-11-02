/*
 * The renderer (the file's name ours): the renderers of the effects, made when they are first
 * asked for, the backgrounds they are drawn over, and the layers of the draw calls that capture
 * the screen for the reflections, the refractions and the blur.
 */
#include <stdint.h>
#include <graphics/drawobj/effect_path.h>
#include <graphics/color.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/camera.h>
#include <graphics/drawobj/draw_env.h>
#include <graphics/effects/background.h>
#include <graphics/effects/post_effects.h>
#include <graphics/effects/fft_wave.h>
#include <inputdevice/inputlistener.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <src/common/frame_count.h>

namespace paf {
namespace graph {

/* the folder the renderers load their resources from: the one given, else the firmware's */
static inline const string &ResourcePath(const string &path)
{
	return path.empty() ? s_effects_path : path;
}

void Renderer::DrawDummyBackground()
{
	if (m_backgrounds.get() != NULL)
		m_backgrounds->Draw();
}

void Renderer::CaptureBlur()
{
	arl::ArlInstance *gl = arl::Gl();

	m_blur.Capture(*arl::Gl()->GetCopyTexture(0), ScePafGraphics_641E3FAE(gl),
		       math::v2(0.0f, 1.0f));
}

void Renderer::DrawBlur()
{
	m_blur.Draw(m_blur_amount);
}

void Renderer::DrawNothing()
{
}

Renderer::Renderer(common::SharedPtr<DrawEnv> draw_env)
	: m_frame(-1), m_reserved_frame(-1), m_blur_amount(0.0f), m_reserved_flag(false),
	  m_rs_world_grid(NULL), m_rs_dummy_background(NULL), m_rs_capture_reflections(NULL),
	  m_rs_copy_screen_for_refractions(NULL), m_rs_capture_blur(NULL), m_reserved_word(0),
	  m_effect_renderer_factory(NULL), m_custom_renderer_factory(NULL)
{
	m_draw_env = draw_env;

	m_listener = common::SharedPtr<MotionListener>(new MotionListener());
	{
		common::SharedPtr<inputdevice::InputListener> listener = m_listener;

		inputdevice::AddInputListener(listener);
	}

	m_camera = common::SharedPtr<Camera>(new Camera(Camera::FLAG_2D));
	m_camera_2d = common::SharedPtr<Camera>(new Camera(*m_camera));
	m_post_effects = common::SharedPtr<PostEffects>(new PostEffects());
	m_blur.SetPostEffects(m_post_effects);
	m_shared_texture = common::SharedPtr<SharedTexture>(new SharedTexture());

	m_backgrounds = common::SharedPtr<BackgroundList>(new BackgroundList());
	m_backgrounds->Add(common::SharedPtr<EmptyBackground>(new EmptyBackground(0)));
	m_backgrounds->Add(common::SharedPtr<ProbeBackground>(new ProbeBackground()));
	m_backgrounds->Init(m_camera_2d, m_shared_texture);
	m_backgrounds->Load(ResourcePath(m_resource_path));
	m_backgrounds->Select(m_backgrounds->m_backgrounds.size() - 1);
	m_background = m_backgrounds;

	m_env = common::SharedPtr<EnvAuto>(new EnvAuto());
	m_env->Init(m_camera_2d, m_shared_texture);
	m_env->SetPostEffects(m_post_effects);
	m_env->Load(ResourcePath(m_resource_path));
	m_background = m_env;

	m_rs_world_grid = m_draw_env->AddLayer("m_rs_world_grid", 101, 2, 0x4000);
	m_rs_world_grid->Push(0x40000000, 0, &Camera::DrawNothing, m_camera_2d.get());
	m_rs_world_grid->m_reserved_value = 2;

	m_rs_dummy_background = m_draw_env->AddLayer("m_rs_dummy_background", 10, 2, 0x4000);
	m_rs_dummy_background->Push(0x40000000, 0, &Renderer::DrawDummyBackground, this);
	m_rs_dummy_background->m_reserved_value = 2;

	m_rs_capture_reflections = m_draw_env->AddLayer("m_rs_capture_reflections", 200, 2, 0x4000);
	m_rs_capture_reflections->Push(0x40000000, 0, &EnvAuto::captureReflections, m_env.get());
	m_rs_capture_reflections->m_reserved_value = 2;

	m_rs_copy_screen_for_refractions =
		m_draw_env->AddLayer("m_rs_copy_screen_for_refractions", 202, 2, 0x4000);
	m_rs_copy_screen_for_refractions->Push(0x40000000, 0, &EnvAuto::CopyScreenForRefractions,
					       m_env.get());
	m_rs_copy_screen_for_refractions->m_reserved_value = 2;

	m_rs_capture_blur = m_draw_env->AddLayer("m_rs_capture_blur", 500, 2, 0x4000);
	m_rs_capture_blur->Push(0x40000000, 0, &Renderer::CaptureBlur, this);
	m_rs_capture_blur->Push(0x50000000, 0, &Renderer::DrawBlur, this);
	m_rs_capture_blur->m_reserved_value = 2;

	arl::ArlInstance *gl = arl::Gl();
	arl::RenderTargetPool::Key small = { 64, 64, 0 };
	arl::RenderTargetPool::Key large = { 128, 128, 0 };

	gl->AddScenes(small, 4);
	gl->AddScenes(large, 1);
}

common::SharedPtr<PanelRenderer> &Renderer::GetPanelRenderer(bool create)
{
	if (m_panel_renderer.get() == NULL && create) {
		m_panel_renderer = common::SharedPtr<PanelRenderer>(new PanelRenderer());
		m_panel_renderer->Init(m_camera, m_camera_2d, m_background, m_draw_env);
		m_panel_renderer->Load(ResourcePath(m_resource_path));
	}
	return m_panel_renderer;
}

common::SharedPtr<WaveRenderer> &Renderer::GetWaveRenderer(bool create)
{
	if (m_wave_renderer.get() == NULL && create) {
		m_wave_renderer = common::SharedPtr<WaveRenderer>(new WaveRenderer());
		m_wave_renderer->Init(m_camera_2d, m_background, m_draw_env, m_post_effects,
				      m_listener);
		m_wave_renderer->Load(ResourcePath(m_resource_path), 1);
	}
	return m_wave_renderer;
}

common::SharedPtr<CustomRenderer> &Renderer::GetCustomRenderer(bool create)
{
	if (m_custom_renderer.get() == NULL && create) {
		if (m_custom_renderer_factory == NULL)
			common_internal::on_error();
		m_custom_renderer = common::SharedPtr<CustomRenderer>(m_custom_renderer_factory());
		m_custom_renderer->Init(m_camera_2d, m_draw_env);
		m_custom_renderer->Load(ResourcePath(m_resource_path), 1024,
					ResourcePath(m_resource_path), 256);
	}
	return m_custom_renderer;
}

common::SharedPtr<PageTurnRenderer> &Renderer::GetPageTurnRenderer(bool create)
{
	if (m_page_turn_renderer.get() == NULL && create)
		m_page_turn_renderer = common::SharedPtr<PageTurnRenderer>(
			new PageTurnRenderer(ResourcePath(m_resource_path)));
	return m_page_turn_renderer;
}

}
}

using namespace paf;
using namespace paf::graph;

common::SharedPtr<EffectRenderer> &ScePafGraphics_71E2CB77(Renderer *renderer, bool create)
{
	if (renderer->m_effect_renderer.get() == NULL && create) {
		if (renderer->m_effect_renderer_factory == NULL)
			common_internal::on_error();
		renderer->m_effect_renderer =
			common::SharedPtr<EffectRenderer>(renderer->m_effect_renderer_factory());
		renderer->m_effect_renderer->Init(renderer->m_camera, renderer->m_background,
						  renderer->m_draw_env, renderer->m_post_effects);
		renderer->m_effect_renderer->Load(ResourcePath(renderer->m_resource_path));
	}
	return renderer->m_effect_renderer;
}

namespace paf {
namespace graph {

void Renderer::SetRoll(float roll)
{
	m_camera->SetRoll(roll);
}

void Renderer::Update()
{
	m_frame = common::FrameCount();

	arl::ArlInstance *gl = arl::Gl();

	m_camera->UpdateAspect();
	*m_camera_2d = *m_camera;
	if (m_wave_renderer.get() != NULL && m_wave_renderer->m_tilt)
		m_camera_2d->m_transform.SetWorld(GetWaveRenderer(false)->GetTiltedWorld(
			m_camera_2d->GetResetWorld()));
	if (m_panel_renderer.get() != NULL)
		m_panel_renderer->Update();
	if (m_page_turn_renderer.get() != NULL)
		m_page_turn_renderer->Update();
	if (m_custom_renderer.get() != NULL) {
		if (m_wave_renderer.get() != NULL) {
			m_custom_renderer->m_wave_x = m_wave_renderer->m_wave_x;
			m_custom_renderer->m_wave_y = m_wave_renderer->m_wave_y;
		} else {
			m_custom_renderer->m_wave_x = 0.0f;
			m_custom_renderer->m_wave_y = 0.0f;
		}
	}
	if (m_effect_renderer.get() != NULL)
		m_effect_renderer->Update(gl->m_frame_time);
	if (m_fft_wave.get() != NULL) {
		m_fft_wave->Update();
		m_fft_wave->m_view = GetPanelRenderer(false)->GetViewMatrix();
		m_fft_wave->m_projection = GetPanelRenderer(false)->GetProjectionMatrix();
	}
}

}
}
