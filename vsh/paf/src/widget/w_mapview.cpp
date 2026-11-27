/*
 * paf::ui::MapView ("mapview") implementation for 2014 PAF.
 */
#include <widget/w_mapview.h>
#include <widget/factory.h>
#include <widget/map_core.h>
#include <versatileparam/resourceparam.h>

namespace paf {
namespace ui {

const char MapView::m_widget_type[] = "mapview";
const char MapViewParam::m_param_name[] = "map_view_base";
const char MapViewStyleParam::m_param_name[] = "style_map_view";

static float s_default_map_height;
static float s_default_map_width;
static MapGeoCoord s_invalid_geo_coord;

const char *MapView::TypeName()
{
	return m_widget_type;
}

bool MapView::IsInherit(const char *type) const
{
	if (type == m_widget_type)
		return true;
	return Widget::IsInherit(type);
}

char *MapView::GetTypeName() const
{
	return const_cast<char *>(m_widget_type);
}

MapView::MapView(Widget *parent, CreateParam *param)
	: Widget(parent, param),
	  m_touched_coord(),
	  m_listener(NULL),
	  m_map_flags(0),
	  m_map_core(NULL)
{
	m_state = (m_state & ~0xFU) | 1U;

	math::v4 size = *GetSize(GET_CURRENT);

	if (size.extract_x() == 0.0f && size.extract_y() == 0.0f) {
		s_default_map_width = 960.0f;
		s_default_map_height = 540.0f;
		SetSize(math::v4(960.0f, 540.0f, 0.0f, 0.0f), NULL, ANIM_SIZE, 0, 0.0f, NULL);
	}

	m_listener = NULL;
	m_map_core = new MapCore(this);
	MapLocationManager::Bind(this);
}

MapView::~MapView()
{
}

void MapView::DestroyWidget()
{
	Widget::DestroyWidget();
}

int32_t MapView::DrawDirectionSteps(MapDirStepList *steps, const char *from_name,
				    const char *to_name, double from_lat, double from_lon,
				    double to_lat, double to_lon, int32_t mode,
				    int32_t step_index, int32_t icon_id,
				    int32_t route_flags, int32_t options)
{
	if (m_map_core != NULL)
		return m_map_core->DrawDirectionSteps(steps, from_name, to_name, from_lat,
						      from_lon, to_lat, to_lon, mode,
						      step_index, icon_id, route_flags, options);
	return (int32_t)(uintptr_t)to_name;
}

int32_t MapView::NotifyMarkerDragEvent(int32_t event_kind, const MapPixelPoint *pos,
				       int32_t marker_id)
{
	(void)event_kind;
	(void)pos;
	(void)marker_id;
	return 0;
}

void MapView::NotifyMapIdle()
{
}

void MapView::NotifyMarkerClick(const MapPixelPoint *pos, int32_t marker_id, int32_t flag_id)
{
	(void)pos;
	(void)marker_id;
	(void)flag_id;
}

void MapView::NotifyTileLoadFinish()
{
}

void MapView::NotifyTileLoadError()
{
}

void MapView::NotifyMapReady()
{
}

void MapView::NotifyZoomCommit(int32_t zoom_level, int32_t status)
{
	(void)zoom_level;
	(void)status;
}

MapViewParam::MapViewParam()
	: WidgetParam()
{
}

MapViewParam::~MapViewParam()
{
}

const char *MapViewParam::ParamName()
{
	return m_param_name;
}

const char *MapViewParam::GetParamName() const
{
	return m_param_name;
}

int32_t MapViewParam::Set()
{
	return WidgetParam::Set();
}

int32_t MapViewParam::Create(int32_t mode)
{
	return WidgetParam::Create(mode);
}

const char *MapViewStyleParam::ParamName()
{
	return m_param_name;
}

const char *MapViewStyleParam::GetParamName() const
{
	return m_param_name;
}

static Widget *CreateMapView(Widget *parent)
{
	return new MapView(parent, NULL);
}

static WidgetParam *CreateMapViewParam()
{
	return new MapViewParam();
}

static VersatileParam *CreateMapViewLayoutParam()
{
	return new WidgetLayoutParam();
}

static VersatileParam *CreateMapViewStyleParam()
{
	return new MapViewStyleParam();
}

static VersatileParam *CreateMapViewHandlerParam()
{
	return new WidgetHandlerParam();
}

static VersatileParam *CreateMapViewSoundParam()
{
	return new WidgetSoundParam();
}

}
}

paf::ui::FactoryFuncTable g_factoryMapView = {
	PAF_FACTORY_TYPE_NAME(paf::ui::MapView),
	paf::ui::CreateMapView,
	paf::ui::CreateMapViewParam,
	paf::ui::CreateMapViewLayoutParam,
	paf::ui::CreateMapViewStyleParam,
	paf::ui::CreateMapViewHandlerParam,
	paf::ui::CreateMapViewSoundParam
};
