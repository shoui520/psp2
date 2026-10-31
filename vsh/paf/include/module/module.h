/*
 * paf::Module: a PRX loaded and started once however many Modules name it.
 * The modules loaded are kept by path; the last Module of a path to go
 * stops and unloads it.
 */
#ifndef PAF_MODULE_MODULE_H
#define PAF_MODULE_MODULE_H

#include <stdint.h>
#include <scetypes.h>
#include <std/string>
#include <std/map>

namespace paf {

class ModuleImpl;

class Module {
public:
	enum Option {
		Option_None = 0,
		/* module_start is given the Module, to set its interfaces */
		Option_UseInterface = 1,
		Option_LoadTypeCdlg = 2
	};

	class ExtraOption {
	};

	Module(const char *name, const char *funcname = NULL, int32_t option = 0,
	       const ExtraOption *extra_option = NULL);
	~Module();

	void SetInterface(int32_t version, void *interface);
	void *GetInterface(int32_t version) const;
	int32_t GetStatus();

	ModuleImpl *impl;
};

class ModuleImpl {
public:
	ModuleImpl(const char *name, int32_t option, const Module::ExtraOption *extra_option);
	~ModuleImpl();

	void Start(Module *referer, const char *funcname);

	string name;
	SceUID handle;
	map<int32_t, void *> interface_table;
	int32_t ref_count;
	int32_t option;
	int32_t result;
};

}

/* When a module was last loaded or let go; no Sony name found for the NID. */
extern "C" SceUInt64 ScePafMisc_A8133C0A(void);

#endif /* PAF_MODULE_MODULE_H */
