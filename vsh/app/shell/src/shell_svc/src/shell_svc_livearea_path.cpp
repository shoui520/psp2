/*
 * SceShell shell_svc: LiveAreaUtil path resolution & app mount helpers
 * 3.65-CEX: 0x811B918C - 0x811B99E0 (sti 0x811B996E)
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/map>
#include <std/string>
#include <buffer/buffer.h>
#include <common/shared_ptr.h>
#include <plugin.h>

#include "../../../plugins.h"
#include "../../livespace_db/lsdb_keys.h"

struct LsdbRowAttrVal {
	int type;
	int pad;
	union {
		SceInt64 intVal;
		paf::string *str;
	};
};

extern "C" {
extern int vshSblSsIsDevelopmentMode(void);
extern void SceLsdb_A6D8772C(void *ctx);
extern int SceLsdb_E90B81CD(void *ctx);
extern void SceLsdb_E2101B03(void *ctx);
extern int SceLsdb_D239031A(void *ctx, const paf::string *titleId,
	paf::common::SharedPtr<paf::map<SceUInt32, LsdbRowAttrVal> > *row);
extern int SceLsdb_6C74B301(void *ctx, paf::string *titleId, const char *path,
	int *outFlag, SceUInt64 *ts, paf::string *outDir, SceUID pid);
extern int SceLsdb_EFD528B1(void *ctx, paf::string *titleId, paf::common::SharedPtr<int> *parsed,
	const char *path, bool flagBit0, int *outFlag, paf::string *outDir, SceUID pid);
}

extern int shellAppMgrConvertPath(SceUID pid, int mode, const char *inPath,
	char *outPath, SceSize outSize);			/* 0x8144DA0C */

/* 0x811B918C */
int shellSvcResolveAppPath(SceUID pid, const char *inPath, paf::string *outResolved, SceUInt8 allowAny)
{
	paf::common::SharedPtr<paf::Buffer> buf = paf::MallocBuffer::Allocate(0x400);
	if (!buf.get())
		return (int)0x80101002;

	int r = shellAppMgrConvertPath(pid, 1, inPath, (char *)buf->GetAddr(), buf->GetSize());
	if (r != 0)
		return (int)0x80104003;

	const char *resolved = (const char *)buf->GetAddr();
	*outResolved = resolved ? resolved : "";

	if (allowAny != 0)
		return 0;

	const char *s = outResolved->c_str();
	if (sce_paf_strncmp("invalid:", s, 8) == 0)
		return (int)0x8010400B;

	if (sce_paf_strncmp("app0:", inPath, 5) != 0 &&
	    sce_paf_strncmp("savedata0:", inPath, 10) != 0) {
		if (vshSblSsIsDevelopmentMode() == 0)
			return (int)0x8010400B;
		if (sce_paf_strncmp("host0:", s, 6) != 0 &&
		    sce_paf_strncmp("ux0:/data", s, 10) != 0 &&
		    sce_paf_strncmp("ux0:/data/", s, 10) != 0) {
			return (int)0x8010400B;
		}
		return 0;
	}

	const char *colon = sce_paf_strchr(s, ':');
	if (!colon)
		return (int)0x8010400B;

	const char *rel = colon + 1;
	if (sce_paf_strncmp("/sce_", rel, 5) == 0 &&
	    (sce_paf_strncmp("app0:", inPath, 5) != 0 ||
	     (sce_paf_strncmp("/sce_sys/livearea/contents", rel, 27) != 0 &&
	      sce_paf_strncmp("/sce_sys/retail/livearea/contents", rel, 34) != 0))) {
		return (int)0x8010400B;
	}
	return 0;
}

/* 0x811B9392 */
int shellSvcGetAppMountPath(const paf::string *titleId, paf::string *outPath)
{
	char lsdbCtx[4];
	SceLsdb_A6D8772C(lsdbCtx);
	int r = SceLsdb_E90B81CD(lsdbCtx);
	if (r != 0) {
		SceLsdb_E2101B03(lsdbCtx);
		return r;
	}

	paf::common::SharedPtr<paf::map<SceUInt32, LsdbRowAttrVal> > row(
		new paf::map<SceUInt32, LsdbRowAttrVal>());
	r = SceLsdb_D239031A(lsdbCtx, titleId, &row);
	if (r != 0) {
		SceLsdb_E2101B03(lsdbCtx);
		return r;
	}

	paf::map<SceUInt32, LsdbRowAttrVal>::iterator it = row->find(0x9A9B3542u);
	if (it == row->end()) {
		SceLsdb_E2101B03(lsdbCtx);
		return (int)0x801040FF;
	}

	const char *val = "";
	if (it->second.type == 1 && it->second.str != NULL)
		val = it->second.str->c_str();
	*outPath = val ? val : "";
	SceLsdb_E2101B03(lsdbCtx);
	return 0;
}

/* 0x811B95D0 */
int shellSvcMountAppForLiveArea(const paf::string *titleId, paf::string *path, paf::string *outMount)
{
	paf::Plugin *gp = paf::Plugin::Find("game_plugin");
	if (!gp)
		return (int)0x801040FF;
	ShellPluginInterface *iface = static_cast<ShellPluginInterface *>(gp->GetInterface(1));
	if (!iface)
		return (int)0x801040FF;

	paf::string appPath;
	int r = shellSvcGetAppMountPath(titleId, &appPath);
	if (r != 0)
		return r;

	char mountBuf[20];
	typedef int (*MountFn)(const char *, char *, int);
	r = reinterpret_cast<MountFn>(iface->func[4])(appPath.c_str(), mountBuf, 17);
	if (r != 0)
		return r;

	*outMount = mountBuf;
	if (outMount->size() != 0) {
		const char *s = path->c_str();
		const char *colon = sce_paf_strchr(s, ':');
		paf::string prefix(outMount->c_str(), outMount->size() - 1);
		if (colon)
			*path = prefix + colon;
		else
			*path = prefix;
	}
	return 0;
}

/* 0x811B9710 */
int shellSvcLiveAreaUpdateFromPath(SceUID pid, const char *inPath, paf::string *titleId,
	SceUInt64 *ts, SceUInt8 allowAny, paf::string *outDir, int *outFlag)
{
	paf::string resolved;
	paf::string mountPoint;

	int r = shellSvcResolveAppPath(pid, inPath, &resolved, allowAny);
	if (r == 0) {
		if (allowAny != 0 ||
		    sce_paf_memcmp(inPath, "app0:", 5) != 0 ||
		    sce_paf_memcmp(resolved.c_str(), "host0:", 6) == 0 ||
		    sce_paf_memcmp(resolved.c_str(), "ux0:/data", 9) == 0 ||
		    (r = shellSvcMountAppForLiveArea(titleId, &resolved, &mountPoint)) == 0) {
			char ctx[4];
			r = SceLsdb_6C74B301(ctx, titleId, resolved.c_str(), outFlag, ts, outDir, pid);
		}
	}

	if (mountPoint.size() != 0) {
		paf::Plugin *gp = paf::Plugin::Find("game_plugin");
		if (gp) {
			ShellPluginInterface *iface = static_cast<ShellPluginInterface *>(gp->GetInterface(1));
			if (iface) {
				typedef void (*UnmountFn)(const char *);
				reinterpret_cast<UnmountFn>(iface->func[5])(mountPoint.c_str());
			}
		}
	}
	return r;
}

/* 0x811B9836 */
int shellSvcLiveAreaSetFromPath(SceUID pid, const char *inPath, paf::common::SharedPtr<int> *parsed,
	int flags, paf::string *titleId, SceUInt8 allowAny, paf::string *outDir, int *outFlag)
{
	paf::string resolved;
	paf::string mountPoint;

	int r = shellSvcResolveAppPath(pid, inPath, &resolved, allowAny);
	if (r == 0) {
		if (allowAny != 0 ||
		    sce_paf_memcmp(inPath, "app0:", 5) != 0 ||
		    sce_paf_memcmp(resolved.c_str(), "host0:", 6) == 0 ||
		    sce_paf_memcmp(resolved.c_str(), "ux0:/data", 9) == 0 ||
		    (r = shellSvcMountAppForLiveArea(titleId, &resolved, &mountPoint)) == 0) {
			char ctx[4];
			r = SceLsdb_EFD528B1(ctx, titleId, parsed, resolved.c_str(),
				(flags & 1) != 0, outFlag, outDir, pid);
		}
	}

	if (mountPoint.size() != 0) {
		paf::Plugin *gp = paf::Plugin::Find("game_plugin");
		if (gp) {
			ShellPluginInterface *iface = static_cast<ShellPluginInterface *>(gp->GetInterface(1));
			if (iface) {
				typedef void (*UnmountFn)(const char *);
				reinterpret_cast<UnmountFn>(iface->func[5])(mountPoint.c_str());
			}
		}
	}
	return r;
}
