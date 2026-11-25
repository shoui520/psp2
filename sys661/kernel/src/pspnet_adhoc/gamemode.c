#include "adhoc_int.h"

static s32 g_gm_master_id = -1;
static struct {
	GameModeNode *head;
	GameModeNode **tail;
} g_gm_list;

static s32 gameModeCreateBuffer(const u8 *mac, void *buf, u32 size);
static s32 gameModeDeleteBuffer(s32 id);
static s32 gameModeUpdateBuffer(s32 id, void *buf, s32 mode);

/* === sceNetAdhocGameModeCreateMaster @ 0x6c1c === */
s32 sceNetAdhocGameModeCreateMaster(void *buf, u32 size)
{
	u8 local_mac[16];
	s32 intr;
	s32 ret;

	if (_sceKernelCheckThreadStack() < 0x7d0)
		return 0x80410005;
	if (buf == NULL)
		return 0x80410711;
	if (size - 1 >= 0x400)
		return 0x80410705;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
	} else if (sceWlanDevIsGameMode() == 0) {
		ret = 0x8041071b;
	} else {
		ret = sceNetGetLocalEtherAddr(local_mac);
		if (ret >= 0) {
			ret = gameModeCreateBuffer(local_mac, buf, size);
			if (ret >= 0) {
				g_gm_master_id = ret;
				ret = 0;
			}
		}
	}

	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === sceNetAdhocGameModeCreateReplica @ 0x6d0c === */
s32 sceNetAdhocGameModeCreateReplica(const u8 *mac, void *buf, u32 size)
{
	s32 intr;
	s32 ret;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (mac == NULL || buf == NULL)
		return 0x80410711;
	if (size - 1 >= 0x400)
		return 0x80410705;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
	} else if (sceWlanDevIsGameMode() == 0) {
		ret = 0x8041071b;
	} else {
		ret = gameModeCreateBuffer(mac, buf, size);
	}

	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === sceNetAdhocGameModeUpdateMaster @ 0x6df4 === */
s32 sceNetAdhocGameModeUpdateMaster(void)
{
	s32 intr;
	s32 ret;
	GameModeNode *gm;

	if (_sceKernelCheckThreadStack() < 0x7f0)
		return 0x80410005;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
	} else if (sceWlanDevIsGameMode() == 0) {
		ret = 0x8041071b;
	} else if (g_gm_master_id < 0) {
		ret = 0x8041071c;
	} else {
		for (gm = g_gm_list.head; gm != NULL; gm = gm->next) {
			if (gm->id == g_gm_master_id)
				break;
		}
		if (gm == NULL) {
			ret = 0x8041071c;
		} else {
			sceKernelMemcpy(gm->work_buf, gm->user_buf, gm->user_size);
			ret = sceNetAdhocAuth_lib_AAB06250(gm->work_buf, gm->work_size);
			if (ret >= 0)
				ret = sceWlanGPSend(0x10, gm->work_buf, gm->work_size);
		}
	}

	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === sceNetAdhocGameModeDeleteMaster @ 0x6f08 === */
s32 sceNetAdhocGameModeDeleteMaster(void)
{
	s32 intr;
	s32 ret;
	GameModeNode *gm;

	if (_sceKernelCheckThreadStack() < 0x7f0)
		return 0x80410005;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
	} else if (g_gm_master_id < 0) {
		ret = 0x8041071c;
	} else {
		for (gm = g_gm_list.head; gm != NULL; gm = gm->next) {
			if (gm->id == g_gm_master_id)
				break;
		}
		if (gm == NULL) {
			ret = 0x8041071c;
		} else {
			if (gm->next != NULL)
				gm->next->prev = gm->prev;
			else
				g_gm_list.tail = gm->prev;
			*gm->prev = gm->next;
			sceNetIfhandle_lib_76BAD213(gm->work_buf);
			sceNetIfhandle_lib_76BAD213(gm);
			ret = 0;
		}
	}

	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === gameModeInit @ 0x7008 === */
s32 gameModeInit(void)
{
	g_gm_list.head = NULL;
	g_gm_list.tail = &g_gm_list.head;
	return 0;
}

/* === gameModeTerm @ 0x7020 === */
void gameModeTerm(void)
{
	GameModeNode *gm;

	while ((gm = g_gm_list.head) != NULL) {
		if (gm->next != NULL)
			gm->next->prev = gm->prev;
		else
			g_gm_list.tail = gm->prev;
		*gm->prev = gm->next;
		sceNetIfhandle_lib_76BAD213(gm);
	}
}

/* === sceNetAdhocGameModeUpdateReplica @ 0x7084 === */
s32 sceNetAdhocGameModeUpdateReplica(s32 id, void *opt_data)
{
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return 0x80410005;
	return gameModeUpdateBuffer(id, opt_data, 0);
}

/* === sceNetAdhoc_lib_AF6DFB15 @ 0x70e0 === */
s32 sceNetAdhoc_lib_AF6DFB15(s32 id, void *opt_data)
{
	return gameModeUpdateBuffer(id, opt_data, 1);
}

/* === sceNetAdhocGameModeDeleteReplica @ 0x70fc === */
s32 sceNetAdhocGameModeDeleteReplica(s32 id)
{
	s32 intr;
	s32 ret;

	if (_sceKernelCheckThreadStack() < 0x7f0)
		return 0x80410005;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
	} else {
		ret = gameModeDeleteBuffer(id);
	}

	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === gameModeCreateBuffer @ 0x7188 === */
static s32 gameModeCreateBuffer(const u8 *mac, void *buf, u32 size)
{
	s32 id;
	GameModeNode *gm;
	u32 work_size;

	id = sceWlanGPPrevEstablishActive();
	if (id < 0)
		return id;

	for (gm = g_gm_list.head; gm != NULL; gm = gm->next) {
		if (gm->id == id)
			return 0x8041071a;
	}

	gm = (GameModeNode *)sceNetIfhandle_lib_15CFE3C0(0x24);
	if (gm == NULL)
		return 0x80410001;

	gm->id = id;
	sceKernelMemcpy(gm->mac, mac, 6);
	work_size = (size + 0x0f) & ~0x0fU;
	gm->user_buf = buf;
	gm->user_size = size;
	gm->work_size = work_size;
	gm->work_buf = sceNetIfhandle_lib_15CFE3C0(work_size);
	if (gm->work_buf == NULL) {
		sceNetIfhandle_lib_76BAD213(gm);
		return 0x80410001;
	}

	gm->next = NULL;
	gm->prev = g_gm_list.tail;
	*g_gm_list.tail = gm;
	g_gm_list.tail = &gm->next;
	return id;
}

/* === gameModeDeleteBuffer @ 0x72a4 === */
static s32 gameModeDeleteBuffer(s32 id)
{
	GameModeNode *gm;

	for (gm = g_gm_list.head; gm != NULL; gm = gm->next) {
		if (gm->id == id)
			break;
	}
	if (gm == NULL)
		return 0x8041071c;

	if (gm->next != NULL)
		gm->next->prev = gm->prev;
	else
		g_gm_list.tail = gm->prev;
	*gm->prev = gm->next;
	sceNetIfhandle_lib_76BAD213(gm->work_buf);
	sceNetIfhandle_lib_76BAD213(gm);
	return 0;
}

/* === gameModeUpdateBuffer @ 0x7338 === */
static s32 gameModeUpdateBuffer(s32 id, void *opt_data, s32 mode)
{
	u32 recv_info[6];
	s32 intr;
	s32 ret;
	GameModeNode *gm;

	if (id < 0)
		return 0x80410708;

	if (mode == 0 && opt_data != NULL) {
		if (*(u32 *)opt_data != 0x10)
			return 0x80410705;
	}

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
	} else if (sceWlanDevIsGameMode() == 0) {
		ret = 0x8041071b;
	} else {
		for (gm = g_gm_list.head; gm != NULL; gm = gm->next) {
			if (gm->id == id)
				break;
		}
		if (gm == NULL) {
			ret = 0x8041071c;
		} else {
			ret = sceWlanGPRecv(id, gm->work_buf, gm->work_size, recv_info);
			if (ret >= 0) {
				if (recv_info[0] != 0) {
					ret = sceNetAdhocAuth_lib_015A8A64(gm->work_buf, gm->work_size);
					if (ret >= 0) {
						sceKernelMemcpy(gm->user_buf, gm->work_buf, gm->user_size);
						ret = 0;
					}
				}
				if (ret >= 0 && opt_data != NULL) {
					if (mode == 1) {
						*(u32 *)((u8 *)opt_data + 0x00) = recv_info[0];
						*(u32 *)((u8 *)opt_data + 0x08) = recv_info[2];
						*(u32 *)((u8 *)opt_data + 0x0c) = recv_info[3];
						*(u32 *)((u8 *)opt_data + 0x10) = recv_info[4];
					} else if (mode == 0) {
						*(u32 *)((u8 *)opt_data + 0x04) = recv_info[0];
						*(u32 *)((u8 *)opt_data + 0x08) = recv_info[2];
						*(u32 *)((u8 *)opt_data + 0x0c) = recv_info[3];
					}
				}
			}
		}
	}

	sceNet_lib_515B2F33(intr);
	return ret;
}
