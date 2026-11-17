/*
 * SceNpCommonPs4: HttpTemplate (npext/http/http_template.cpp), an SceHttp template that is deleted
 * with the object.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

extern const void *g_httpTemplateVtable[];	/* 0x8102ab58 */

struct NpCommonPs4HttpTemplate *npCommonPs4HttpTemplateConstruct(struct NpCommonPs4HttpTemplate *tmpl)
{
	tmpl->vtable = NP_COMMON_PS4_VTABLE(g_httpTemplateVtable);
	tmpl->templateId = 0;
	return tmpl;
}

struct NpCommonPs4HttpTemplate *npCommonPs4HttpTemplateDestruct(struct NpCommonPs4HttpTemplate *tmpl)
{
	SceInt32 templateId;

	templateId = tmpl->templateId;
	if (templateId != 0) {
		tmpl->vtable = NP_COMMON_PS4_VTABLE(g_httpTemplateVtable);
		sceHttpDeleteTemplate(templateId);
	}
	tmpl->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return tmpl;
}

int npCommonPs4HttpTemplateDelete(struct NpCommonPs4HttpTemplate *tmpl)
{
	SceInt32 templateId;

	templateId = tmpl->templateId;
	if (templateId != 0) {
		tmpl->vtable = NP_COMMON_PS4_VTABLE(g_httpTemplateVtable);
		sceHttpDeleteTemplate(templateId);
	}
	tmpl->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(tmpl);
}

int npCommonPs4HttpTemplateInit(struct NpCommonPs4HttpTemplate *tmpl, int unused, const char *userAgent,
	SceInt32 httpVer, SceUInt8 autoProxyConf)
{
	SceInt32 templateId;
	int result;

	templateId = tmpl->templateId;
	if (templateId != 0) {
		NP_COMMON_PS4_ASSERT(0, "Init", "npext\\http\\http_template.cpp", 34, "!IsInit()");
		templateId = tmpl->templateId;
	}
	if (templateId != 0)
		return NP_COMMON_PS4_ERROR_80559E03;
	result = sceHttpCreateTemplate(userAgent, httpVer, autoProxyConf);
	if (result >= 0) {
		tmpl->templateId = result;
		return 0;
	}
	return result;
}

const void *g_httpTemplateVtable[] = {		/* 0x8102ab58 */
	NULL,
	NULL,
	(const void *)npCommonPs4HttpTemplateDestruct,
	(const void *)npCommonPs4HttpTemplateDelete,
};

}
