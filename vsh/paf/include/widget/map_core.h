/*
 * paf::ui::MapCore and supporting map protocol, JSON serializer, marker/overlay
 * data types, and JS invocation queue for 2014 PAF MapView.
 */
#ifndef PAF_WIDGET_MAP_CORE_H
#define PAF_WIDGET_MAP_CORE_H

#include <stdint.h>
#include <std/string>
#include <thread/thread.h>
#include <widget/w_mapview.h>
#include <widget/web_controller.h>

namespace paf {
namespace ui {

class MapCanvas;
class MapCore;
class MapJsResultParser;

/* Event notification pushed from MapCanvas / MapCanvasObserver into MapCore. */
struct MapCanvasEvent {
	MapCanvasEvent(int32_t kind, int32_t code) : m_kind(kind), m_code(code)
	{
	}

	int32_t m_kind;
	int32_t m_code;
};

/* Common 21-slot callback base shared by MapCore and MapCanvas (vtable 0x812f2d98). */
class MapCallbackBase {
public:
	MapCallbackBase()
	{
	}

	virtual ~MapCallbackBase()
	{
	}

	virtual void OnMapStartLoad()
	{
	}

	virtual void OnMapFinishLoad()
	{
	}

	virtual void OnLoadResult(int32_t status)
	{
		(void)status;
	}

	virtual void OnLoadError(int32_t error_code, int32_t detail)
	{
		(void)error_code;
		(void)detail;
	}

	virtual void OnRedirect()
	{
	}

	virtual void OnBrowserComponentReady()
	{
	}

	virtual void OnSecurityStatusChanged()
	{
	}

	virtual void OnTitleChanged()
	{
	}

	virtual void OnLocationChanged()
	{
	}

	virtual void OnProgressChanged()
	{
	}

	virtual void OnScrollOffsetChanged()
	{
	}

	virtual void OnFaviconChanged()
	{
	}

	virtual void OnContextMenu()
	{
	}

	virtual void OnOpenNewWindow()
	{
	}

	virtual void OnWindowCloseRequested()
	{
	}

	virtual void OnOpenNewWindowWithTarget()
	{
	}

	virtual void OnNavigationStateChanged()
	{
	}

	virtual void OnSelectPopupClosed()
	{
	}

	virtual void OnHistoryChanged()
	{
	}
};

/* WebControllerObserver embedded inside MapCanvas (vtable 0x812f2e80) forwarding to MapCore. */
class MapCanvasObserver : public WebControllerObserver {
public:
	MapCanvasObserver() : WebControllerObserver(), m_map_core(NULL)
	{
	}

	virtual ~MapCanvasObserver()
	{
	}

	virtual void OnStartLoad();
	virtual void OnFinishLoad();
	virtual void OnLoadResult(int32_t status);
	virtual void OnLoadError(int32_t error_code, const common::String &message);
	virtual void OnRedirect();
	virtual void OnBrowserComponentReady();
	virtual void OnProgressChanged(int32_t progress);
	virtual void OnTitleChanged(const common::String &title);
	virtual void OnLocationChanged(const Url &url);
	virtual void OnSecurityStatusChanged(int32_t status);
	virtual void OnScrollOffsetChanged(const math::v2 &offset);
	virtual void OnContextMenu(const ContextMenuInfo &info);
	virtual void OnOpenNewWindow(const Url &url);
	virtual void OnWindowCloseRequested();
	virtual void OnOpenNewWindowWithTarget(const Url &url, const common::String &target);
	virtual void OnNavigationStateChanged(int32_t state);
	virtual void OnSelectPopupClosed();
	virtual void OnHistoryChanged(bool can_go_back, bool can_go_forward);

	MapCore *m_map_core;
};

/* Reference-counted task handle interface stored at the tail of MapCore. */
class MapAsyncTask {
public:
	virtual ~MapAsyncTask()
	{
	}

	virtual int32_t Release() = 0;
	virtual int32_t Cancel() = 0;
	virtual int32_t Start(bool async_mode, int32_t priority, int32_t stack_size) = 0;
	virtual int32_t Wait(int32_t timeout) = 0;
	virtual int32_t Pause() = 0;
	virtual int32_t Resume() = 0;
	virtual int32_t GetStatus() = 0;
	virtual int32_t SetOption(int32_t option_key, int32_t option_value) = 0;
	virtual int32_t GetOption(int32_t option_key, int32_t option_value) = 0;
};

class MapTaskHolder {
public:
	MapTaskHolder() : m_task(NULL)
	{
	}

	virtual ~MapTaskHolder()
	{
		if (m_task != NULL) {
			m_task->Release();
			m_task = NULL;
		}
	}

	virtual int32_t Release()
	{
		if (m_task != NULL) {
			m_task->Release();
			m_task = NULL;
		}
		return 0;
	}

	virtual int32_t Cancel()
	{
		if (m_task != NULL)
			return m_task->Cancel();
		return 0;
	}

	virtual int32_t Start(bool async_mode, int32_t priority, int32_t stack_size)
	{
		if (m_task != NULL)
			return m_task->Start(async_mode, priority, stack_size);
		return 0;
	}

	virtual int32_t Wait(int32_t timeout)
	{
		if (m_task != NULL)
			return m_task->Wait(timeout);
		return 0;
	}

	virtual int32_t Pause()
	{
		if (m_task != NULL)
			return m_task->Pause();
		return 0;
	}

	virtual int32_t Resume()
	{
		if (m_task != NULL)
			return m_task->Resume();
		return 0;
	}

	virtual int32_t GetStatus()
	{
		if (m_task != NULL)
			return m_task->GetStatus();
		return 0;
	}

	virtual int32_t SetOption(int32_t option_key, int32_t option_value)
	{
		if (m_task != NULL)
			return m_task->SetOption(option_key, option_value);
		return 0;
	}

	virtual int32_t GetOption(int32_t option_key, int32_t option_value)
	{
		if (m_task != NULL)
			return m_task->GetOption(option_key, option_value);
		return 0;
	}

	MapAsyncTask *m_task;
};

/* Map marker descriptor (56 bytes, tracked in a global linked list). */
class MapMarker {
public:
	MapMarker();
	~MapMarker();

	const char *GetPopupText() const;

	bool m_draggable;
	char *m_title;
	bool m_has_popup;
	bool m_show_popup;
	bool m_has_image;
	char *m_popup_text;
	MapGeoCoord m_coord;
	int32_t m_anchor_x;
	int32_t m_anchor_y;
	int32_t m_marker_id;
	bool m_visible;
	bool m_center_on_marker;
	int32_t m_icon_id;
};

struct MapMarkerNode {
	MapMarker *m_marker;
	MapMarkerNode *m_next;
};

class MapMarkerList {
public:
	MapMarker *GetAt(int32_t index) const;
	int32_t GetCount() const;

	uint8_t m_fit_bounds;
	MapMarkerNode *m_head;
	int32_t m_count;
	int32_t m_selected_index;
};

/* Map polyline overlay descriptor passed to MapJsRequest::SerializePolyline. */
struct MapPolyline {
	char m_stroke_color[8];
	float m_stroke_opacity;
	int32_t m_stroke_weight;
	int32_t m_style_flags;
	MapGeoCoord *m_points;
	int32_t m_point_count;
	int32_t m_z_index;
	int32_t m_id;
};

/* Map circle overlay descriptor passed to MapJsRequest::SerializeCircle. */
struct MapCircle {
	MapGeoCoord m_center;
	int32_t m_radius;
	float m_fill_opacity;
	char m_fill_color[8];
	char m_stroke_color[8];
	float m_stroke_opacity;
	int32_t m_stroke_weight;
	int32_t m_z_index;
	int32_t m_id;
};

/* Direction step entry serialized in MapJsRequest::SerializeDirectionSteps. */
struct MapDirStep {
	int32_t m_points_len;
	int32_t m_levels_len;
	const char *m_points;
	const char *m_levels;
	MapGeoCoord m_coord;
};

struct MapDirStepList {
	int32_t m_count;
	MapDirStep *m_steps;
};

/* Pending synchronous JS invocation entry tracked in the global invocation table. */
struct MapPendingInvocation {
	void Complete(int32_t status, int32_t error_code);

	uint8_t padding[12];
	int32_t *m_out_result;
	uint8_t mid_padding[4];
	int32_t m_status;
	uint8_t wait_padding[12];
	struct MapInvocationWaiter {
		int32_t SetResult(int32_t first_val, int32_t second_val);

		uint8_t header[12];
		int32_t m_first_val;
		int32_t m_second_val;
	} m_waiter;
	int32_t m_error_code;
};

/* Parsed "##"-delimited callback message sent back from the map JS page. */
class MapJsResultParser {
public:
	MapJsResultParser();
	~MapJsResultParser();

	bool SetInput(const char *data, int32_t length);
	int32_t Clear();
	int32_t ParseField(int32_t field_index);
	int32_t ConvertToken(int32_t field_index);

	string *m_raw_message;
	double m_double_val;
	int32_t m_int_val;
	int32_t m_bool_val;
	const char *m_string_val;
	int32_t m_string_kind;
	char *m_token_buf;
	int32_t m_token_len;
	int32_t m_command_id;
	int32_t m_event_id;
};

/* Handles unsolicited map events (zoom, center, marker drag/click) from JS. */
class MapJsEventHandler {
public:
	MapJsEventHandler();
	~MapJsEventHandler();

	int32_t HandleZoomEvent(MapJsResultParser *parser);
	int32_t HandleCenterZoomEvent(MapJsResultParser *parser);
	int32_t HandleMarkerTapEvent(MapJsResultParser *parser);
	int32_t Dispatch(MapJsResultParser *parser);
	int32_t Handle(MapJsResultParser *parser);

	MapCore *m_map_core;
	int32_t m_last_zoom;
};

/* Handles command completion replies from JS and wakes blocked callers. */
class MapJsResponseHandler {
public:
	MapJsResponseHandler();
	~MapJsResponseHandler();

	int32_t CompleteInvocation(MapJsResultParser *parser);
	int32_t HandleCountryCheckResponse(MapJsResultParser *parser);
	int32_t Dispatch(MapJsResultParser *parser);
	int32_t Handle(MapJsResultParser *parser);

	MapCore *m_map_core;
};

/* Mutex/condvar-protected queue matching JS replies to pending native calls. */
class MapJsMethodInvocationQueue {
public:
	MapJsMethodInvocationQueue();
	~MapJsMethodInvocationQueue();

	void BindCore(MapCore *core);
	int32_t Dispatch(const char *data, int32_t length);
	int32_t SignalResult(int32_t atomic_id, int32_t error_code);
	int32_t SignalResultWithValue(int32_t atomic_id, int32_t error_code, int32_t value);
	int32_t CompleteUpTo(int32_t max_atomic_id);

	thread::Mutex *m_mutex;
	thread::Cond *m_cond;
	MapJsResponseHandler m_response_handler;
	MapJsEventHandler m_event_handler;
};

/* Complete request payload serialized to JSON for javascript:jscallnativebinder('...'). */
class MapJsRequest {
public:
	MapJsRequest();
	~MapJsRequest();

	void SetFunctionId(int32_t function_id);
	void SetTargetZoom(int32_t zoom);
	void SetCurrentZoom(int32_t zoom);
	int32_t GetTargetZoom() const;
	int32_t GetCurrentZoom() const;
	void SetZoomCenter(const MapPixelPoint *point);
	void SetNewZoomCenter(const MapPixelPoint *point);

	int32_t SerializeFullPayload(char **out_buf, uint32_t *out_len,
				     MapMarkerList *markers, bool markers_only);
	int32_t SerializeMarkerList(char **out_buf, uint32_t *out_len, MapMarkerList *markers);
	int32_t SerializeMapState(char **out_buf, uint32_t *out_len, MapMarkerList *markers);
	int32_t SerializePolyline(char **out_buf, uint32_t *out_len);
	int32_t SerializeCircle(char **out_buf, uint32_t *out_len);
	int32_t SerializeZoomCommand(char **out_buf, uint32_t *out_len);
	int32_t SerializeDirectionSteps(char **out_buf, uint32_t *out_len);

	MapGeoCoord m_center;
	MapGeoCoord m_position;
	int32_t m_zoom_param;
	int32_t m_state_flags;
	int32_t m_map_type;
	MapMarker m_marker;
	float m_marker_alpha;
	float m_marker_tilt;
	int32_t m_atomic_id;
	char *m_from_address;
	char *m_to_address;
	int32_t m_direction_mode;
	int32_t m_step_index;
	bool m_direction_active;
	int32_t m_marker_visible_flag;
	int32_t m_function_id;
	MapGeoCoord m_from_coord;
	MapGeoCoord m_to_coord;
	MapGeoCoord m_bounds_sw;
	int32_t m_height;
	int32_t m_width;
	int32_t m_layout_flags;
	MapGeoCoord m_bounds_ne;
	int32_t m_default_zoom;
	int32_t m_tile_mode;
	const char *m_marker_image_url;
	MapMarkerList *m_marker_list;
	MapMarker m_flag_marker;
	MapPolyline *m_polyline;
	MapCircle *m_circle;
	int32_t m_overlay_id;
	int32_t m_overlay_flags;
	const char *m_route_points;
	const char *m_route_levels;
	int32_t m_route_flags;
	MapPixelPoint m_zoom_center;
	MapPixelPoint m_new_zoom_center;
	char *m_language;
	MapDirStepList *m_dir_step_list;
	int32_t m_target_zoom;
	int32_t m_current_zoom;
};

/* Wraps MapJsRequest JSON payloads in javascript:jscallnativebinder('...');. */
class MapJsScriptFormatter {
public:
	char *FormatZoomCommand(MapJsRequest *request);
	char *FormatMarkerList(MapJsRequest *request);
	char *FormatDirectionSteps(MapJsRequest *request);
	char *FormatMapState(MapJsRequest *request);
	char *FormatPolyline(MapJsRequest *request);
	char *FormatCircle(MapJsRequest *request);
};

/* Bridge owned by MapCore that loads loadmapview.htm and executes JS commands. */
class MapJsBridge {
public:
	MapJsBridge(MapCore *core);

	int32_t IsReady() const;
	int32_t LoadMapPage();
	int32_t ExecuteRequest(MapJsRequest *request, int32_t serializer_kind);

	uint32_t m_bridge_flags;
	MapCore *m_map_core;
	void *m_pending_handle;
	int32_t m_pending_count;
	uint8_t padding[20];
	bool m_loaded;
};

/* Country capability lookup helper using the 240-entry ISO-3166 coordinate table. */
class MapCountryLookup {
public:
	bool IsRegionFlagged(const char *country_code) const;
};

/* Singleton helper binding MapView to its location tracking state. */
class MapLocationManager {
public:
	struct MapLocationClient {
		void *m_handle;
		uint32_t m_state;
		uint32_t m_interval;
		MapView *m_map_view;
		uint32_t m_active;
		int32_t m_mode;
	};

	static int32_t Bind(MapView *view);

	MapLocationClient *m_client;
	MapView *m_map_view;
	uint8_t padding[8];
	MapGeoCoord m_last_coord;
	MapGeoCoord m_target_coord;
	uint32_t m_flags;
};

/* Core controller owned by MapView (496 bytes, vtable 0x812f2df4). */
class MapCore : public MapCallbackBase {
public:
	MapCore(MapView *map_view);
	virtual ~MapCore();

	virtual int32_t ZoomInAt(int32_t zoom_level, const MapPixelPoint *old_center,
				 const MapPixelPoint *new_center);
	virtual int32_t ZoomOutAt(int32_t zoom_level, const MapPixelPoint *old_center,
				  const MapPixelPoint *new_center);
	virtual int32_t DispatchJsResult(const char *data, int32_t length);
	virtual void OnCanvasEvent(const MapCanvasEvent *event);
	virtual MapViewListener *GetViewListener();
	virtual MapView *GetMapView();
	virtual int32_t GetZoomLevel(int32_t *out_zoom);

	int32_t LoadPageUrl(const char *url);
	int32_t ExecuteJavaScriptUrl(const char *script_url);
	int32_t DrawDirectionSteps(MapDirStepList *steps, const char *from_name,
				   const char *to_name, double from_lat, double from_lon,
				   double to_lat, double to_lon, int32_t mode,
				   int32_t step_index, int32_t icon_id, int32_t route_flags,
				   int32_t options);

	thread::Mutex *m_mutex;
	MapJsMethodInvocationQueue m_invocation_queue;
	MapJsBridge *m_js_bridge;
	uint8_t padding[16];
	MapGeoCoord m_default_coord;
	MapJsRequest m_request;
	void *m_pending_callback;
	void *m_active_listener;
	MapView *m_map_view;
	MapCanvas *m_canvas;
	bool m_auto_init;
	bool m_init_with_center;
	int32_t m_map_state;
	bool m_init_default;
	int32_t m_error_code;
	MapTaskHolder m_geocode_task;
	MapTaskHolder m_route_task;
};

}
}

#endif
