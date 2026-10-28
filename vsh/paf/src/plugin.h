#ifndef PAF_PLUGIN_H
#define PAF_PLUGIN_H

#include "paf_types.h"
#include "std/string.h"

typedef struct Plugin Plugin;
typedef struct HeapAllocator HeapAllocator;
typedef struct SurfacePool SurfacePool;
typedef struct EnvironmentParam EnvironmentParam;
typedef struct PluginInitParam2 PluginInitParam2;

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

#endif
