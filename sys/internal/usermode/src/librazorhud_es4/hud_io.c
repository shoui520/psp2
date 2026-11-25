/*
 * Async file writer and CSV/TXT/XML GPU kick log writers for SceRazorHud.
 */
#include "razor_hud_internal.h"

typedef struct HudAsyncFileImpl {
	char filename[1024];
	int32_t fd;
	int32_t asyncId;
	uint32_t asyncOpt[6];
	int32_t *asyncResult;
	uint32_t asyncPad;
	uint8_t frontBuf[4096];
	uint32_t frontUsed;
	uint8_t backBuf[4096];
	uint32_t backUsed;
	uint8_t ttyFallback;
	uint8_t useFile;
	uint8_t tailPad[6];
} HudAsyncFileImpl;

typedef struct HudAsyncFile {
	HudAsyncFileImpl *impl;
	char fmtBuf[0x20000];
} HudAsyncFile;

typedef struct HudViewVtable {
	void *(*dtor)(void *self);
	void *(*deletingDtor)(void *self);
	void (*render)(void *self, void *ctx);
} HudViewVtable;

typedef struct HudLogView {
	const HudViewVtable *vtable;
	void *dataSource;
	uint8_t enabled;
	uint8_t pad[7];
	HudAsyncFile file;
	uint8_t needHeader;
	uint8_t logTailPad[3];
} HudLogView;

typedef struct HudGpuKickEntry {
	uint32_t type;
	float ms;
	uint32_t frame;
	uint32_t core;
} HudGpuKickEntry;

static const HudViewVtable g_viewBaseVtable = {
	hudViewBaseDtor,
	hudViewBaseDeletingDtor,
	NULL,
};

static const HudViewVtable g_logBaseVtable = {
	hudLogBaseDtor,
	hudLogBaseDeletingDtor,
	NULL,
};

static const HudViewVtable g_csvLogVtable = {
	hudCsvLogDtor,
	hudCsvLogDeletingDtor,
	(void (*)(void *, void *))hudCsvLogRender,
};

static const HudViewVtable g_txtLogVtable = {
	hudTxtLogDtor,
	hudTxtLogDeletingDtor,
	(void (*)(void *, void *))hudTxtLogRender,
};

static const HudViewVtable g_xmlLogVtable = {
	hudXmlLogDtor,
	hudXmlLogDeletingDtor,
	(void (*)(void *, void *))hudXmlLogRender,
};

static inline __attribute__((always_inline)) int hudAsyncWaitEvent(HudAsyncFileImpl *impl)
{
	uint32_t outBits;
	if (impl->asyncId == -1)
		return 0;
	if (sceKernelWaitEvent(impl->asyncId, 1U, &outBits, &impl->asyncResult, NULL) < 0)
		return -1;
	impl->asyncId = -1;
	return 0;
}

static inline __attribute__((always_inline)) void hudAsyncFlushFront(HudAsyncFileImpl *impl)
{
	if (impl->frontUsed == 0)
		return;
	if (hudAsyncWaitEvent(impl) < 0)
		return;
	sceClibMemcpy_safe(impl->backBuf, impl->frontBuf, impl->frontUsed);
	impl->backUsed = impl->frontUsed;
	impl->frontUsed = 0;
	impl->asyncId = sceIoWriteAsync(impl->fd, impl->backBuf, impl->backUsed, impl->asyncOpt);
}

void *hudAsyncFileInit(void *objPtr)
{
	HudAsyncFile *af = (HudAsyncFile *)objPtr;
	HudAsyncFileImpl *impl = (HudAsyncFileImpl *)hudNew(sizeof(HudAsyncFileImpl));

	af->impl = impl;
	sceClibMemset(impl, 0, sizeof(HudAsyncFileImpl));
	impl->fd = -1;
	impl->asyncId = -1;
	af->fmtBuf[0] = '\0';
	return af;
}

void *hudAsyncFileDestroy(void *objPtr)
{
	HudAsyncFile *af = (HudAsyncFile *)objPtr;
	HudAsyncFileImpl *impl = af->impl;

	hudAsyncFlushFront(impl);
	if (impl->fd >= 0) {
		if (hudAsyncWaitEvent(impl) == 0) {
			impl->asyncId = sceIoCloseAsync(impl->fd, impl->asyncOpt);
			if (hudAsyncWaitEvent(impl) == 0 && impl->asyncResult[1] >= 0)
				impl->fd = -1;
		}
	}
	hudDelete(impl);
	return af;
}

void hudAsyncFileSetUseFile(void *objPtr, uint8_t useFile)
{
	HudAsyncFile *af = (HudAsyncFile *)objPtr;
	af->impl->useFile = useFile;
}

char *hudAsyncFileSetFilename(void *objPtr, const char *name)
{
	HudAsyncFile *af = (HudAsyncFile *)objPtr;
	HudAsyncFileImpl *impl = af->impl;

	hudAsyncFlushFront(impl);
	if (impl->fd >= 0) {
		if (hudAsyncWaitEvent(impl) == 0) {
			impl->asyncId = sceIoCloseAsync(impl->fd, impl->asyncOpt);
			if (hudAsyncWaitEvent(impl) == 0 && impl->asyncResult[1] >= 0)
				impl->fd = -1;
		}
	}
	if (name != NULL) {
		sceClibStrncpy(impl->filename, "host0:", 1023U);
		sceClibStrncat(impl->filename, name, 1023U);
		impl->filename[1023] = '\0';
		return impl->filename;
	}
	impl->filename[0] = '\0';
	return impl->filename;
}

const char *hudAsyncFileGetFilename(void *objPtr)
{
	HudAsyncFile *af = (HudAsyncFile *)objPtr;
	return af->impl->filename;
}

int hudAsyncFileEnsureOpen(void *objPtr)
{
	HudAsyncFile *af = (HudAsyncFile *)objPtr;
	HudAsyncFileImpl *impl = af->impl;

	if (impl->useFile == 0 || impl->filename[0] == '\0') {
		if (impl->ttyFallback != 0)
			return 1;
		hudAsyncFlushFront(impl);
		if (impl->fd >= 0) {
			if (hudAsyncWaitEvent(impl) == 0) {
				impl->asyncId = sceIoCloseAsync(impl->fd, impl->asyncOpt);
				if (hudAsyncWaitEvent(impl) == 0 && impl->asyncResult[1] >= 0)
					impl->fd = -1;
			}
		}
		impl->ttyFallback = 1;
		return 1;
	}

	if (impl->fd < 0) {
		impl->asyncId = sceIoOpenAsync(impl->filename, 0x0302, 0x180, impl->asyncOpt);
		if (impl->asyncId < 0)
			return 0;
		if (hudAsyncWaitEvent(impl) < 0)
			return 0;
		impl->fd = impl->asyncResult[0];
		if (impl->asyncResult[1] < 0)
			return 0;
		impl->ttyFallback = 0;
	}
	return impl->fd != 0 ? 1 : 0;
}

int hudAsyncFileFlushAndClose(void *objPtr)
{
	HudAsyncFile *af = (HudAsyncFile *)objPtr;
	HudAsyncFileImpl *impl = af->impl;

	hudAsyncFlushFront(impl);
	if (impl->fd < 0)
		return 1;
	if (hudAsyncWaitEvent(impl) < 0)
		return 0;
	impl->asyncId = sceIoCloseAsync(impl->fd, impl->asyncOpt);
	if (hudAsyncWaitEvent(impl) < 0)
		return 0;
	if (impl->asyncResult[1] >= 0) {
		impl->fd = -1;
		return 1;
	}
	return 0;
}

int hudAsyncFileReopen(void *objPtr)
{
	HudAsyncFile *af = (HudAsyncFile *)objPtr;
	HudAsyncFileImpl *impl = af->impl;

	if (impl->useFile != 0 && impl->filename[0] != '\0') {
		hudAsyncFileFlushAndClose(af);
		impl->asyncId = sceIoOpenAsync(impl->filename, 0x0602, 0x180, impl->asyncOpt);
		if (impl->asyncId < 0)
			return 0;
		if (hudAsyncWaitEvent(impl) < 0)
			return 0;
		impl->fd = impl->asyncResult[0];
		if (impl->asyncResult[1] < 0)
			return 0;
		impl->ttyFallback = 0;
		return impl->fd >= 0 ? 1 : 0;
	}
	if (impl->ttyFallback == 0) {
		hudAsyncFileFlushAndClose(af);
		impl->ttyFallback = 1;
	}
	return 1;
}

int hudAsyncFilePrintf(void *objPtr, const char *fmt, ...)
{
	HudAsyncFile *af = (HudAsyncFile *)objPtr;
	HudAsyncFileImpl *impl = af->impl;
	va_list ap;
	uint32_t len;
	const char *p;

	va_start(ap, fmt);
	sceClibVsnprintf(af->fmtBuf, sizeof(af->fmtBuf), fmt, ap);
	va_end(ap);

	len = sceClibStrnlen(af->fmtBuf, sizeof(af->fmtBuf));
	if (len == 0)
		return 1;

	if (impl->ttyFallback != 0) {
		sceClibPrintf("%s", af->fmtBuf);
		return 1;
	}

	p = af->fmtBuf;
	while (len > 0) {
		uint32_t space = 4096U - impl->frontUsed;
		uint32_t chunk = (len < space) ? len : space;
		sceClibMemcpy_safe(impl->frontBuf + impl->frontUsed, p, chunk);
		p += chunk;
		len -= chunk;
		impl->frontUsed += chunk;
		if (impl->frontUsed == 4096U) {
			if (hudAsyncWaitEvent(impl) == 0) {
				sceClibMemcpy_safe(impl->backBuf, impl->frontBuf, impl->frontUsed);
				impl->backUsed = impl->frontUsed;
				impl->frontUsed = 0;
				impl->asyncId = sceIoWriteAsync(impl->fd, impl->backBuf, impl->backUsed, impl->asyncOpt);
			}
		}
	}
	return 1;
}

void *hudViewBaseDtor(void *objPtr)
{
	HudLogView *v = (HudLogView *)objPtr;
	v->vtable = &g_viewBaseVtable;
	return v;
}

void *hudViewBaseDeletingDtor(void *objPtr)
{
	HudLogView *v = (HudLogView *)objPtr;
	v->vtable = &g_viewBaseVtable;
	hudDelete(v);
	return v;
}

void *hudViewBaseCtor(void *objPtr)
{
	HudLogView *v = (HudLogView *)objPtr;
	v->vtable = &g_viewBaseVtable;
	v->dataSource = NULL;
	v->enabled = 1;
	return v;
}

void hudViewSetEnabled(void *objPtr, uint8_t enabled)
{
	HudLogView *v = (HudLogView *)objPtr;
	v->enabled = enabled;
}

void hudViewSetDataSource(void *objPtr, void *src)
{
	HudLogView *v = (HudLogView *)objPtr;
	v->dataSource = src;
}

void *hudLogBaseDtor(void *objPtr)
{
	HudLogView *v = (HudLogView *)objPtr;
	v->vtable = &g_logBaseVtable;
	hudAsyncFileDestroy(&v->file);
	hudViewBaseDtor(v);
	return v;
}

void *hudLogBaseDeletingDtor(void *objPtr)
{
	HudLogView *v = (HudLogView *)objPtr;
	v->vtable = &g_logBaseVtable;
	hudAsyncFileDestroy(&v->file);
	hudViewBaseDtor(v);
	hudDelete(v);
	return v;
}

void *hudLogBaseCtor(void *objPtr)
{
	HudLogView *v = (HudLogView *)objPtr;
	hudViewBaseCtor(v);
	v->vtable = &g_logBaseVtable;
	hudAsyncFileInit(&v->file);
	v->needHeader = 1;
	return v;
}

void hudLogSetUseFile(void *objPtr, uint8_t useFile)
{
	HudLogView *v = (HudLogView *)objPtr;
	hudAsyncFileSetUseFile(&v->file, useFile);
}

char *hudLogSetFilename(void *objPtr, const char *name)
{
	HudLogView *v = (HudLogView *)objPtr;
	return hudAsyncFileSetFilename(&v->file, name);
}

void *hudCsvLogDtor(void *objPtr)
{
	HudLogView *v = (HudLogView *)objPtr;
	v->vtable = &g_csvLogVtable;
	hudLogBaseDtor(v);
	return v;
}

void *hudCsvLogDeletingDtor(void *objPtr)
{
	HudLogView *v = (HudLogView *)objPtr;
	v->vtable = &g_csvLogVtable;
	hudLogBaseDtor(v);
	hudDelete(v);
	return v;
}

void hudCsvLogRender(void *objPtr)
{
	HudLogView *v = (HudLogView *)objPtr;
	uint8_t *src = (uint8_t *)v->dataSource;
	uint32_t count, i;

	if (src == NULL || src[4] == 0)
		return;
	hudGpuTimelineSrcSetLogEnabled(src, 1);
	if (v->needHeader != 0) {
		if (hudAsyncFileReopen(&v->file) == 0)
			return;
		hudAsyncFilePrintf(&v->file, "Frame,Type,Core,Ms\n");
		v->needHeader = 0;
	} else {
		if (hudAsyncFileEnsureOpen(&v->file) == 0)
			return;
	}

	count = hudGpuTimelineSrcGetKickCount(src);
	for (i = 0; i < count; i++) {
		HudGpuKickEntry *k = (HudGpuKickEntry *)hudGpuTimelineSrcGetKick(src, i);
		const char *typeStr = (k->type == 0) ? "Vertex" : "Fragment";
		char msBuf[32];
		sceClibMemset(msBuf, 0, 4U);
		sceClibSnprintf(msBuf, sizeof(msBuf), "%.2f", (double)k->ms);
		hudAsyncFilePrintf(&v->file, "%d,%s,%d,%s\n", k->frame, typeStr, k->core, msBuf);
	}
}

void *hudCsvLogCtor(void *objPtr)
{
	HudLogView *v = (HudLogView *)objPtr;
	hudLogBaseCtor(v);
	v->vtable = &g_csvLogVtable;
	return v;
}

void *hudTxtLogDtor(void *objPtr)
{
	HudLogView *v = (HudLogView *)objPtr;
	v->vtable = &g_txtLogVtable;
	hudLogBaseDtor(v);
	return v;
}

void *hudTxtLogDeletingDtor(void *objPtr)
{
	HudLogView *v = (HudLogView *)objPtr;
	v->vtable = &g_txtLogVtable;
	hudLogBaseDtor(v);
	hudDelete(v);
	return v;
}

void hudTxtLogRender(void *objPtr)
{
	HudLogView *v = (HudLogView *)objPtr;
	uint8_t *src = (uint8_t *)v->dataSource;
	uint32_t count, i;

	if (src == NULL || src[4] == 0)
		return;
	hudGpuTimelineSrcSetLogEnabled(src, 1);
	if (v->needHeader != 0) {
		if (hudAsyncFileReopen(&v->file) == 0)
			return;
		hudAsyncFilePrintf(&v->file, "\n|  Frame |   Type   | Core |       Ms      |\n");
		v->needHeader = 0;
	} else {
		if (hudAsyncFileEnsureOpen(&v->file) == 0)
			return;
	}

	count = hudGpuTimelineSrcGetKickCount(src);
	for (i = 0; i < count; i++) {
		HudGpuKickEntry *k = (HudGpuKickEntry *)hudGpuTimelineSrcGetKick(src, i);
		const char *typeStr = (k->type == 0) ? "Vertex  " : "Fragment";
		char msBuf[32];
		const char *frameStr;
		sceClibMemset(msBuf, 0, 4U);
		sceClibSnprintf(msBuf, sizeof(msBuf), "%.2f", (double)k->ms);
		frameStr = hudFormatInt64Commas((int64_t)k->frame);
		hudAsyncFilePrintf(&v->file, "|%7s | %s |%6d|%14s |\n", frameStr, typeStr, k->core, msBuf);
	}
}

void *hudTxtLogCtor(void *objPtr)
{
	HudLogView *v = (HudLogView *)objPtr;
	hudLogBaseCtor(v);
	v->vtable = &g_txtLogVtable;
	return v;
}

void *hudXmlLogDtor(void *objPtr)
{
	HudLogView *v = (HudLogView *)objPtr;
	v->vtable = &g_xmlLogVtable;
	if (v->needHeader == 0) {
		hudAsyncFilePrintf(&v->file, "\t</Log>\n");
		hudAsyncFilePrintf(&v->file, "</Capture>\n");
	}
	hudLogBaseDtor(v);
	return v;
}

void *hudXmlLogDeletingDtor(void *objPtr)
{
	HudLogView *v = (HudLogView *)objPtr;
	v->vtable = &g_xmlLogVtable;
	if (v->needHeader == 0) {
		hudAsyncFilePrintf(&v->file, "\t</Log>\n");
		hudAsyncFilePrintf(&v->file, "</Capture>\n");
	}
	hudLogBaseDtor(v);
	hudDelete(v);
	return v;
}

void hudXmlLogRender(void *objPtr)
{
	HudLogView *v = (HudLogView *)objPtr;
	uint8_t *src = (uint8_t *)v->dataSource;
	uint32_t count, i;

	if (src == NULL || src[4] == 0)
		return;
	hudGpuTimelineSrcSetLogEnabled(src, 1);
	if (v->needHeader != 0) {
		const char *fname;
		if (hudAsyncFileReopen(&v->file) == 0)
			return;
		hudAsyncFilePrintf(&v->file, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
		hudAsyncFilePrintf(&v->file, "<?xml-stylesheet type=\"text/xsl\" href=\"perflog.xsl\"?>\n");
		hudAsyncFilePrintf(&v->file, "<Capture>\n");
		fname = hudAsyncFileGetFilename(&v->file);
		if (fname != NULL && fname[0] != '\0')
			hudAsyncFilePrintf(&v->file, "\t<Filename>%s</Filename>\n", fname);
		hudAsyncFilePrintf(&v->file, "\t<Log>\n");
		v->needHeader = 0;
	} else {
		if (hudAsyncFileEnsureOpen(&v->file) == 0)
			return;
	}

	count = hudGpuTimelineSrcGetKickCount(src);
	for (i = 0; i < count; i++) {
		HudGpuKickEntry *k = (HudGpuKickEntry *)hudGpuTimelineSrcGetKick(src, i);
		const char *typeStr = (k->type != 0) ? "Fragment" : "Vertex  ";
		hudAsyncFilePrintf(&v->file, "\t\t\t<Kick>\n");
		hudAsyncFilePrintf(&v->file, "\t\t\t\t<Frame>%u</Frame>\n", k->frame);
		hudAsyncFilePrintf(&v->file, "\t\t\t\t<Type>%s</Type>\n", typeStr);
		hudAsyncFilePrintf(&v->file, "\t\t\t\t<ms>%.2f</ms>\n", (double)k->ms);
		hudAsyncFilePrintf(&v->file, "\t\t\t\t<core>%d</core>\n", k->core);
		hudAsyncFilePrintf(&v->file, "\t\t\t</Kick>\n");
	}
}

void *hudXmlLogCtor(void *objPtr)
{
	HudLogView *v = (HudLogView *)objPtr;
	hudLogBaseCtor(v);
	v->vtable = &g_xmlLogVtable;
	return v;
}
