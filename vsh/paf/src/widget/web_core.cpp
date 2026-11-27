/*
 * paf::ui::WebCore implementation for 2014 PAF WebView/MapView.
 */
#include <widget/web_core.h>
#include <widget/web_controller.h>

namespace paf {
namespace ui {

WebCore *WebCore::s_instance = NULL;

WebCore *WebCore::GetInstance()
{
	return s_instance;
}

int32_t WebCore::Shutdown(bool notify_restart)
{
	if (!m_active)
		return 0;

	m_active = false;

	for (size_t i = 0; i < m_controllers.size(); i++) {
		WebController *controller = m_controllers[i];
		if (controller->IsWakeup())
			controller->Sleep(!notify_restart);
	}

	if (m_engine != NULL) {
		m_engine->Shutdown();
		m_engine = NULL;
	}

	if (m_resource_client != NULL) {
		m_resource_client->Finalize();
		delete m_resource_client;
		m_resource_client = NULL;
	}

	if (m_process != NULL) {
		m_process->Terminate();
		m_process = NULL;
	}

	if (m_event_listener != NULL)
		m_event_listener->OnShutdown(this);

	m_initialized = false;
	return 0;
}

void WebCore::SetSuspendFlag(bool flag)
{
	m_suspend_flag = flag;
}

bool WebCore::GetSuspendFlag() const
{
	return m_suspend_flag;
}

void WebCore::NotifyControllerWakeup(WebController *sender)
{
	for (size_t i = 0; i < m_controllers.size(); i++) {
		if (m_controllers[i] != sender)
			m_controllers[i]->OnOtherControllerWakeup();
	}
}

int32_t WebCore::RemoveController(WebController *controller)
{
	for (vector<WebController *>::iterator it = m_controllers.begin();
	     it != m_controllers.end(); ++it) {
		if (*it == controller) {
			vector<WebController *>::iterator next = it + 1;
			m_controllers.erase(it, next);
			break;
		}
	}
	return (int32_t)m_controllers.size();
}

}
}
