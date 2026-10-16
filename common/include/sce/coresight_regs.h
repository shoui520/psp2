/*
 * The Cortex-A9 CoreSight blocks' physical pages: per core a debug and a PMU
 * page (0x2000 apart), a CTI and a PTM, plus the shared TPIU, funnel and ITM.
 * SceProfilerAgentMgr maps all of them; the debugger's file manager maps the
 * debug pages (SceDbg<n>RegForSdfmgr). Register layouts follow ARM's CoreSight
 * documentation and are kept by the module that drives them.
 */
#pragma once

#define CORESIGHT_DBG_BASE(core)	(0xe3310000 + (core) * 0x2000)
#define CORESIGHT_PMU_BASE(core)	(0xe3311000 + (core) * 0x2000)
#define CORESIGHT_CTI_BASE(core)	(0xe3318000 + (core) * 0x1000)
#define CORESIGHT_PTM_BASE(core)	(0xe331c000 + (core) * 0x1000)
#define CORESIGHT_TPIU_BASE		0xe3203000
#define CORESIGHT_FUNNEL_BASE		0xe3204000
#define CORESIGHT_ITM_BASE		0xe3205000
#define CORESIGHT_PAGE_SIZE		0x1000
