#ifndef PAF_PLUGIN_H
#define PAF_PLUGIN_H

#include "paf_types.h"
#include "std/string.h"
#include "std/map.h"
#include "std/list.h"
#include "timer/timer.h"
#include "resource/resource_obj.h"

typedef struct Plugin Plugin;
typedef struct SurfacePool SurfacePool;
typedef struct EnvironmentParam EnvironmentParam;
typedef struct PluginInitParam2 PluginInitParam2;

typedef enum TransitionType {
	TransitionType_None = 0,
	TransitionType_SlideFromTop = 1,
	TransitionType_SlideFromBottom = 2,
	TransitionType_SlideFromLeft = 3,
	TransitionType_SlideFromRight = 4
} TransitionType;

typedef enum PageOption {
	PageOption_None = 0,
	PageOption_Create = 1
} PageOption;

typedef struct InitEventFunctionEntry2 InitEventFunctionEntry2;

typedef struct PluginPageOpenParam {
	int32_t draw_layer;
	int32_t draw_mask;
	int32_t overwrite_draw_priority;
	int32_t option;
	InitEventFunctionEntry2 *event_handler_table;
	unsigned char fade;
	float fade_time_ms;
	TransitionType transition_type;
	Timer *timer;
	EnvironmentParam *env_param;
	int32_t graphics_flag;
} PluginPageOpenParam;

typedef char PluginPageOpenParamSizeCheck[sizeof(PluginPageOpenParam) == 44 ? 1 : -1];

typedef enum PluginOption {
	Option_None = 0,
	Option_AutoOpen = 1,
	Option_AutoActivate = 2,
	Option_RememberSurface = 4,
	Option_MakeSurfaceFirst = 8,
	Option_WaitWidgetActuallyDestroyed = 16,
	Option_ResourceLoadWithDebugSymbol = 32,
	Option_Default = 28
} PluginOption;

typedef enum PluginArgumentType {
	ArgumentType_Int = 0,
	ArgumentType_Float = 1,
	ArgumentType_String = 2
} PluginArgumentType;

typedef struct PluginArgument {
	PluginArgumentType type;
	union {
		int32_t i;
		float f;
	};
	PafString str;
} PluginArgument;

typedef struct PluginArgumentEntry {
	PafString first;
	PluginArgument second;
} PluginArgumentEntry;

typedef struct PluginArgumentMapNode {
	struct PluginArgumentMapNode *_Parent;
	struct PluginArgumentMapNode *_Left;
	struct PluginArgumentMapNode *_Right;
	PluginArgumentEntry _Myval;
	unsigned char _Color;
	unsigned char _Isnil;
} PluginArgumentMapNode;

typedef struct PluginArgumentMap {
	PluginArgumentMapNode *_Myhead;
	size_t _Mysize;
	unsigned char _allocator;
	unsigned char _compare;
} PluginArgumentMap;

struct PluginInitParam2 {
	int32_t draw_priority;
	uint32_t surface_pool_size;
	int32_t texture_load_filter;
	int32_t graphics_flag;
};

typedef struct PluginInitParam {
	PafString name;
	PafString caller_name;
	void (*set_param_func)(PluginInitParam2 *param);
	void (*init_func)(Plugin *plugin);
	void (*start_func)(Plugin *plugin);
	void (*stop_func)(Plugin *plugin);
	void (*exit_func)(Plugin *plugin);
	PafString resource_file;
	uint8_t *resource_data;
	uint32_t resource_data_size;
	Locale locale;
	uint32_t private_heap_size;
	HeapAllocator *private_heap;
	uint32_t decode_heap_size;
	HeapAllocator *decode_heap;
	SurfacePool *surface_pool;
	int32_t draw_priority;
	PluginArgumentMap arguments;
	PafString module_file;
	int32_t module_interface_version;
	int32_t module_option;
	int32_t option;
	void (*notify_callback)(int32_t event, void *userdata);
	void (*unload_callback)(void *userdata);
	void *param;
	EnvironmentParam *env_param;
	int32_t graphics_flag;
} PluginInitParam;

typedef struct Module Module;
typedef struct Widget Widget;
typedef struct Scene Scene;
typedef struct VersatileParamManager VersatileParamManager;
typedef struct InitEventFunctionEntry InitEventFunctionEntry;
typedef struct EventFunctionEntry EventFunctionEntry;
typedef struct LayoutFunctionEntry LayoutFunctionEntry;

typedef struct DrawOrderElement {
	Scene *page;
	int32_t view_priority;
	int32_t page_priority;
} DrawOrderElement;

typedef struct DrawOrderVector {
	DrawOrderElement *_data;
	size_t _size;
	size_t _capacity;
	unsigned char _allocator;
} DrawOrderVector;

typedef struct PluginNotifyNode {
	struct PluginNotifyNode *prev;
	struct PluginNotifyNode *next;
	void (*val)(int32_t status, void *userdata);
} PluginNotifyNode;

struct Plugin {
	unsigned char initialized;
	PafString name;
	PafString caller_name;
	void (*set_param_func)(PluginInitParam2 *param);
	void (*init_func)(Plugin *plugin);
	void (*start_func)(Plugin *plugin);
	void (*stop_func)(Plugin *plugin);
	void (*exit_func)(Plugin *plugin);
	Module *module;
	InitEventFunctionEntry *init_event_handler_table;
	EventFunctionEntry *event_handler_table;
	PafMap event_handler_name_table;
	LayoutFunctionEntry *layout_handler_table;
	PafMap layout_handler_name_table;
	PafMap interface_table;
	PafMap widget_table;
	PafMap create_page_table;
	PafMap surface_table;
	PafMap add_texture_table;
	PafMap template_table;
	DrawOrderVector draw_order;
	ResourceObj *rscobj;
	VersatileParamManager *style_param_manager;
	VersatileParamManager *obj_param_manager;
	VersatileParamManager *sound_param_manager;
	HeapAllocator *private_heap;
	unsigned char private_heap_created;
	HeapAllocator *decode_heap;
	unsigned char decode_heap_created;
	void *surface_pool_addr;
	SurfacePool *surface_pool;
	PluginArgumentMap arguments;
	int32_t option;
	PafList notify_callback_list;
	PafList unload_callback_list;
	void *param;
	CxmlElement *last_active_page_node;
	int32_t draw_priority;
	Locale locale;
	unsigned char texture_table_loaded;
	int32_t tex_load_filter;
	void (*monitoring_callback_pageopen)(Plugin *plugin, CxmlElement *node);
	void (*monitoring_callback_pageactivate)(Plugin *plugin, CxmlElement *node);
	void (*monitoring_callback_pageclose)(Plugin *plugin, CxmlElement *node);
	uint8_t *resource_data;
	uint32_t resource_data_size;
	uint32_t page_debug_focus_enabled;
	int32_t page_debug_mode;
	EnvironmentParam *env_param;
	unsigned char env_param_created;
	unsigned char graphics_flag;
	unsigned char unloading;
};

typedef char DrawOrderElementSizeCheck[sizeof(DrawOrderElement) == 12 ? 1 : -1];
typedef char DrawOrderVectorSizeCheck[sizeof(DrawOrderVector) == 16 ? 1 : -1];
typedef char PluginSizeCheck[sizeof(Plugin) == 316 ? 1 : -1];
typedef char PluginResourceOffsetCheck[offsetof(Plugin, rscobj) == 176 ? 1 : -1];
typedef char PluginParamOffsetCheck[offsetof(Plugin, param) == 256 ? 1 : -1];
typedef char PluginEnvironmentOffsetCheck[offsetof(Plugin, env_param) == 308 ? 1 : -1];

typedef char PluginArgumentSizeCheck[sizeof(PluginArgument) == 20 ? 1 : -1];
typedef char PluginArgumentMapNodeSizeCheck[sizeof(PluginArgumentMapNode) == 48 ? 1 : -1];
typedef char PluginArgumentMapSizeCheck[sizeof(PluginArgumentMap) == 12 ? 1 : -1];
typedef char PluginInitParam2SizeCheck[sizeof(PluginInitParam2) == 16 ? 1 : -1];
typedef char PluginInitParamSizeCheck[sizeof(PluginInitParam) == 148 ? 1 : -1];

#define PluginInitParam2Construct ScePafToplevel_EA5FFA2A
#define PluginArgumentToInt ScePafToplevel_15B7D17E
#define PluginArgumentToFloat ScePafToplevel_CA9CA34D
#define PluginArgumentToString ScePafToplevel_1A287262
#define PluginInitParam2ConstructBase ScePafToplevel_5498F7B7
#define PluginInitParamConstruct ScePafToplevel_400F84CE
#define PluginInitParamConstructBase ScePafToplevel_58C1A580
#define PluginLoadAsync ScePafToplevel_F702E40A
#define PluginStop ScePafToplevel_2645FC81
#define PluginStart ScePafToplevel_3451A886
#define PluginPageOpenParamConstruct ScePafToplevel_34FE1331
#define PluginPageOpenParamConstructBase ScePafToplevel_814052CF
#define PluginPageOpen ScePafToplevel_4B5FE871
#define PluginPageActivate ScePafToplevel_856092B5
#define PluginGetArg ScePafToplevel_20D1E49B
#define PluginNotify ScePafToplevel_CC31A095

PluginInitParam2 *PluginInitParam2Construct(PluginInitParam2 *self);
int32_t PluginArgumentToInt(const PluginArgument *self);
float PluginArgumentToFloat(const PluginArgument *self);
PafString *PluginArgumentToString(PafString *result, const PluginArgument *self);
PluginInitParam2 *PluginInitParam2ConstructBase(PluginInitParam2 *self);
PluginInitParam *PluginInitParamConstruct(PluginInitParam *self);
PluginInitParam *PluginInitParamConstructBase(PluginInitParam *self);
PluginArgumentMap *PluginArgumentMapClear(PluginArgumentMap *self);
void PluginLoadAsync(const PluginInitParam *param,
		     void (*finish_callback)(Plugin *plugin), int32_t option);
void PluginStop(Plugin *self);
void PluginStart(Plugin *self);
PluginPageOpenParam *PluginPageOpenParamConstruct(PluginPageOpenParam *self);
PluginPageOpenParam *PluginPageOpenParamConstructBase(PluginPageOpenParam *self);
Scene *PluginPageOpen(Plugin *self, const CxmlElement *node,
		    const PluginPageOpenParam *param);
void PluginPageActivate(Plugin *self, const CxmlElement *node);
void PluginGetArg(PluginArgument *result, const Plugin *self,
		  const char *key, unsigned char *exist);
void PluginNotify(const Plugin *self, int32_t status);
void PluginArgumentMapFind(PluginArgumentMapNode **result,
			   const PluginArgumentMap *self, const PafString *key);

#endif
