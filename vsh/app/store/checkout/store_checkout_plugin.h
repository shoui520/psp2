/*
 * store_checkout_plugin's interface (vs0:sys/external/store_checkout_plugin.suprx): what the
 * checkout dialog's plugin (libcdlg_checkout, ./plugin) gives it. The plugin's module hands out
 * the interface through paf::Plugin::GetInterface(STORE_CHECKOUT_PLUGIN_INTERFACE_VERSION); the
 * dialog passes it the parameter below and its colours. No Sony name is known for any of it:
 * the names are ours, and the fields the dialog only clears keep neutral names until
 * store_checkout_plugin is reversed.
 */
#ifndef STORE_CHECKOUT_PLUGIN_H
#define STORE_CHECKOUT_PLUGIN_H

#include <scetypes.h>
#include <std/string>
#include <std/vector>

#define STORE_CHECKOUT_PLUGIN_INTERFACE_VERSION	1

/* StoreCheckoutPluginParam::type: the only one the dialog gives */
#define STORE_CHECKOUT_PLUGIN_PARAM_TYPE_DIALOG	3

/* StoreCheckoutPluginParam::flags: the dialog's version 2 modes, and the install mode */
#define STORE_CHECKOUT_PLUGIN_FLAG_MODE2	0x8000000
#define STORE_CHECKOUT_PLUGIN_FLAG_INSTALL	0x4000000

/* what the dialog asks for: its modes, as store_checkout_plugin's */
#define STORE_CHECKOUT_PLUGIN_MODE_NONE		0
#define STORE_CHECKOUT_PLUGIN_MODE_CHECKOUT	1
#define STORE_CHECKOUT_PLUGIN_MODE_DOWNLOAD	5
#define STORE_CHECKOUT_PLUGIN_MODE_SYSTEM1	6
#define STORE_CHECKOUT_PLUGIN_MODE_SYSTEM2	8
#define STORE_CHECKOUT_PLUGIN_MODE_SYSTEM3	9
#define STORE_CHECKOUT_PLUGIN_MODE_APPLY	12

struct StoreCheckoutPluginParam {
	SceUInt32 sdkVersion;
	SceUInt32 flags;
	SceInt32 type;
	paf::vector<paf::string> skuIds;
	SceUInt32 skuNum;
	paf::vector<paf::string> reservedList;
	SceInt32 reserved48;
	paf::vector<SceInt32> reservedValues;
	SceInt32 reserved68;
	paf::string reservedString72;
	paf::string reservedString84;
	paf::string reservedString96;
	paf::string reservedString108;
	paf::string reservedString120;
	SceInt32 reserved132;
	paf::string serviceId;
	SceInt32 reserved148;
	paf::string reservedString152;
	paf::string reservedString164;
	paf::string reservedString176;
	paf::string reservedString188;
	paf::string reservedString200;
	paf::string reservedString212;
};

typedef char StoreCheckoutPluginParamServiceId[__builtin_offsetof(StoreCheckoutPluginParam, serviceId) == 136 ? 1 : -1];
typedef char StoreCheckoutPluginParamSize[sizeof(StoreCheckoutPluginParam) == 224 ? 1 : -1];

/* the dialog's result: the checkout's result and the NP error */
typedef void (*StoreCheckoutPluginResultCallback)(SceInt32 result, SceInt32 npError);

/* the interface store_checkout_plugin's module gives */
struct StoreCheckoutPluginInterface {
	void (*reserved)(void);
	/* the dialog's caller asked to close it */
	void (*abort)(void);
	void (*start)(SceInt32 mode, StoreCheckoutPluginParam *param);
	void (*setResultCallback)(StoreCheckoutPluginResultCallback callback);
	/* RGBA, 0 to 1 */
	void (*setBgColor)(const float *color);
	void (*setDimmerColor)(const float *color);
};

#endif
