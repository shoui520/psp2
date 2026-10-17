#ifndef MSIF_RMAUTH_H
#define MSIF_RMAUTH_H

#include <sm_comm/sm_comm.h>

extern const SceSelfAuthInfo gMsifSmAuthInfo;
extern SceSblSmCommId gMsifSmId;

int invoke_rm_auth_secure_module(void);
int call_get_key_master_gen_no(uint32_t *generation);
int call_set_ind_key(const uint8_t *mediaId);

#endif
