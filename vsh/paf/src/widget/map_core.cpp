/*
 * paf::ui::MapCore, JS protocol serializer/parser, invocation queue, marker
 * tracking, and ISO-3166 country lookup table for 2014 PAF MapView.
 */
#include <widget/map_core.h>
#include <widget/map_canvas.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <url/url.h>

namespace paf {
namespace ui {

static const char s_empty_str[] = "";
static const char s_comma_sep[] = ", ";
static const char s_field_delimiter[] = "##";
static const char s_js_binder_prefix[] = "javascript:jscallnativebinder('";
static const char s_js_binder_suffix[] = ");";
static const char s_load_map_url[] = "file:///mapview/loadmapview.htm";

static bool s_map_loaded_once;
static bool s_browser_ready;
static bool s_map_tiles_loading;
static MapJsMethodInvocationQueue *s_active_invocation_queue;
static bool s_parser_first_use;
static MapLocationManager *s_location_manager;

struct MapMarkerRegistryNode {
	MapMarker *m_marker;
	MapMarkerRegistryNode *m_prev;
	MapMarkerRegistryNode *m_next;
};

static MapMarkerRegistryNode *s_marker_registry_head;
static int32_t s_next_marker_id;

struct MapInvocationSlot {
	int32_t m_atomic_id;
	MapPendingInvocation *m_invocation;
};

static MapInvocationSlot s_pending_invocations[150];
static uint32_t s_marker_id_pool_a[255];
static uint32_t s_marker_id_pool_b[255];
static uint32_t s_marker_id_pool_c[255];
static uint32_t s_marker_id_pool_d[255];

static const int32_t s_js_field_types[17] = {
	3, 3, 3, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 4, 3, 3
};

struct MapCountryEntry {
	const char *m_code;
	double m_latitude;
	double m_longitude;
	uint8_t m_feature_a;
	uint8_t m_region_flag;
	uint8_t m_feature_b;
	uint8_t m_feature_c;
};

static const MapCountryEntry s_country_table[240] = {
	{ "AD", 42.5, 1.5, 0, 0, 0, 0 },
	{ "AE", 24.0, 54.0, 0, 0, 0, 0 },
	{ "AF", 33.0, 65.0, 0, 0, 0, 0 },
	{ "AG", 17.049999237060547, -61.79999923706055, 0, 0, 0, 0 },
	{ "AI", 18.25, -63.16669845581055, 0, 0, 0, 0 },
	{ "AL", 41.0, 20.0, 0, 0, 0, 0 },
	{ "AM", 40.0, 45.0, 0, 0, 0, 0 },
	{ "AN", 12.25, -68.75, 0, 0, 0, 0 },
	{ "AO", -12.5, 18.5, 0, 0, 0, 0 },
	{ "AP", 35.0, 105.0, 0, 0, 0, 0 },
	{ "AQ", -90.0, 0.0, 0, 0, 0, 0 },
	{ "AR", -34.0, -64.0, 0, 0, 0, 0 },
	{ "AS", -14.33329963684082, -170.0, 0, 0, 0, 0 },
	{ "AT", 47.33330154418945, 13.33329963684082, 0, 0, 0, 0 },
	{ "AU", -27.0, 133.0, 0, 0, 0, 0 },
	{ "AW", 12.5, -69.9666976928711, 0, 0, 0, 0 },
	{ "AZ", 40.5, 47.5, 0, 0, 0, 0 },
	{ "BA", 44.0, 18.0, 0, 0, 0, 0 },
	{ "BB", 13.16670036315918, -59.53329849243164, 0, 0, 0, 0 },
	{ "BD", 24.0, 90.0, 0, 0, 0, 0 },
	{ "BE", 50.83330154418945, 4.0, 0, 0, 0, 0 },
	{ "BF", 13.0, -2.0, 0, 0, 0, 0 },
	{ "BG", 43.0, 25.0, 0, 0, 0, 0 },
	{ "BH", 26.0, 50.54999923706055, 0, 0, 0, 0 },
	{ "BI", -3.5, 30.0, 0, 0, 0, 0 },
	{ "BJ", 9.5, 2.25, 0, 0, 0, 0 },
	{ "BM", 32.33330154418945, -64.75, 0, 0, 0, 0 },
	{ "BN", 4.5, 114.66670227050781, 0, 0, 0, 0 },
	{ "BO", -17.0, -65.0, 0, 0, 0, 0 },
	{ "BR", -10.0, -55.0, 0, 0, 0, 0 },
	{ "BS", 24.25, -76.0, 0, 0, 0, 0 },
	{ "BT", 27.5, 90.5, 0, 0, 0, 0 },
	{ "BV", -54.43330001831055, 3.4000000953674316, 0, 0, 0, 0 },
	{ "BW", -22.0, 24.0, 0, 0, 0, 0 },
	{ "BY", 53.0, 28.0, 0, 0, 0, 0 },
	{ "BZ", 17.25, -88.75, 0, 0, 0, 0 },
	{ "CA", 60.0, -95.0, 1, 1, 1, 0 },
	{ "CC", -12.5, 96.83329772949219, 0, 0, 0, 0 },
	{ "CD", 0.0, 25.0, 0, 0, 0, 0 },
	{ "CF", 7.0, 21.0, 0, 0, 0, 0 },
	{ "CG", -1.0, 15.0, 0, 0, 0, 0 },
	{ "CH", 47.0, 8.0, 0, 0, 0, 0 },
	{ "CI", 8.0, -5.0, 0, 0, 0, 0 },
	{ "CK", -21.233299255371094, -159.76669311523438, 0, 0, 0, 0 },
	{ "CL", -30.0, -71.0, 0, 0, 0, 0 },
	{ "CM", 6.0, 12.0, 0, 0, 0, 0 },
	{ "CN", 35.0, 105.0, 0, 0, 0, 0 },
	{ "CO", 4.0, -72.0, 0, 0, 0, 0 },
	{ "CR", 10.0, -84.0, 0, 0, 0, 0 },
	{ "CU", 21.5, -80.0, 0, 0, 0, 0 },
	{ "CV", 16.0, -24.0, 0, 0, 0, 0 },
	{ "CX", -10.5, 105.66670227050781, 0, 0, 0, 0 },
	{ "CY", 35.0, 33.0, 0, 0, 0, 0 },
	{ "CZ", 49.75, 15.5, 0, 0, 0, 0 },
	{ "DE", 51.0, 9.0, 0, 0, 0, 0 },
	{ "DJ", 11.5, 43.0, 0, 0, 0, 0 },
	{ "DK", 56.0, 10.0, 0, 0, 0, 0 },
	{ "DM", 15.4167, -61.33330154418945, 0, 0, 0, 0 },
	{ "DO", 19.0, -70.66670227050781, 0, 0, 0, 0 },
	{ "DZ", 28.0, 3.0, 0, 0, 0, 0 },
	{ "EC", -2.0, -77.5, 0, 0, 0, 0 },
	{ "EE", 59.0, 26.0, 0, 0, 0, 0 },
	{ "EG", 27.0, 30.0, 0, 0, 0, 0 },
	{ "EH", 24.5, -13.0, 0, 0, 0, 0 },
	{ "ER", 15.0, 39.0, 0, 0, 0, 0 },
	{ "ES", 40.0, -4.0, 0, 0, 0, 0 },
	{ "ET", 8.0, 38.0, 0, 0, 0, 0 },
	{ "EU", 47.0, 8.0, 0, 0, 0, 0 },
	{ "FI", 64.0, 26.0, 1, 1, 1, 0 },
	{ "FJ", -18.0, 175.0, 0, 0, 0, 0 },
	{ "FK", -51.75, -59.0, 0, 0, 0, 0 },
	{ "FM", 6.9167, 158.25, 0, 0, 0, 0 },
	{ "FO", 62.0, -7.0, 0, 0, 0, 0 },
	{ "FR", 46.0, 2.0, 0, 0, 0, 0 },
	{ "GA", -1.0, 11.75, 0, 0, 0, 0 },
	{ "GB", 54.0, -2.0, 0, 0, 0, 0 },
	{ "GD", 12.1167, -61.66669845581055, 0, 0, 0, 0 },
	{ "GE", 42.0, 43.5, 0, 0, 0, 0 },
	{ "GF", 4.0, -53.0, 0, 0, 0, 0 },
	{ "GH", 8.0, -2.0, 0, 0, 0, 0 },
	{ "GI", 36.18330001831055, -5.366700172424316, 0, 0, 0, 0 },
	{ "GL", 72.0, -40.0, 0, 0, 0, 0 },
	{ "GM", 13.466699600219727, -16.566699981689453, 0, 0, 0, 0 },
	{ "GN", 11.0, -10.0, 0, 0, 0, 0 },
	{ "GP", 16.25, -61.58330154418945, 0, 0, 0, 0 },
	{ "GQ", 2.0, 10.0, 0, 0, 0, 0 },
	{ "GR", 39.0, 22.0, 0, 0, 0, 0 },
	{ "GS", -54.5, -37.0, 0, 0, 0, 0 },
	{ "GT", 15.5, -90.25, 0, 0, 0, 0 },
	{ "GU", 13.466699600219727, 144.78329467773438, 0, 0, 0, 0 },
	{ "GW", 12.0, -15.0, 0, 0, 0, 0 },
	{ "GY", 5.0, -59.0, 0, 0, 0, 0 },
	{ "HK", 22.25, 114.16670227050781, 0, 1, 1, 0 },
	{ "HM", -53.099998474121094, 72.5167007446289, 0, 0, 0, 0 },
	{ "HN", 15.0, -86.5, 0, 0, 0, 0 },
	{ "HR", 45.16669845581055, 15.5, 0, 0, 0, 0 },
	{ "HT", 19.0, -72.41670227050781, 0, 0, 0, 0 },
	{ "HU", 47.0, 20.0, 0, 0, 0, 0 },
	{ "ID", -5.0, 120.0, 0, 0, 0, 0 },
	{ "IE", 53.0, -8.0, 0, 0, 0, 0 },
	{ "IL", 31.5, 34.75, 0, 0, 0, 0 },
	{ "IN", 20.0, 77.0, 0, 0, 0, 0 },
	{ "IO", -6.0, 71.5, 0, 0, 0, 0 },
	{ "IQ", 33.0, 44.0, 0, 0, 0, 0 },
	{ "IR", 32.0, 53.0, 0, 0, 0, 0 },
	{ "IS", 65.0, -18.0, 0, 0, 0, 0 },
	{ "IT", 42.83330154418945, 12.83329963684082, 1, 1, 1, 0 },
	{ "JM", 18.25, -77.5, 0, 0, 0, 0 },
	{ "JO", 31.0, 36.0, 0, 0, 0, 0 },
	{ "JP", 36.0, 138.0, 0, 0, 0, 0 },
	{ "KE", 1.0, 38.0, 0, 0, 0, 0 },
	{ "KG", 41.0, 75.0, 0, 0, 0, 0 },
	{ "KH", 13.0, 105.0, 0, 0, 0, 0 },
	{ "KI", 1.4167, 173.0, 0, 0, 0, 0 },
	{ "KM", -12.16670036315918, 44.25, 0, 0, 0, 0 },
	{ "KN", 17.33329963684082, -62.75, 0, 0, 0, 0 },
	{ "KP", 40.0, 127.0, 0, 0, 0, 0 },
	{ "KR", 37.0, 127.5, 0, 0, 0, 0 },
	{ "KW", 29.3375, 47.65810012817383, 0, 0, 0, 0 },
	{ "KY", 19.5, -80.5, 0, 0, 0, 0 },
	{ "KZ", 48.0, 68.0, 0, 0, 0, 0 },
	{ "LA", 18.0, 105.0, 0, 0, 0, 0 },
	{ "LB", 33.83330154418945, 35.83330154418945, 0, 0, 0, 0 },
	{ "LC", 13.883299827575684, -61.13330078125, 0, 0, 0, 0 },
	{ "LI", 47.16669845581055, 9.533300399780273, 0, 0, 0, 0 },
	{ "LK", 7.0, 81.0, 0, 0, 0, 0 },
	{ "LR", 6.5, -9.5, 0, 0, 0, 0 },
	{ "LS", -29.5, 28.5, 0, 0, 0, 0 },
	{ "LT", 56.0, 24.0, 0, 0, 0, 0 },
	{ "LU", 49.75, 6.1666998863220215, 0, 0, 0, 0 },
	{ "LV", 57.0, 25.0, 0, 0, 0, 0 },
	{ "LY", 25.0, 17.0, 0, 0, 0, 0 },
	{ "MA", 32.0, -5.0, 0, 0, 0, 0 },
	{ "MC", 43.733299255371094, 7.400000095367432, 0, 0, 0, 0 },
	{ "MD", 47.0, 29.0, 0, 0, 0, 0 },
	{ "ME", 42.0, 19.0, 0, 0, 0, 0 },
	{ "MG", -20.0, 47.0, 0, 0, 0, 0 },
	{ "MH", 9.0, 168.0, 0, 0, 0, 0 },
	{ "MK", 41.83330154418945, 22.0, 0, 0, 0, 0 },
	{ "ML", 17.0, -4.0, 0, 0, 0, 0 },
	{ "MM", 22.0, 98.0, 0, 0, 0, 0 },
	{ "MN", 46.0, 105.0, 0, 0, 0, 0 },
	{ "MO", 22.16670036315918, 113.55000305175781, 0, 0, 0, 0 },
	{ "MP", 15.199999809265137, 145.75, 0, 0, 0, 0 },
	{ "MQ", 14.66670036315918, -61.0, 0, 0, 0, 0 },
	{ "MR", 20.0, -12.0, 0, 0, 0, 0 },
	{ "MS", 16.75, -62.20000076293945, 0, 0, 0, 0 },
	{ "MT", 35.83330154418945, 14.58329963684082, 0, 0, 0, 0 },
	{ "MU", -20.283300399780273, 57.54999923706055, 0, 0, 0, 0 },
	{ "MV", 3.25, 73.0, 0, 0, 0, 0 },
	{ "MW", -13.5, 34.0, 0, 0, 0, 0 },
	{ "MX", 23.0, -102.0, 0, 0, 0, 0 },
	{ "MY", 2.5, 112.5, 0, 0, 0, 0 },
	{ "MZ", -18.25, 35.0, 0, 0, 0, 0 },
	{ "NA", -22.0, 17.0, 0, 0, 0, 0 },
	{ "NC", -21.5, 165.5, 0, 0, 0, 0 },
	{ "NE", 16.0, 8.0, 0, 0, 0, 0 },
	{ "NF", -29.033300399780273, 167.9499969482422, 0, 0, 0, 0 },
	{ "NG", 10.0, 8.0, 0, 0, 0, 0 },
	{ "NI", 13.0, -85.0, 0, 0, 0, 0 },
	{ "NL", 52.5, 5.75, 0, 0, 0, 0 },
	{ "NO", 62.0, 10.0, 0, 0, 0, 0 },
	{ "NP", 28.0, 84.0, 0, 0, 0, 0 },
	{ "NR", -0.53329998254776, 166.9167022705078, 0, 0, 0, 0 },
	{ "NU", -19.033300399780273, -169.86669921875, 0, 0, 0, 0 },
	{ "NZ", -41.0, 174.0, 0, 0, 0, 0 },
	{ "OM", 21.0, 57.0, 0, 0, 0, 0 },
	{ "PA", 9.0, -80.0, 0, 0, 0, 0 },
	{ "PE", -10.0, -76.0, 0, 0, 0, 0 },
	{ "PF", -15.0, -140.0, 0, 0, 0, 0 },
	{ "PG", -6.0, 147.0, 0, 0, 0, 0 },
	{ "PH", 13.0, 122.0, 0, 0, 0, 0 },
	{ "PK", 30.0, 70.0, 0, 0, 0, 0 },
	{ "PL", 52.0, 20.0, 0, 0, 0, 0 },
	{ "PM", 46.83330154418945, -56.33330154418945, 0, 0, 0, 0 },
	{ "PR", 18.25, -66.5, 0, 0, 0, 0 },
	{ "PS", 32.0, 35.25, 0, 0, 0, 0 },
	{ "PT", 39.5, -8.0, 0, 0, 0, 0 },
	{ "PW", 7.5, 134.5, 0, 0, 0, 0 },
	{ "PY", -23.0, -58.0, 0, 0, 0, 0 },
	{ "QA", 25.5, 51.25, 0, 0, 0, 0 },
	{ "RE", -21.100000381469727, 55.599998474121094, 0, 0, 0, 0 },
	{ "RO", 46.0, 25.0, 0, 0, 0, 0 },
	{ "RS", 44.0, 21.0, 0, 0, 0, 0 },
	{ "RU", 60.0, 100.0, 1, 1, 1, 0 },
	{ "RW", -2.0, 30.0, 0, 0, 0, 0 },
	{ "SA", 25.0, 45.0, 0, 0, 0, 0 },
	{ "SB", -8.0, 159.0, 0, 0, 0, 0 },
	{ "SC", -4.5833001136779785, 55.66669845581055, 0, 0, 0, 0 },
	{ "SD", 15.0, 30.0, 0, 0, 0, 0 },
	{ "SE", 62.0, 15.0, 0, 0, 0, 0 },
	{ "SG", 1.3667000532150269, 103.80000305175781, 0, 0, 0, 0 },
	{ "SH", -15.933300018310547, -5.699999809265137, 0, 0, 0, 0 },
	{ "SI", 46.0, 15.0, 0, 0, 0, 0 },
	{ "SJ", 78.0, 20.0, 0, 0, 0, 0 },
	{ "SK", 48.66669845581055, 19.5, 0, 0, 0, 0 },
	{ "SL", 8.5, -11.5, 0, 0, 0, 0 },
	{ "SM", 43.766700744628906, 12.41670036315918, 0, 0, 0, 0 },
	{ "SN", 14.0, -14.0, 1, 1, 1, 0 },
	{ "SO", 10.0, 49.0, 0, 0, 0, 0 },
	{ "SR", 4.0, -56.0, 0, 0, 0, 0 },
	{ "ST", 1.0, 7.0, 0, 0, 0, 0 },
	{ "SV", 13.83329963684082, -88.91670227050781, 0, 0, 0, 0 },
	{ "SY", 35.0, 38.0, 0, 0, 0, 0 },
	{ "SZ", -26.5, 31.5, 0, 0, 0, 0 },
	{ "TC", 21.75, -71.58329772949219, 0, 0, 0, 0 },
	{ "TD", 15.0, 19.0, 0, 0, 0, 0 },
	{ "TF", -43.0, 67.0, 0, 0, 0, 0 },
	{ "TG", 8.0, 1.166700005531311, 0, 0, 0, 0 },
	{ "TH", 15.0, 100.0, 0, 1, 1, 0 },
	{ "TJ", 39.0, 71.0, 0, 0, 0, 0 },
	{ "TK", -9.0, -172.0, 0, 0, 0, 0 },
	{ "TM", 40.0, 60.0, 0, 0, 0, 0 },
	{ "TN", 34.0, 9.0, 0, 0, 0, 0 },
	{ "TO", -20.0, -175.0, 0, 0, 0, 0 },
	{ "TR", 39.0, 35.0, 0, 0, 0, 0 },
	{ "TT", 11.0, -61.0, 0, 0, 0, 0 },
	{ "TV", -8.0, 178.0, 0, 0, 0, 0 },
	{ "TW", 23.5, 121.0, 0, 0, 0, 0 },
	{ "TZ", -6.0, 35.0, 0, 0, 0, 0 },
	{ "UA", 49.0, 32.0, 0, 0, 0, 0 },
	{ "UG", 1.0, 32.0, 0, 0, 0, 0 },
	{ "UM", 19.283300399780273, 166.60000610351562, 0, 0, 0, 0 },
	{ "US", 38.0, -97.0, 1, 1, 1, 0 },
	{ "UY", -33.0, -56.0, 0, 0, 0, 0 },
	{ "UZ", 41.0, 64.0, 0, 0, 0, 0 },
	{ "VA", 41.900001525878906, 12.449999809265137, 0, 0, 0, 0 },
	{ "VC", 13.25, -61.20000076293945, 0, 0, 0, 0 },
	{ "VE", 8.0, -66.0, 0, 0, 0, 0 },
	{ "VG", 18.5, -64.5, 0, 0, 0, 0 },
	{ "VI", 18.33329963684082, -64.83329772949219, 0, 0, 0, 0 },
	{ "VN", 16.0, 106.0, 0, 0, 0, 0 },
	{ "VU", -16.0, 167.0, 0, 0, 0, 0 },
	{ "WF", -13.300000190734863, -176.1999969482422, 0, 0, 0, 0 },
	{ "WS", -13.58329963684082, -172.3332977294922, 0, 0, 0, 0 },
	{ "YE", 15.0, 48.0, 0, 0, 0, 0 },
	{ "YT", -12.83329963684082, 45.16669845581055, 0, 0, 0, 0 },
	{ "ZA", -29.0, 24.0, 0, 0, 0, 0 },
	{ "ZM", -15.0, 30.0, 0, 0, 0, 0 },
	{ "ZW", -20.0, 30.0, 0, 0, 0, 0 }
};

MapCore::MapCore(MapView *map_view)
	: MapCallbackBase(),
	  m_mutex(NULL),
	  m_invocation_queue(),
	  m_js_bridge(NULL),
	  m_default_coord(),
	  m_request(),
	  m_pending_callback(NULL),
	  m_active_listener(NULL),
	  m_map_view(map_view),
	  m_canvas(NULL),
	  m_auto_init(true),
	  m_init_with_center(false),
	  m_map_state(0),
	  m_init_default(false),
	  m_error_code(0),
	  m_geocode_task(),
	  m_route_task()
{
	m_invocation_queue.BindCore(this);
	m_mutex = new thread::Mutex("_MapCore", 0);
	m_js_bridge = new MapJsBridge(this);
	m_canvas = new MapCanvas(map_view, NULL);
	m_canvas->AttachCore(this);
	static_cast<Widget *>(m_canvas)->SetAdjust(1, 1, 1);

	math::v4 view_size = *map_view->GetSize(Widget::GET_CURRENT);

	m_canvas->UpdateCanvasSize(view_size);
	m_init_default = false;
}

MapCore::~MapCore()
{
	m_active_listener = NULL;
	if (m_mutex != NULL)
		delete m_mutex;
	if (m_canvas != NULL) {
		static_cast<Widget *>(m_canvas)->DestroyWidget();
		m_canvas->DetachCore();
		m_canvas = NULL;
	}
}

int32_t MapCore::LoadPageUrl(const char *url)
{
	string url_str(url);
	Url parsed(url_str);
	int32_t ret = static_cast<WebController *>(m_canvas)->LoadUrl(parsed);

	if (ret != 0)
		sce_paf_printf_dummy();
	return ret;
}

int32_t MapCore::ExecuteJavaScriptUrl(const char *script_url)
{
	string url_str(script_url);
	Url parsed(url_str);
	int32_t ret = static_cast<WebController *>(m_canvas)->LoadUrl(parsed);

	if (ret != 0)
		sce_paf_printf_dummy();
	return ret;
}

int32_t MapCore::GetZoomLevel(int32_t *out_zoom)
{
	if (m_mutex != NULL)
		m_mutex->Lock();
	if (out_zoom != NULL) {
		*out_zoom = m_request.GetCurrentZoom();
		if (m_mutex != NULL)
			m_mutex->Unlock();
		return 0;
	}
	if (m_mutex != NULL)
		m_mutex->Unlock();
	return 4;
}

int32_t MapCore::DrawDirectionSteps(MapDirStepList *steps, const char *from_name,
				    const char *to_name, double from_lat, double from_lon,
				    double to_lat, double to_lon, int32_t mode,
				    int32_t step_index, int32_t icon_id, int32_t route_flags,
				    int32_t options)
{
	(void)from_name;
	(void)to_name;
	(void)from_lat;
	(void)from_lon;
	(void)to_lat;
	(void)to_lon;
	(void)mode;
	(void)step_index;
	(void)icon_id;
	(void)route_flags;
	(void)options;

	if (m_mutex != NULL)
		m_mutex->Lock();

	int32_t ret = 0;

	if (m_js_bridge != NULL) {
		m_request.m_dir_step_list = steps;
		m_request.SetFunctionId(1434);
		ret = m_js_bridge->ExecuteRequest(&m_request, 5);
		m_request.m_route_points = NULL;
		m_request.m_route_levels = NULL;
		m_request.m_dir_step_list = NULL;
	}

	if (m_mutex != NULL)
		m_mutex->Unlock();
	return ret;
}

int32_t MapCore::ZoomInAt(int32_t zoom_level, const MapPixelPoint *old_center,
			  const MapPixelPoint *new_center)
{
	if (m_mutex != NULL)
		m_mutex->Lock();
	if (m_js_bridge != NULL) {
		m_request.SetTargetZoom(zoom_level);
		m_request.SetCurrentZoom(m_request.GetTargetZoom());
		m_request.SetZoomCenter(old_center);
		m_request.SetNewZoomCenter(new_center);
		m_request.SetFunctionId(269);

		int32_t ret = m_js_bridge->ExecuteRequest(&m_request, 2);

		if (m_mutex != NULL)
			m_mutex->Unlock();
		return ret;
	}
	if (m_mutex != NULL)
		m_mutex->Unlock();
	return 4;
}

int32_t MapCore::ZoomOutAt(int32_t zoom_level, const MapPixelPoint *old_center,
			   const MapPixelPoint *new_center)
{
	if (m_mutex != NULL)
		m_mutex->Lock();
	if (m_js_bridge != NULL) {
		m_request.SetTargetZoom(zoom_level);
		m_request.SetCurrentZoom(m_request.GetTargetZoom());
		m_request.SetZoomCenter(old_center);
		m_request.SetNewZoomCenter(new_center);
		m_request.SetFunctionId(270);

		int32_t ret = m_js_bridge->ExecuteRequest(&m_request, 2);

		if (m_mutex != NULL)
			m_mutex->Unlock();
		return ret;
	}
	if (m_mutex != NULL)
		m_mutex->Unlock();
	return 4;
}

void MapCore::OnCanvasEvent(const MapCanvasEvent *event)
{
	sce_paf_printf_dummy();
	if (event->m_kind == 0) {
		s_browser_ready = true;
		m_js_bridge = new MapJsBridge(this);
		if (m_js_bridge != NULL && m_js_bridge->IsReady() == 0)
			m_js_bridge->LoadMapPage();
	} else if (event->m_kind == 2) {
		if (!s_map_loaded_once && m_map_view != NULL && m_map_view->m_listener != NULL)
			m_map_view->m_listener->OnMapLoadStart(m_map_view);
	} else if (event->m_kind == 3) {
		if (!s_map_loaded_once) {
			sce_paf_printf_dummy();
			if (m_init_with_center) {
				m_request.SetFunctionId(10);
				m_js_bridge->ExecuteRequest(&m_request, 0);
			} else if (m_init_default) {
				m_request.SetFunctionId(11);
				m_js_bridge->ExecuteRequest(&m_request, 0);
			}
			s_map_loaded_once = true;
			if (m_map_view != NULL && m_map_view->m_listener != NULL)
				m_map_view->m_listener->OnMapLoadFinish(m_map_view);
		}
	}
}

MapViewListener *MapCore::GetViewListener()
{
	if (m_map_view != NULL && m_map_view->m_listener != NULL)
		return m_map_view->m_listener;
	return NULL;
}

MapView *MapCore::GetMapView()
{
	return m_map_view;
}

int32_t MapCore::DispatchJsResult(const char *data, int32_t length)
{
	return m_invocation_queue.Dispatch(data, length);
}

int32_t MapPendingInvocation::MapInvocationWaiter::SetResult(int32_t first_val, int32_t second_val)
{
	m_first_val = first_val;
	m_second_val = second_val;
	return 0;
}

void MapPendingInvocation::Complete(int32_t status, int32_t error_code)
{
	m_error_code = error_code;
	m_waiter.SetResult(0, 0);
	m_status = status;
}

MapJsMethodInvocationQueue::MapJsMethodInvocationQueue()
	: m_mutex(NULL), m_cond(NULL), m_response_handler(), m_event_handler()
{
	m_mutex = new thread::Mutex("_MapCoreJsMethodInvocationQueue", 0);
	if (m_mutex != NULL)
		m_cond = new thread::Cond(*m_mutex, "_MapCoreJsMethodInvocationQueue", 0);
	s_active_invocation_queue = this;
}

MapJsMethodInvocationQueue::~MapJsMethodInvocationQueue()
{
	if (m_mutex != NULL) {
		delete m_mutex;
		m_mutex = NULL;
	}
	if (m_cond != NULL) {
		delete m_cond;
		m_cond = NULL;
	}
	s_active_invocation_queue = NULL;
}

void MapJsMethodInvocationQueue::BindCore(MapCore *core)
{
	m_response_handler.m_map_core = core;
	m_event_handler.m_map_core = core;
}

int32_t MapJsMethodInvocationQueue::Dispatch(const char *data, int32_t length)
{
	MapJsResultParser parser;

	if (parser.SetInput(data, length))
		return 1;

	if (parser.ParseField(1) == 2) {
		parser.Clear();
		return 0;
	}

	int32_t event_id = parser.m_int_val;

	parser.m_event_id = event_id;
	parser.ParseField(0);
	parser.m_command_id = parser.m_int_val;

	if (event_id == 0) {
		int32_t ret = m_response_handler.Handle(&parser);

		parser.Clear();
		return ret;
	}

	int32_t event_ret = m_event_handler.Handle(&parser);

	if (parser.m_command_id != 0) {
		int32_t resp_ret = m_response_handler.Handle(&parser);

		parser.Clear();
		return resp_ret;
	}
	parser.Clear();
	return event_ret;
}

int32_t MapJsMethodInvocationQueue::SignalResult(int32_t atomic_id, int32_t error_code)
{
	m_mutex->Lock();
	for (int32_t idx = 0; idx < 150; ++idx) {
		if (s_pending_invocations[idx].m_atomic_id == atomic_id &&
		    s_pending_invocations[idx].m_invocation != NULL) {
			if (error_code != 0)
				s_pending_invocations[idx].m_invocation->Complete(2, error_code);
			else
				s_pending_invocations[idx].m_invocation->Complete(0, 0);
			m_mutex->Unlock();
			return 0;
		}
	}
	m_mutex->Unlock();
	return -1;
}

int32_t MapJsMethodInvocationQueue::SignalResultWithValue(int32_t atomic_id, int32_t error_code,
							  int32_t value)
{
	m_mutex->Lock();
	for (int32_t idx = 0; idx < 150; ++idx) {
		if (s_pending_invocations[idx].m_atomic_id == atomic_id &&
		    s_pending_invocations[idx].m_invocation != NULL) {
			if (error_code != 0) {
				s_pending_invocations[idx].m_invocation->Complete(2, error_code);
			} else {
				*s_pending_invocations[idx].m_invocation->m_out_result = value;
				s_pending_invocations[idx].m_invocation->Complete(0, 0);
			}
			m_mutex->Unlock();
			return 0;
		}
	}
	m_mutex->Unlock();
	return -1;
}

int32_t MapJsMethodInvocationQueue::CompleteUpTo(int32_t max_atomic_id)
{
	m_mutex->Lock();
	for (int32_t idx = 0; idx < 150; ++idx) {
		if (s_pending_invocations[idx].m_atomic_id <= max_atomic_id &&
		    s_pending_invocations[idx].m_invocation != NULL)
			s_pending_invocations[idx].m_invocation->Complete(0, 0);
	}
	m_mutex->Unlock();
	return -1;
}

MapJsEventHandler::MapJsEventHandler() : m_map_core(NULL), m_last_zoom(0)
{
}

MapJsEventHandler::~MapJsEventHandler()
{
}

int32_t MapJsEventHandler::HandleZoomEvent(MapJsResultParser *parser)
{
	if (parser != NULL) {
		parser->ParseField(7);
		int32_t zoom = parser->m_int_val;

		if (m_map_core != NULL) {
			m_map_core->m_request.SetCurrentZoom(zoom);
			MapView *view = m_map_core->m_map_view;

			if (view != NULL && view->m_listener != NULL) {
				if (m_last_zoom != zoom) {
					view->m_listener->OnZoomLevelChanged(view, zoom);
					m_last_zoom = zoom;
				}
				m_map_core->m_map_view->NotifyZoomCommit(zoom, 0);
			}
		}
	}
	return 0;
}

int32_t MapJsEventHandler::HandleCenterZoomEvent(MapJsResultParser *parser)
{
	if (parser != NULL) {
		parser->ParseField(8);
		double lat = parser->m_double_val;

		parser->ParseField(9);
		m_map_core->m_request.m_center.m_latitude = lat;
		m_map_core->m_request.m_center.m_longitude = parser->m_double_val;

		MapView *view = m_map_core->m_map_view;

		if (view != NULL && view->m_listener != NULL)
			view->m_listener->OnCenterChanged(view, &m_map_core->m_request.m_center);

		HandleZoomEvent(parser);
	}
	return 0;
}

int32_t MapJsEventHandler::HandleMarkerTapEvent(MapJsResultParser *parser)
{
	if (parser != NULL && parser->ParseField(6) != 2) {
		int32_t marker_id = parser->m_int_val;
		MapPixelPoint pt;

		parser->ParseField(4);
		pt.m_x = parser->m_int_val;
		parser->ParseField(5);
		pt.m_y = parser->m_int_val;

		parser->ParseField(8);
		double lat = parser->m_double_val;

		parser->ParseField(9);
		MapView *view = m_map_core->m_map_view;

		view->m_touched_coord.m_latitude = lat;
		view->m_touched_coord.m_longitude = parser->m_double_val;
		if (m_map_core != NULL && view != NULL)
			view->NotifyMarkerClick(&pt, marker_id, -3);
	}
	return 0;
}

int32_t MapJsEventHandler::Dispatch(MapJsResultParser *parser)
{
	int32_t ev = parser->m_event_id;

	if (ev <= 99) {
		if (ev > 22) {
			if (ev == 99) {
				s_map_tiles_loading = true;
				return 0;
			}
			return 2;
		}
		if (ev < 0)
			return 2;

		switch (ev) {
		case 0:
			return HandleMarkerTapEvent(parser);
		case 1:
			if (parser != NULL && parser->ParseField(6) != 2) {
				int32_t marker_id = parser->m_int_val;
				MapPixelPoint pt;

				parser->ParseField(4);
				pt.m_x = parser->m_int_val;
				parser->ParseField(5);
				pt.m_y = parser->m_int_val;
				if (m_map_core != NULL && m_map_core->m_map_view != NULL)
					m_map_core->m_map_view->NotifyMarkerClick(&pt, marker_id,
										  -3);
			}
			return 0;
		case 2:
			if (parser != NULL && parser->ParseField(6) != 2) {
				int32_t flag_id = parser->m_int_val;
				MapPixelPoint pt;

				parser->ParseField(4);
				pt.m_x = parser->m_int_val;
				parser->ParseField(5);
				pt.m_y = parser->m_int_val;
				if (m_map_core != NULL && m_map_core->m_map_view != NULL)
					m_map_core->m_map_view->NotifyMarkerClick(&pt, -3,
										  flag_id);
			}
			return 0;
		case 3: {
			MapPixelPoint pt;

			parser->ParseField(4);
			pt.m_x = parser->m_int_val;
			parser->ParseField(5);
			pt.m_y = parser->m_int_val;
			parser->ParseField(6);
			m_map_core->m_map_view->NotifyMarkerDragEvent(1, &pt, parser->m_int_val);
			return 0;
		}
		case 4:
			if (parser != NULL && parser->ParseField(6) != 2) {
				int32_t marker_id = parser->m_int_val;
				MapPixelPoint pt;

				parser->ParseField(4);
				pt.m_x = parser->m_int_val;
				parser->ParseField(5);
				pt.m_y = parser->m_int_val;
				parser->ParseField(8);
				double lat = parser->m_double_val;

				parser->ParseField(9);
				m_map_core->m_map_view->NotifyMarkerDragEvent(0, &pt, marker_id);
				m_map_core->m_map_view->m_touched_coord.m_latitude = lat;
				m_map_core->m_map_view->m_touched_coord.m_longitude =
					parser->m_double_val;
			}
			return 0;
		case 5:
			return 0;
		case 6:
			if (m_map_core != NULL && m_map_core->m_map_view != NULL)
				m_map_core->m_map_view->NotifyMapIdle();
			return 0;
		case 15:
			return HandleCenterZoomEvent(parser);
		case 16:
			return HandleZoomEvent(parser);
		case 22:
			if (m_map_core != NULL && m_map_core->m_map_view != NULL)
				m_map_core->m_map_view->NotifyMapReady();
			return 0;
		default:
			return 2;
		}
	}

	if (ev == 100) {
		s_map_tiles_loading = false;
		if (m_map_core != NULL && m_map_core->m_map_view != NULL)
			m_map_core->m_map_view->NotifyTileLoadFinish();
		return 0;
	}
	if (ev == 101) {
		if (m_map_core != NULL && m_map_core->m_map_view != NULL)
			m_map_core->m_map_view->NotifyTileLoadError();
		return 0;
	}
	if (ev == 200)
		return 0;
	return 2;
}

int32_t MapJsEventHandler::Handle(MapJsResultParser *parser)
{
	return Dispatch(parser) == 0 ? 1 : 0;
}

MapJsResponseHandler::MapJsResponseHandler() : m_map_core(NULL)
{
}

MapJsResponseHandler::~MapJsResponseHandler()
{
}

int32_t MapJsResponseHandler::CompleteInvocation(MapJsResultParser *parser)
{
	int32_t atomic_id = 0;

	if (parser != NULL) {
		parser->ParseField(0);
		parser->ParseField(1);
		parser->ParseField(2);
		int32_t error_code = parser->m_int_val;

		parser->ParseField(16);
		atomic_id = parser->m_int_val;
		m_map_core->m_invocation_queue.SignalResult(atomic_id, error_code);
	}
	m_map_core->m_invocation_queue.CompleteUpTo(atomic_id);
	return 0;
}

int32_t MapJsResponseHandler::HandleCountryCheckResponse(MapJsResultParser *parser)
{
	if (parser->ParseField(14) != 2) {
		MapCountryLookup lookup;
		int32_t region_result = lookup.IsRegionFlagged(parser->m_string_val) ? 0 : 255;

		if (parser != NULL) {
			parser->ParseField(0);
			parser->ParseField(1);
			parser->ParseField(2);
			int32_t error_code = parser->m_int_val;

			parser->ParseField(16);
			m_map_core->m_invocation_queue.SignalResultWithValue(
				parser->m_int_val, error_code, region_result);
		}
	}
	return 0;
}

int32_t MapJsResponseHandler::Dispatch(MapJsResultParser *parser)
{
	int32_t cmd = parser->m_command_id;

	if (cmd == 10 || cmd == 11) {
		MapView *view = m_map_core->m_map_view;

		if (view != NULL && view->m_listener != NULL)
			view->m_listener->OnCenterChanged(view, &m_map_core->m_request.m_center);
		return CompleteInvocation(parser);
	}
	if (cmd == 1564)
		return HandleCountryCheckResponse(parser);

	if ((cmd >= 117 && cmd <= 120) || cmd == 144 || cmd == 145 || cmd == 160 ||
	    (cmd >= 209 && cmd <= 211) || (cmd >= 215 && cmd <= 217) ||
	    (cmd >= 266 && cmd <= 270) || cmd == 282 || cmd == 298 || cmd == 299 ||
	    cmd == 314 || cmd == 315 || (cmd >= 330 && cmd <= 333) ||
	    (cmd >= 1434 && cmd <= 1436) || cmd == 1562 || cmd == 1563 ||
	    cmd == 1882 || cmd == 1883)
		return CompleteInvocation(parser);

	return 2;
}

int32_t MapJsResponseHandler::Handle(MapJsResultParser *parser)
{
	return Dispatch(parser) == 0 ? 1 : 0;
}

MapJsRequest::MapJsRequest()
	: m_center(),
	  m_position(),
	  m_zoom_param(0),
	  m_state_flags(0),
	  m_map_type(0),
	  m_marker(),
	  m_marker_alpha(0.0f),
	  m_marker_tilt(0.0f),
	  m_atomic_id(0),
	  m_from_address(NULL),
	  m_to_address(NULL),
	  m_direction_mode(0),
	  m_step_index(0),
	  m_direction_active(false),
	  m_marker_visible_flag(1),
	  m_function_id(0),
	  m_from_coord(),
	  m_to_coord(),
	  m_bounds_sw(),
	  m_height(0),
	  m_width(0),
	  m_layout_flags(0),
	  m_bounds_ne(),
	  m_default_zoom(8),
	  m_tile_mode(0),
	  m_marker_image_url(NULL),
	  m_marker_list(NULL),
	  m_flag_marker(),
	  m_polyline(NULL),
	  m_circle(NULL),
	  m_overlay_id(0),
	  m_overlay_flags(0),
	  m_route_points(NULL),
	  m_route_levels(NULL),
	  m_route_flags(0),
	  m_language(NULL),
	  m_dir_step_list(NULL),
	  m_target_zoom(2),
	  m_current_zoom(2)
{
	m_zoom_center.m_x = 0;
	m_zoom_center.m_y = 0;
	m_new_zoom_center.m_x = 0;
	m_new_zoom_center.m_y = 0;
}

MapJsRequest::~MapJsRequest()
{
	if (m_from_address != NULL)
		delete[] m_from_address;
	if (m_to_address != NULL)
		delete[] m_to_address;
	if (m_language != NULL)
		delete[] m_language;
	m_from_address = NULL;
	m_to_address = NULL;
}

void MapJsRequest::SetFunctionId(int32_t function_id)
{
	m_function_id = function_id;
}

void MapJsRequest::SetTargetZoom(int32_t zoom)
{
	if (zoom < 2)
		m_target_zoom = 2;
	else if (zoom > 21)
		m_target_zoom = 21;
	else
		m_target_zoom = zoom;
}

void MapJsRequest::SetCurrentZoom(int32_t zoom)
{
	if (zoom < 2)
		m_current_zoom = 2;
	else if (zoom > 21)
		m_current_zoom = 21;
	else
		m_current_zoom = zoom;
}

int32_t MapJsRequest::GetTargetZoom() const
{
	return m_target_zoom;
}

int32_t MapJsRequest::GetCurrentZoom() const
{
	return m_current_zoom;
}

void MapJsRequest::SetZoomCenter(const MapPixelPoint *point)
{
	m_zoom_center.m_x = point->m_x;
	m_zoom_center.m_y = point->m_y;
}

void MapJsRequest::SetNewZoomCenter(const MapPixelPoint *point)
{
	m_new_zoom_center.m_x = point->m_x;
	m_new_zoom_center.m_y = point->m_y;
}

int32_t MapJsRequest::SerializeFullPayload(char **out_buf, uint32_t *out_len,
					   MapMarkerList *markers, bool markers_only)
{
	char *marker_json = NULL;
	int32_t fit_bounds = 1;
	int32_t selected = -1;

	if (markers != NULL) {
		fit_bounds = markers->m_fit_bounds;
		selected = markers->m_selected_index;
		marker_json = new char[102400];
		if (marker_json == NULL)
			return 3;
		sce_paf_memset(marker_json, 0, 102400);

		int32_t offset = 0;

		for (int32_t idx = 0; idx < markers->GetCount(); ++idx) {
			MapMarker *mk = markers->GetAt(idx);

			if (idx != 0) {
				sce_paf_memcpy(marker_json + offset, s_comma_sep, 2);
				offset += 2;
			}
			const char *popup = mk->m_popup_text != NULL ? mk->m_popup_text : s_empty_str;

			offset += sce_paf_snprintf(
				marker_json + offset, 102400,
				"{&quot;lat&quot;:&quot;%f&quot;,&quot;lan&quot;: &quot;%f&quot;,"
				"&quot;iconid&quot;:&quot;%d&quot;, &quot;dragable&quot;:&quot;%d&quot;, "
				"&quot;visible&quot;:&quot;%d&quot;, &quot;center&quot;: &quot;%d&quot;, "
				"&quot;popup&quot;:&quot;%s&quot;, &quot;showpopup&quot;:&quot;%d&quot;, "
				"&quot;Id&quot;:&quot;%0d&quot; , &quot;imageurl&quot;:&quot;%s&quot; }",
				mk->m_coord.m_latitude, mk->m_coord.m_longitude, mk->m_icon_id,
				mk->m_draggable ? 1 : 0, mk->m_visible ? 1 : 0,
				mk->m_center_on_marker ? 1 : 0, popup,
				mk->m_show_popup ? 1 : 0, mk->m_marker_id, s_empty_str);
		}
	}

	char *payload = new char[102400];

	if (payload == NULL)
		return 3;
	sce_paf_memset(payload, 0, 102400);

	int32_t written = 0;

	if (markers_only) {
		written = sce_paf_snprintf(
			payload + 30, 102400,
			"'{ &quot;Function&quot;: { &quot;Id&quot;: &quot;%d&quot; , "
			"&quot;AtomicId&quot;: &quot;%d&quot; , &quot;FitBounds&quot;: &quot;%d&quot; , "
			"&quot;Selcted&quot;: &quot;%d&quot; }, &quot;MapMarkerList&quot;: [%s] }'",
			m_function_id, m_atomic_id, fit_bounds, selected,
			marker_json != NULL ? marker_json : s_empty_str);
	} else {
		const char *popup = m_marker.m_popup_text != NULL ? m_marker.GetPopupText()
								  : s_empty_str;
		const char *lang = m_language != NULL ? m_language : s_empty_str;

		written = sce_paf_snprintf(
			payload + 30, 102400,
			"'{ &quot;Function&quot;: { &quot;Id&quot;: &quot;%d&quot;, "
			"&quot;AtomicId&quot;: &quot;%d&quot;}, &quot;Height&quot;:&quot;%d&quot;,"
			"&quot;Width&quot;:&quot;%d&quot;,&quot;MapCenter&quot;:{&quot;lat&quot;:"
			"&quot;%0f&quot;,&quot;lan&quot;: &quot;%0f&quot;},&quot;Position&quot;:"
			"{&quot;lat&quot;:&quot;%0f&quot;,&quot;lan&quot;: &quot;%0f&quot;},"
			"&quot;MapZoomLevel&quot;: { &quot;level&quot;: &quot;%d&quot;}, "
			"&quot;MapType&quot;: { &quot;type&quot;: &quot;%d&quot;}, &quot;MapMarker&quot;:"
			"{&quot;lat&quot;:&quot;%f&quot;,&quot;lan&quot;: &quot;%f&quot;,"
			"&quot;iconid&quot;:&quot;%d&quot;, &quot;dragable&quot;:&quot;%d&quot;, "
			"&quot;visible&quot;:&quot;%d&quot;, &quot;center&quot;: &quot;%d&quot;, "
			"&quot;popup&quot;:&quot;%s&quot;, &quot;showpopup&quot;:&quot;%d&quot;, "
			"&quot;a&quot;:&quot;%0f&quot;,&quot;t&quot;:&quot;%0f&quot;,"
			"&quot;Id&quot;:&quot;%0d&quot; , &quot;imageurl&quot;:&quot;%s&quot; }, "
			"&quot;Flag&quot;:{&quot;lat&quot;:&quot;%f&quot;,&quot;lan&quot;: "
			"&quot;%f&quot;,&quot;iconid&quot;:&quot;%d&quot;, &quot;dragable&quot;:"
			"&quot;%d&quot;, &quot;visible&quot;:&quot;%d&quot;, &quot;center&quot;: "
			"&quot;%d&quot;, &quot;Id&quot;:&quot;%0d&quot; , &quot;imageurl&quot;:"
			"&quot;%s&quot; },  &quot;Direction&quot;: { &quot;from&quot;: &quot;%s&quot;, "
			"&quot;to&quot;: &quot;%s&quot;,&quot;mode&quot;: &quot;%d&quot;, "
			"&quot;stepindex&quot;: &quot;%d&quot; , &quot;FromLat&quot;: &quot;%f&quot; , "
			"&quot;FromLng&quot;: &quot;%f&quot; , &quot;ToLat&quot;: &quot;%f&quot; , "
			"&quot;ToLng&quot;: &quot;%f&quot; }, &quot;Language&quot;: &quot;%s&quot; , "
			"&quot;Error&quot;: &quot;-1&quot; ,  &quot;MapMarkerList&quot;: [%s] }'",
			m_function_id, m_atomic_id, m_height, m_width,
			m_center.m_latitude, m_center.m_longitude,
			m_position.m_latitude, m_position.m_longitude,
			m_target_zoom, m_map_type,
			m_marker.m_coord.m_latitude, m_marker.m_coord.m_longitude,
			m_marker.m_icon_id, m_marker.m_draggable ? 1 : 0,
			m_marker_visible_flag, m_marker.m_center_on_marker ? 1 : 0,
			popup, m_marker.m_show_popup ? 1 : 0,
			(double)m_marker_alpha, (double)m_marker_tilt,
			m_marker.m_marker_id,
			m_marker_image_url != NULL ? m_marker_image_url : s_empty_str,
			m_flag_marker.m_coord.m_latitude, m_flag_marker.m_coord.m_longitude,
			m_flag_marker.m_icon_id, m_flag_marker.m_draggable ? 1 : 0,
			m_flag_marker.m_visible ? 1 : 0,
			m_flag_marker.m_center_on_marker ? 1 : 0,
			m_flag_marker.m_marker_id, s_empty_str,
			s_empty_str, s_empty_str, m_direction_mode, m_step_index,
			m_from_coord.m_latitude, m_from_coord.m_longitude,
			m_to_coord.m_latitude, m_to_coord.m_longitude,
			lang, marker_json != NULL ? marker_json : s_empty_str);
	}

	if (written == -1) {
		delete[] payload;
		*out_buf = NULL;
		*out_len = 0;
		if (marker_json != NULL)
			delete[] marker_json;
		return 1;
	}

	if (marker_json != NULL)
		delete[] marker_json;
	*out_buf = payload;
	*out_len = (uint32_t)(written + 30);
	return 0;
}

int32_t MapJsRequest::SerializeMarkerList(char **out_buf, uint32_t *out_len,
					  MapMarkerList *markers)
{
	return SerializeFullPayload(out_buf, out_len, markers, true);
}

int32_t MapJsRequest::SerializeMapState(char **out_buf, uint32_t *out_len, MapMarkerList *markers)
{
	return SerializeFullPayload(out_buf, out_len, markers, false);
}

int32_t MapJsRequest::SerializePolyline(char **out_buf, uint32_t *out_len)
{
	if (m_polyline == NULL)
		return 1;

	char *points_json = new char[102400];

	if (points_json == NULL)
		return 3;
	sce_paf_memset(points_json, 0, 102400);

	int32_t offset = 0;

	for (int32_t idx = 0; idx < m_polyline->m_point_count; ++idx) {
		if (idx != 0) {
			sce_paf_memcpy(points_json + offset, s_comma_sep, 2);
			offset += 2;
		}
		offset += sce_paf_snprintf(
			points_json + offset, 102400,
			"{ &quot;lat&quot;:&quot;%f&quot;,&quot;lan&quot;: &quot;%f&quot; }",
			m_polyline->m_points[idx].m_latitude,
			m_polyline->m_points[idx].m_longitude);
	}

	char *payload = new char[102400];

	if (payload == NULL)
		return 3;
	sce_paf_memset(payload, 0, 102400);

	int32_t written = sce_paf_snprintf(
		payload + 30, 102400,
		"'{ &quot;Function&quot;: { &quot;Id&quot;: &quot;%d&quot;, "
		"&quot;AtomicId&quot;: &quot;%d&quot;}, &quot;Options&quot;: { "
		"&quot;strokeColor&quot;: &quot;%s&quot;, &quot;strokeOpacity&quot;: "
		"&quot;%f&quot; , &quot;strokeWeight&quot;: &quot;%d&quot;, "
		"&quot;zIndex&quot;: &quot;%d&quot;, &quot;Id&quot;: &quot;%d&quot;}, "
		"&quot;PointList&quot;: [%s] }'",
		m_function_id, m_atomic_id, m_polyline->m_stroke_color,
		(double)m_polyline->m_stroke_opacity, m_polyline->m_stroke_weight,
		m_polyline->m_z_index, m_polyline->m_id, points_json);

	if (written == -1) {
		delete[] payload;
		*out_buf = NULL;
		*out_len = 0;
		return 1;
	}

	delete[] points_json;
	*out_buf = payload;
	*out_len = (uint32_t)(written + 30);
	return 0;
}

int32_t MapJsRequest::SerializeCircle(char **out_buf, uint32_t *out_len)
{
	char *payload = new char[102400];

	if (payload == NULL)
		return 3;
	if (m_circle == NULL)
		return 1;

	sce_paf_memset(payload, 0, 102400);

	int32_t written = sce_paf_snprintf(
		payload + 30, 102400,
		"'{ &quot;Function&quot;: { &quot;Id&quot;: &quot;%d&quot;, "
		"&quot;AtomicId&quot;: &quot;%d&quot;}, &quot;Options&quot;: { "
		"&quot;fillColor&quot;: &quot;%s&quot;, &quot;fillOpacity&quot;: "
		"&quot;%f&quot;, &quot;radius&quot;: &quot;%d&quot;, "
		"&quot;strokeColor&quot;: &quot;%s&quot;, &quot;strokeOpacity&quot;: "
		"&quot;%f&quot; , &quot;strokeWeight&quot;: &quot;%d&quot;, "
		"&quot;zIndex&quot;: &quot;%d&quot; , &quot;Id&quot;: &quot;%d&quot;}, "
		"&quot;Point&quot;: {&quot;lat&quot;:&quot;%f&quot;,&quot;lan&quot;: "
		"&quot;%f&quot;} }'",
		m_function_id, m_atomic_id, m_circle->m_fill_color,
		(double)m_circle->m_fill_opacity, m_circle->m_radius,
		m_circle->m_stroke_color, (double)m_circle->m_stroke_opacity,
		m_circle->m_stroke_weight, m_circle->m_z_index, m_circle->m_id,
		m_circle->m_center.m_latitude, m_circle->m_center.m_longitude);

	*out_buf = payload;
	*out_len = (uint32_t)(written + 30);
	return 0;
}

int32_t MapJsRequest::SerializeZoomCommand(char **out_buf, uint32_t *out_len)
{
	char *payload = new char[102400];

	if (payload == NULL)
		return 3;
	sce_paf_memset(payload, 0, 102400);

	const char *popup = m_marker.m_popup_text != NULL ? m_marker.GetPopupText() : s_empty_str;
	const char *points = m_route_points != NULL ? m_route_points : s_empty_str;
	const char *levels = m_route_levels != NULL ? m_route_levels : s_empty_str;

	int32_t written = sce_paf_snprintf(
		payload + 30, 102400,
		"'{ &quot;Function&quot;: { &quot;Id&quot;: &quot;%d&quot;, "
		"&quot;AtomicId&quot;: &quot;%d&quot; }, &quot;Height&quot;:&quot;%d&quot;,"
		"&quot;Width&quot;:&quot;%d&quot;,&quot;MapCenter&quot;:{&quot;lat&quot;:"
		"&quot;%0f&quot;,&quot;lan&quot;: &quot;%0f&quot;},&quot;Position&quot;:"
		"{&quot;lat&quot;:&quot;%0f&quot;,&quot;lan&quot;: &quot;%0f&quot;}, "
		"&quot;MapZoomCenter&quot;:{&quot;cssX&quot;:&quot;%d&quot;,&quot;cssY&quot;:"
		"&quot;%d&quot;}, &quot;MapNewZoomCenter&quot;:{&quot;cssX&quot;:&quot;%d&quot;,"
		"&quot;cssY&quot;:&quot;%d&quot;}, &quot;MapZoomLevel&quot;: { &quot;level&quot;: "
		"&quot;%d&quot;}, &quot;MapType&quot;: { &quot;type&quot;: &quot;%d&quot;}, "
		"&quot;MapMarker&quot;:{&quot;lat&quot;:&quot;%f&quot;,&quot;lan&quot;: "
		"&quot;%f&quot;,&quot;iconid&quot;:&quot;%d&quot;, &quot;dragable&quot;:"
		"&quot;%d&quot;, &quot;visible&quot;:&quot;%d&quot;, &quot;center&quot;: "
		"&quot;%d&quot;, &quot;popup&quot;:&quot;%s&quot;, &quot;showpopup&quot;:"
		"&quot;%d&quot;, &quot;a&quot;:&quot;%0f&quot;,&quot;t&quot;:&quot;%0f&quot;,"
		"&quot;Id&quot;:&quot;%0d&quot; , &quot;imageurl&quot;:&quot;%s&quot; }, "
		"&quot;Direction&quot;: { &quot;from&quot;: &quot;%s&quot;, &quot;to&quot;: "
		"&quot;%s&quot;,&quot;mode&quot;: &quot;%d&quot;, &quot;stepindex&quot;: "
		"&quot;%d&quot; , &quot;FromLat&quot;: &quot;%f&quot; , &quot;FromLng&quot;: "
		"&quot;%f&quot; , &quot;ToLat&quot;: &quot;%f&quot; , &quot;ToLng&quot;: "
		"&quot;%f&quot; , &quot;Points&quot;: &quot;%s&quot; , &quot;Levels&quot;: "
		"&quot;%s&quot; } , &quot;Error&quot;: &quot;-1&quot;, &quot;Url&quot;: "
		"&quot;%s&quot; }'",
		m_function_id, m_atomic_id, m_height, m_width,
		m_center.m_latitude, m_center.m_longitude,
		m_position.m_latitude, m_position.m_longitude,
		m_zoom_center.m_x, m_zoom_center.m_y,
		m_new_zoom_center.m_x, m_new_zoom_center.m_y,
		m_target_zoom, m_map_type,
		m_marker.m_coord.m_latitude, m_marker.m_coord.m_longitude,
		m_marker.m_icon_id, m_marker.m_draggable ? 1 : 0,
		m_marker_visible_flag, m_marker.m_center_on_marker ? 1 : 0,
		popup, m_marker.m_show_popup ? 1 : 0,
		(double)m_marker_alpha, (double)m_marker_tilt,
		m_marker.m_marker_id,
		m_marker_image_url != NULL ? m_marker_image_url : s_empty_str,
		s_empty_str, s_empty_str, m_direction_mode, m_step_index,
		m_from_coord.m_latitude, m_from_coord.m_longitude,
		m_to_coord.m_latitude, m_to_coord.m_longitude,
		points, levels, s_empty_str);

	*out_buf = payload;
	*out_len = (uint32_t)(written + 30);
	return 0;
}

int32_t MapJsRequest::SerializeDirectionSteps(char **out_buf, uint32_t *out_len)
{
	char *steps_json = new char[1];

	sce_paf_memset(steps_json, 0, 1);

	if (m_dir_step_list != NULL) {
		delete[] steps_json;

		const char *step_fmt =
			"{&quot;points&quot;:&quot;%s&quot;,&quot;levels&quot;:&quot;%s&quot;,"
			"&quot;lat&quot;:&quot;%f&quot;,&quot;lan&quot;:&quot;%f&quot;}";
		size_t sep_len = sce_paf_strlen(s_comma_sep);
		size_t base_len = sce_paf_strlen(step_fmt) - 2 * sce_paf_strlen("%f") -
				  2 * sce_paf_strlen("%s") - sep_len;

		sce_paf_printf_dummy();

		int32_t total_len = 0;

		for (int32_t idx = 0; idx < m_dir_step_list->m_count; ++idx) {
			MapDirStep &step = m_dir_step_list->m_steps[idx];
			char coord_buf[36];

			sce_paf_memset(coord_buf, 0, 33);
			total_len += (int32_t)(2 * base_len) + step.m_points_len +
				     step.m_levels_len +
				     sce_paf_snprintf(coord_buf, 32, "%f%f",
						      step.m_coord.m_latitude,
						      step.m_coord.m_longitude);
		}

		steps_json = new char[(size_t)(total_len + 1)];
		sce_paf_memset(steps_json, 0, (size_t)(total_len + 1));

		int32_t offset = 0;

		for (int32_t idx = 0; idx < m_dir_step_list->m_count; ++idx) {
			MapDirStep &step = m_dir_step_list->m_steps[idx];

			if (offset != 0) {
				size_t len = sce_paf_strlen(s_comma_sep);

				sce_paf_memcpy(steps_json + offset, s_comma_sep, len);
				offset += (int32_t)sce_paf_strlen(s_comma_sep);
			}
			offset += sce_paf_snprintf(steps_json + offset, (size_t)total_len, step_fmt,
						   step.m_points, step.m_levels,
						   step.m_coord.m_latitude,
						   step.m_coord.m_longitude);
		}
	}

	char *payload = new char[102400];

	if (payload == NULL)
		return 3;
	sce_paf_memset(payload, 0, 102400);

	int32_t written = sce_paf_snprintf(
		payload + 30, 102400,
		"'{&quot;Function&quot;:{&quot;Id&quot;:&quot;%d&quot;,&quot;AtomicId&quot;:"
		"&quot;%d&quot;},&quot;MapMarker&quot;:{&quot;iconid&quot;:&quot;%d&quot;},"
		"&quot;Direction&quot;:{&quot;from&quot;:&quot;%s&quot;,&quot;to&quot;:"
		"&quot;%s&quot;,&quot;mode&quot;:&quot;%d&quot;,&quot;stepindex&quot;:"
		"&quot;%d&quot;,&quot;FromLat&quot;:&quot;%f&quot;,&quot;FromLng&quot;:"
		"&quot;%f&quot;,&quot;ToLat&quot;:&quot;%f&quot;,&quot;ToLng&quot;:"
		"&quot;%f&quot;},&quot;Error&quot;:&quot;-1&quot;,&quot;DirStepList&quot;:[%s]}'",
		m_function_id, m_atomic_id, m_marker.m_icon_id,
		s_empty_str, s_empty_str, m_direction_mode, m_step_index,
		m_from_coord.m_latitude, m_from_coord.m_longitude,
		m_to_coord.m_latitude, m_to_coord.m_longitude, steps_json);

	delete[] steps_json;
	*out_buf = payload;
	*out_len = (uint32_t)(written + 30);
	return 0;
}

static char *WrapJsBinder(char *payload, uint32_t length)
{
	sce_paf_memcpy(payload, s_js_binder_prefix, 30);
	sce_paf_memcpy(payload + length, s_js_binder_suffix, 3);
	return payload;
}

char *MapJsScriptFormatter::FormatZoomCommand(MapJsRequest *request)
{
	char *buf = NULL;
	uint32_t len = 0;

	if (request->SerializeZoomCommand(&buf, &len) != 0)
		return NULL;
	return WrapJsBinder(buf, len);
}

char *MapJsScriptFormatter::FormatMarkerList(MapJsRequest *request)
{
	char *buf = NULL;
	uint32_t len = 0;

	if (request->SerializeMarkerList(&buf, &len, request->m_marker_list) != 0)
		return NULL;
	return WrapJsBinder(buf, len);
}

char *MapJsScriptFormatter::FormatDirectionSteps(MapJsRequest *request)
{
	char *buf = NULL;
	uint32_t len = 0;

	if (request->SerializeDirectionSteps(&buf, &len) != 0)
		return NULL;
	return WrapJsBinder(buf, len);
}

char *MapJsScriptFormatter::FormatMapState(MapJsRequest *request)
{
	char *buf = NULL;
	uint32_t len = 0;

	if (request->SerializeMapState(&buf, &len, request->m_marker_list) != 0)
		return NULL;
	return WrapJsBinder(buf, len);
}

char *MapJsScriptFormatter::FormatPolyline(MapJsRequest *request)
{
	char *buf = NULL;
	uint32_t len = 0;

	if (request->SerializePolyline(&buf, &len) != 0)
		return NULL;
	return WrapJsBinder(buf, len);
}

char *MapJsScriptFormatter::FormatCircle(MapJsRequest *request)
{
	char *buf = NULL;
	uint32_t len = 0;

	if (request->SerializeCircle(&buf, &len) != 0)
		return NULL;
	return WrapJsBinder(buf, len);
}

__attribute__((noinline)) MapJsBridge::MapJsBridge(MapCore *core)
	: m_bridge_flags(0),
	  m_map_core(core),
	  m_pending_handle(NULL),
	  m_pending_count(0),
	  m_loaded(false)
{
}

__attribute__((noinline)) int32_t MapJsBridge::IsReady() const
{
	return 0;
}

__attribute__((noinline)) int32_t MapJsBridge::LoadMapPage()
{
	return m_map_core->LoadPageUrl(s_load_map_url);
}

int32_t MapJsBridge::ExecuteRequest(MapJsRequest *request, int32_t serializer_kind)
{
	MapJsScriptFormatter formatter;
	char *script = NULL;

	switch (serializer_kind) {
	case 0:
		script = formatter.FormatMapState(request);
		break;
	case 1:
		script = formatter.FormatMarkerList(request);
		break;
	case 2:
		script = formatter.FormatZoomCommand(request);
		break;
	case 3:
		script = formatter.FormatPolyline(request);
		break;
	case 4:
		script = formatter.FormatCircle(request);
		break;
	case 5:
		script = formatter.FormatDirectionSteps(request);
		break;
	default:
		break;
	}

	int32_t ret = m_map_core->ExecuteJavaScriptUrl(script);

	if (script != NULL)
		delete[] script;
	return ret;
}

MapJsResultParser::MapJsResultParser()
	: m_raw_message(NULL),
	  m_double_val(0.0),
	  m_int_val(0),
	  m_bool_val(0),
	  m_string_val(NULL),
	  m_string_kind(0),
	  m_token_buf(NULL),
	  m_token_len(0),
	  m_command_id(0),
	  m_event_id(0)
{
}

MapJsResultParser::~MapJsResultParser()
{
}

bool MapJsResultParser::SetInput(const char *data, int32_t length)
{
	if (s_parser_first_use) {
		sce_paf_printf_dummy();
		s_parser_first_use = false;
	}

	string *str = new string();

	if (str != NULL) {
		if (data != NULL && length != 0 && *data != '\0')
			str->assign(data, (size_t)length);
	}
	m_raw_message = str;
	return str == NULL;
}

int32_t MapJsResultParser::Clear()
{
	if (m_raw_message != NULL) {
		delete m_raw_message;
		m_raw_message = NULL;
	}
	if (m_token_buf != NULL) {
		delete[] m_token_buf;
		m_token_buf = NULL;
	}
	return 0;
}

int32_t MapJsResultParser::ConvertToken(int32_t field_index)
{
	if (m_token_buf == NULL)
		return 2;

	switch (s_js_field_types[field_index]) {
	case 1:
		m_double_val = sce_paf_strtod(m_token_buf, NULL);
		return 0;
	case 2:
		m_bool_val = (sce_paf_atoi(m_token_buf) == 1) ? 1 : 0;
		return 0;
	case 3:
		m_int_val = sce_paf_atoi(m_token_buf);
		return 0;
	case 4:
		m_string_val = m_token_buf;
		m_string_kind = 2;
		return 0;
	default:
		return 2;
	}
}

int32_t MapJsResultParser::ParseField(int32_t field_index)
{
	m_double_val = 0.0;
	m_int_val = 0;
	m_bool_val = 0;
	m_string_val = NULL;
	m_string_kind = 0;

	if (m_token_buf != NULL) {
		delete[] m_token_buf;
		m_token_buf = NULL;
	}
	m_token_len = 0;

	if (m_raw_message == NULL || m_raw_message->length() == 0)
		return 2;

	size_t delim_pos = m_raw_message->find(s_field_delimiter);

	if (delim_pos == string::npos)
		return 2;

	size_t start = 0;
	int32_t current_index = 0;

	while (true) {
		if (current_index == field_index && delim_pos > start) {
			m_token_buf = new char[1024];
			if (m_token_buf != NULL)
				break;
		}
		start = delim_pos + 2;
		delim_pos = m_raw_message->find(s_field_delimiter, start);
		if (++current_index > 17 || delim_pos == string::npos)
			return 2;
	}

	sce_paf_memset(m_token_buf, 0, 1024);

	string token = m_raw_message->substr(start, delim_pos - start);

	sce_paf_memcpy(m_token_buf, token.c_str(), token.length());
	return ConvertToken(field_index);
}

bool MapCountryLookup::IsRegionFlagged(const char *country_code) const
{
	for (uint32_t idx = 0; idx < 240; ++idx) {
		if (sce_paf_strncasecmp(s_country_table[idx].m_code, country_code, 2) == 0)
			return s_country_table[idx].m_region_flag != 0;
	}
	return false;
}

int32_t MapLocationManager::Bind(MapView *view)
{
	if (view->m_map_core == NULL)
		return 0;

	if (s_location_manager == NULL) {
		s_location_manager = new MapLocationManager();
		if (s_location_manager == NULL)
			return 0;
		s_location_manager->m_client = NULL;
		s_location_manager->m_map_view = NULL;
	}

	MapView *owner_view = view->m_map_core->m_map_view;

	s_location_manager->m_map_view = owner_view;
	if (s_location_manager->m_client == NULL) {
		MapLocationClient *client = new MapLocationClient();

		if (client != NULL) {
			client->m_handle = NULL;
			client->m_active = 0;
			client->m_map_view = owner_view;
			client->m_mode = 4;
		}
		s_location_manager->m_client = client;
	}

	return s_location_manager->m_client != NULL ? 0 : 3;
}

MapMarker::MapMarker()
	: m_draggable(false),
	  m_title(NULL),
	  m_has_popup(false),
	  m_show_popup(false),
	  m_has_image(false),
	  m_popup_text(NULL),
	  m_coord(),
	  m_anchor_x(0),
	  m_anchor_y(0),
	  m_marker_id(0),
	  m_visible(false),
	  m_center_on_marker(false),
	  m_icon_id(0)
{
	++s_next_marker_id;

	MapMarkerRegistryNode *node = new MapMarkerRegistryNode();

	if (node != NULL) {
		node->m_marker = this;
		node->m_prev = NULL;
		node->m_next = NULL;
	}

	if (s_marker_registry_head == NULL) {
		s_marker_registry_head = node;
	} else {
		MapMarkerRegistryNode *tail = s_marker_registry_head;

		while (tail->m_next != NULL)
			tail = tail->m_next;
		tail->m_next = node;
	}

	if (node != NULL)
		m_marker_id = s_next_marker_id;
}

MapMarker::~MapMarker()
{
	m_draggable = false;
	if (m_title != NULL)
		delete[] m_title;
	m_has_popup = false;
	m_has_image = false;
	if (m_popup_text != NULL)
		delete[] m_popup_text;
	m_title = NULL;
	m_popup_text = NULL;
}

const char *MapMarker::GetPopupText() const
{
	return m_popup_text;
}

MapMarker *MapMarkerList::GetAt(int32_t index) const
{
	MapMarkerNode *node = m_head;

	if (index == 0 && node != NULL)
		return node->m_marker;
	if (node == NULL)
		return NULL;

	int32_t pos = 0;

	while (node != NULL) {
		++pos;
		node = node->m_next;
		if (pos == index)
			return node != NULL ? node->m_marker : NULL;
	}
	return NULL;
}

int32_t MapMarkerList::GetCount() const
{
	return m_count;
}

void MapCanvasObserver::OnStartLoad()
{
	if (m_map_core != NULL) {
		MapCanvasEvent ev(2, 0);

		m_map_core->OnCanvasEvent(&ev);
	}
}

void MapCanvasObserver::OnFinishLoad()
{
	if (m_map_core != NULL) {
		MapCanvasEvent ev(3, 0);

		m_map_core->OnCanvasEvent(&ev);
	}
}

void MapCanvasObserver::OnLoadResult(int32_t status)
{
	if (m_map_core != NULL) {
		int32_t detail = 0;

		if (status == 1)
			detail = 18;
		else if (status == 2)
			detail = 19;
		MapCanvasEvent ev(3, detail);

		m_map_core->OnCanvasEvent(&ev);
	}
}

void MapCanvasObserver::OnLoadError(int32_t error_code, const common::String &message)
{
	(void)message;
	if (m_map_core != NULL) {
		MapCanvasEvent ev(1, error_code);

		m_map_core->OnCanvasEvent(&ev);
	}
}

void MapCanvasObserver::OnRedirect()
{
	if (m_map_core != NULL) {
		MapCanvasEvent ev(2, 0);

		m_map_core->OnCanvasEvent(&ev);
	}
}

void MapCanvasObserver::OnBrowserComponentReady()
{
	if (m_map_core != NULL) {
		MapCanvasEvent ev(0, 0);

		m_map_core->OnCanvasEvent(&ev);
	}
}

void MapCanvasObserver::OnProgressChanged(int32_t progress)
{
	(void)progress;
}

void MapCanvasObserver::OnTitleChanged(const common::String &title)
{
	(void)title;
}

void MapCanvasObserver::OnLocationChanged(const Url &url)
{
	(void)url;
}

void MapCanvasObserver::OnSecurityStatusChanged(int32_t status)
{
	(void)status;
}

void MapCanvasObserver::OnScrollOffsetChanged(const math::v2 &offset)
{
	(void)offset;
}

void MapCanvasObserver::OnContextMenu(const ContextMenuInfo &info)
{
	(void)info;
}

void MapCanvasObserver::OnOpenNewWindow(const Url &url)
{
	(void)url;
}

void MapCanvasObserver::OnWindowCloseRequested()
{
}

void MapCanvasObserver::OnOpenNewWindowWithTarget(const Url &url, const common::String &target)
{
	(void)url;
	(void)target;
	if (m_map_core != NULL) {
		MapCanvasEvent ev(1, 250);

		m_map_core->OnCanvasEvent(&ev);
	}
}

void MapCanvasObserver::OnNavigationStateChanged(int32_t state)
{
	(void)state;
}

void MapCanvasObserver::OnSelectPopupClosed()
{
}

void MapCanvasObserver::OnHistoryChanged(bool can_go_back, bool can_go_forward)
{
	(void)can_go_back;
	(void)can_go_forward;
}

}
}
