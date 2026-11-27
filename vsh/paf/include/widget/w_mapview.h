/*
 * paf::ui::MapView ("mapview"): 2014 PAF interactive map widget wrapping MapCore,
 * MapViewListener, and resource/style parameter classes ("map_view_base", "style_map_view").
 */
#ifndef PAF_WIDGET_W_MAPVIEW_H
#define PAF_WIDGET_W_MAPVIEW_H

#include <stdint.h>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

class MapCore;
class MapView;
struct MapDirStepList;

/* Geographic coordinate (latitude/longitude in degrees; -200.0 means unset). */
class MapGeoCoord {
public:
#ifdef PAF_BUILD_2014
	__attribute__((always_inline)) MapGeoCoord() : m_latitude(-200.0), m_longitude(-200.0)
#else
	MapGeoCoord() : m_latitude(-200.0), m_longitude(-200.0)
#endif
	{
	}

	~MapGeoCoord()
	{
		m_latitude = -200.0;
		m_longitude = -200.0;
	}

	double m_latitude;
	double m_longitude;
};

/* Screen-space integer pixel coordinate used in MapView touch/zoom callbacks. */
struct MapPixelPoint {
	int32_t m_x;
	int32_t m_y;
};

/* Application callback interface registered on MapView (16 virtual slots). */
class MapViewListener {
public:
	MapViewListener()
	{
	}

	virtual ~MapViewListener()
	{
	}

	virtual void OnMapLoadStart(MapView *view)
	{
	}

	virtual void OnMapLoadFinish(MapView *view)
	{
	}

	virtual void OnTouchBegin(MapView *view, const MapPixelPoint *pos, uint32_t user_data)
	{
	}

	virtual void OnTouchEnd(MapView *view, const MapPixelPoint *pos, uint32_t user_data)
	{
	}

	virtual void OnTouchMove(MapView *view, const MapPixelPoint *pos)
	{
	}

	virtual void OnDragBegin(MapView *view, const MapPixelPoint *pos)
	{
	}

	virtual void OnDragMove(MapView *view, const MapPixelPoint *pos)
	{
	}

	virtual void OnDragEnd(MapView *view, const MapPixelPoint *pos)
	{
	}

	virtual void OnMarkerSelected(MapView *view, int32_t marker_id)
	{
	}

	virtual void OnZoomLevelChanged(MapView *view, int32_t zoom_level)
	{
	}

	virtual void OnCenterChanged(MapView *view, const MapGeoCoord *center)
	{
	}

	virtual void OnBoundsChanged(MapView *view)
	{
	}

	virtual void OnPinchEnd(MapView *view, const MapPixelPoint *pos)
	{
	}

	virtual void OnSingleTap(MapView *view)
	{
	}
};

/* Direction routing parameters forwarded to MapCore::DrawDirectionSteps. */
struct MapDirectionArgs {
	MapDirStepList *m_steps;
	const char *m_from_name;
	const char *m_to_name;
	double m_from_lat;
	double m_from_lon;
	double m_to_lat;
	double m_to_lon;
	int32_t m_mode;
	int32_t m_step_index;
	int32_t m_icon_id;
	int32_t m_route_flags;
	int32_t m_options;
};

class MapView : public Widget {
public:
	static const char m_widget_type[];
	static const char *TypeName();

	MapView(Widget *parent, CreateParam *param);
	virtual ~MapView();

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void DestroyWidget();

	virtual int32_t DrawDirectionSteps(MapDirStepList *steps, const char *from_name,
					   const char *to_name, double from_lat,
					   double from_lon, double to_lat, double to_lon,
					   int32_t mode, int32_t step_index, int32_t icon_id,
					   int32_t route_flags, int32_t options);

	/* Internal hooks called by MapCore's JS event handler. */
	int32_t NotifyMarkerDragEvent(int32_t event_kind, const MapPixelPoint *pos,
				      int32_t marker_id);
	void NotifyMapIdle();
	void NotifyMarkerClick(const MapPixelPoint *pos, int32_t marker_id, int32_t flag_id);
	void NotifyTileLoadFinish();
	void NotifyTileLoadError();
	void NotifyMapReady();
	void NotifyZoomCommit(int32_t zoom_level, int32_t status);

	uint8_t padding[20];
	MapGeoCoord m_touched_coord;
	uint32_t reserved;
	MapViewListener *m_listener;
	uint32_t m_map_flags;
	MapCore *m_map_core;
};

/* Resource parameters for MapView ("map_view_base"). */
class MapViewParam : public WidgetParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	MapViewParam();
	virtual ~MapViewParam();

	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Create(int32_t mode);
};

/* Style parameters for MapView ("style_map_view"). */
class MapViewStyleParam : public WidgetStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	MapViewStyleParam()
	{
	}

	virtual const char *GetParamName() const;
};

}
}

#endif
