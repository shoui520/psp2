#include "krm_internal.h"

int KrmDriverCryptUserData(const KrmCryptRequest *request, int encrypt)
{
	KrmCryptRequest local __attribute__((aligned(8)));
	SceOpenPsId keyId;
	SceOpenPsId iv;
	void *localIv;
	int ret;

	if (!request || !KrmDriverIsAccessible(request, sizeof(*request), 1) ||
	    KrmDriverCopyFromUser(&local, request, sizeof(local)) < 0 ||
	    local.srcSize > g_krmSignedState.state.config.maxDataSize ||
	    local.dstSize > g_krmSignedState.state.config.maxDataSize ||
	    !KrmDriverIsAccessible(local.src, local.srcSize, 1) ||
	    !KrmDriverIsAccessible(local.dst, local.dstSize, 2))
		return (int)0x80a00000;
	if (local.keyId) {
		if (!KrmDriverIsAccessible(local.keyId, 16, 1))
			return (int)0x80a00000;
		ret = KrmDriverCopyFromUser(&keyId, local.keyId, 16);
		if (ret < 0)
			goto clearKeyId;
		local.keyId = &keyId;
	}
	localIv = local.iv;
	if (localIv) {
		ret = (int)0x80a00000;
		if (!KrmDriverIsAccessible(localIv, 16, 3))
			goto clearKeyId;
		ret = KrmDriverCopyFromUser(&iv, localIv, 16);
		if (ret < 0)
			goto clearIv;
		localIv = &iv;
	}
	ret = KrmDriverCopyFromUser(g_krmCryptBuffer, local.src, local.srcSize);
	if (ret >= 0) {
		ret = KrmCryptData(&g_krmSignedState.state, local.keyId,
				  g_krmCryptBuffer, local.srcSize, g_krmCryptBuffer,
				  local.dstSize, localIv, encrypt);
		if (ret >= 0) {
			ret = KrmDriverCopyToUser(local.dst, g_krmCryptBuffer, local.dstSize);
			if (ret == 0 && local.iv)
				ret = KrmDriverCopyToUser(local.iv, &iv, 16);
		}
	}
	memset(g_krmCryptBuffer, 0, sizeof(g_krmCryptBuffer));
clearIv:
	__builtin_memset(&iv, 0, sizeof(iv));
clearKeyId:
	__builtin_memset(&keyId, 0, sizeof(keyId));
	return ret;
}

int KrmDriverLockProcess(void)
{
	SceUID pid;
	SceUID currentPid;
	int ret;

	if (!g_krmKeysInitialized)
		return (int)0x80a0000a;
	ret = KrmDriverLock();
	if (ret < 0)
		return ret;
	if (!KrmDriverGetState()) {
		KrmDriverUnlock();
		return (int)0x80a0000a;
	}
	pid = KrmDriverGetPID();
	currentPid = ksceKernelGetProcessId();
	if (pid == -1) {
		if (KrmDriverGetAuthorization(currentPid)) {
			memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
			__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
			__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
			sceKrmDriverSetPID(-1);
			sceKrmDriverSetPID(currentPid);
			return 0;
		}
	} else if (pid == currentPid) {
		return 0;
	}
	KrmDriverUnlock();
	return (int)0x80a00006;
}

int SceKrm_ECE6B5F4(const KrmSignedStateConfig *config)
{
	KrmSignedStateConfig local;
	SceUID pid;
	SceUID currentPid;
	unsigned int syscall = sceSyscallEnter();
	int ret;

	if (!g_krmKeysInitialized) {
		ret = (int)0x80a0000a;
		goto leave;
	}
	ret = KrmDriverLock();
	if (ret < 0)
		goto leave;
	if (KrmDriverGetState()) {
		ret = (int)0x80a0000b;
		KrmDriverUnlock();
		goto leave;
	}
	pid = KrmDriverGetPID();
	currentPid = ksceKernelGetProcessId();
	if (pid == -1) {
		if (!KrmDriverGetAuthorization(currentPid))
			goto denied;
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
		sceKrmDriverSetPID(currentPid);
	} else if (pid != currentPid) {
		goto denied;
	}
	if (config && KrmDriverIsAccessible(config, sizeof(*config), 1)) {
		ret = KrmDriverCopyFromUser(&local, config, sizeof(local));
		if (ret >= 0) {
			if (KrmDriverVerifySignature(&local.signature, &local.config,
						     sizeof(local.config), &g_krmConfigPublicKey) < 0) {
				ret = (int)0x80a00005;
				memset(&local, 0, sizeof(local));
			} else {
				ret = KrmInitializeState(&g_krmSignedState.state, &local.config);
				memset(&local, 0, sizeof(local));
				if (ret == 0) {
					ret = KrmDriverDeriveStateKeys();
					if (ret == 0)
						KrmDriverSetState();
				}
			}
		}
	} else {
		ret = (int)0x80a00000;
	}
	if (!KrmDriverGetState()) {
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
	}
	KrmDriverUnlock();
leave:
	sceSyscallLeave(syscall);
	return ret;
denied:
	ret = (int)0x80a00006;
	KrmDriverUnlock();
	goto leave;
}

int SceKrm_820B45C6(void)
{
	SceUID pid;
	SceUID currentPid;
	unsigned int syscall = sceSyscallEnter();
	int ret;

	if (!g_krmKeysInitialized) {
		ret = (int)0x80a0000a;
		goto leave;
	}
	ret = KrmDriverLock();
	if (ret < 0)
		goto leave;
	if (!KrmDriverGetState()) {
		KrmDriverUnlock();
		sceSyscallLeave(syscall);
		return (int)0x80a0000a;
	}
	pid = KrmDriverGetPID();
	currentPid = ksceKernelGetProcessId();
	if (pid == -1) {
		if (!KrmDriverGetAuthorization(currentPid))
			goto denied;
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
		sceKrmDriverSetPID(currentPid);
	} else if (pid != currentPid) {
		goto denied;
	}
	KrmDriverClearState();
	if (!KrmDriverGetState()) {
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
	}
	ret = 0;
	KrmDriverUnlock();
leave:
	sceSyscallLeave(syscall);
	return ret;
denied:
	KrmDriverUnlock();
	ret = (int)0x80a00006;
	goto leave;
}

int SceKrm_E0AEAE3B(const KrmSignedState *state)
{
	SceOpenPsId iv;
	uint64_t tick;
	SceUID pid;
	SceUID currentPid;
	unsigned int syscall = sceSyscallEnter();
	int ret;

	if (!g_krmKeysInitialized) {
		ret = (int)0x80a0000a;
		goto leave;
	}
	ret = KrmDriverLock();
	if (ret < 0)
		goto leave;
	if (KrmDriverGetState()) {
		ret = (int)0x80a0000b;
		KrmDriverUnlock();
		goto leave;
	}
	pid = KrmDriverGetPID();
	currentPid = ksceKernelGetProcessId();
	if (pid == -1) {
		if (!KrmDriverGetAuthorization(currentPid))
			goto denied;
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
		sceKrmDriverSetPID(currentPid);
	} else if (pid != currentPid) {
		goto denied;
	}
	ret = KrmDriverDeriveStateKeys();
	if (ret == 0) {
		if (KrmDriverIsAccessible(state, sizeof(*state), 1)) {
			ret = KrmDriverCopyFromUser(&g_krmEncryptedState, state, sizeof(*state));
			if (ret == 0) {
				ret = KrmDriverVerifySignature(&g_krmEncryptedState.signature,
					&g_krmEncryptedState.state, sizeof(KrmState), &g_krmStatePublicKey);
				if (ret >= 0) {
					__builtin_memcpy(&iv, g_krmStateIv, sizeof(iv));
					ret = KrmAesCbcDecrypt((const void *)&g_krmEncryptedState.state,
						(void *)&g_krmSignedState.state, sizeof(KrmState), 256,
						g_krmStateKey.bytes, iv.open_psid);
					__builtin_memset(&iv, 0, sizeof(iv));
					if (ret == 0) {
						ret = KrmDriverGetNetworkTick(&tick);
						if (ret == 0) {
							if (tick >= g_krmSignedState.state.tick) {
								memset(&g_krmEncryptedState, 0, sizeof(g_krmEncryptedState));
								KrmDriverSetState();
								goto checkState;
							}
							ret = (int)0x80a00005;
						}
					}
					memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
				}
			}
			memset(&g_krmEncryptedState, 0, sizeof(g_krmEncryptedState));
		} else {
			ret = (int)0x80a00000;
		}
	}
checkState:
	if (!KrmDriverGetState()) {
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
	}
	KrmDriverUnlock();
leave:
	sceSyscallLeave(syscall);
	return ret;
denied:
	ret = (int)0x80a00006;
	KrmDriverUnlock();
	goto leave;
}

int EncryptAndSignState(KrmSignedState *state)
{
	SceOpenPsId iv;
	unsigned int syscall = sceSyscallEnter();
	int ret;
	int error;

	ret = KrmDriverLockProcess();
	if (ret < 0)
		goto leave;
	if (__builtin_expect(!KrmDriverIsAccessible(state, sizeof(*state), 2), 0)) {
		ret = (int)0x80a00000;
		goto checkState;
	}
	memcpy(&g_krmEncryptedState, &g_krmSignedState, sizeof(g_krmEncryptedState));
	error = KrmDriverGetRandomBytes(g_krmEncryptedState.state.nonce, 8);
	if (error < 0) {
		ret = (int)0x80a00008;
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0, &g_krmRandomDebug,
			g_krmRandomError,
			g_krmEncryptAndSignStateName, error);
#endif
		goto error;
	}
	ret = KrmDriverGetNetworkTick(&g_krmEncryptedState.state.tick);
	if (ret < 0) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0, &g_krmTickDebug,
			g_krmTickError,
			g_krmEncryptAndSignStateName, ret);
#endif
		goto error;
	}
	__builtin_memcpy(&iv, g_krmStateIv, sizeof(iv));
	ret = KrmAesCbcEncrypt((const void *)&g_krmEncryptedState.state,
		(void *)&g_krmEncryptedState.state, sizeof(KrmState), 256,
		g_krmStateKey.bytes, iv.open_psid);
	__builtin_memset(&iv, 0, sizeof(iv));
	if (__builtin_expect(ret == 0, 0)) {
		ret = KrmDriverSignMessage(&g_krmEncryptedState.signature,
			&g_krmEncryptedState.state, sizeof(KrmState), &g_krmStatePrivateKey);
		if (ret == 0) {
			ret = KrmDriverCopyToUser(state, &g_krmEncryptedState, sizeof(*state));
			goto clear;
		}
	}
error:
	memset(&g_krmEncryptedState, 0, sizeof(g_krmEncryptedState));
clear:
	memset(&g_krmEncryptedState, 0, sizeof(g_krmEncryptedState));
checkState:
	if (!KrmDriverGetState()) {
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
	}
	KrmDriverUnlock();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int SceKrm_10202EB4(const void *keyId)
{
	SceOpenPsId localId;
	SceUID pid;
	SceUID currentPid;
	unsigned int syscall = sceSyscallEnter();
	int ret;

	if (!g_krmKeysInitialized) {
		ret = (int)0x80a0000a;
		goto leave;
	}
	ret = KrmDriverLock();
	if (ret < 0)
		goto leave;
	if (!KrmDriverGetState()) {
		ret = (int)0x80a0000a;
		KrmDriverUnlock();
		goto leave;
	}
	pid = KrmDriverGetPID();
	currentPid = ksceKernelGetProcessId();
	if (__builtin_expect(pid == -1, 0)) {
		if (!KrmDriverGetAuthorization(currentPid))
			goto denied;
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		g_krmStateKey.words[0] = 0;
		g_krmStateKey.words[1] = 0;
		g_krmStateKey.words[2] = 0;
		g_krmStateKey.words[3] = 0;
		g_krmStateKey.words[4] = 0;
		g_krmStateKey.words[5] = 0;
		g_krmStateKey.words[6] = 0;
		g_krmStateKey.words[7] = 0;
		g_krmStateIv[0] = 0;
		g_krmStateIv[1] = 0;
		g_krmStateIv[2] = 0;
		g_krmStateIv[3] = 0;
		sceKrmDriverSetPID(-1);
		sceKrmDriverSetPID(currentPid);
	} else if (pid != currentPid) {
		goto denied;
	}
	ret = (int)0x80a00000;
	if (__builtin_expect(keyId &&
			     KrmDriverIsAccessible(keyId, sizeof(localId), 1), 1)) {
		ret = KrmDriverCopyFromUser(&localId, keyId, sizeof(localId));
		if (__builtin_expect(ret == 0, 0))
			ret = KrmSetDefaultKey(&g_krmSignedState.state, &localId);
		__builtin_memset(&localId, 0, sizeof(localId));
	}
	if (__builtin_expect(!KrmDriverGetState(), 1)) {
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
	}
	KrmDriverUnlock();
leave:
	sceSyscallLeave(syscall);
	return ret;
denied:
	ret = (int)0x80a00006;
	KrmDriverUnlock();
	goto leave;
}

int SceKrm_98EEB7F3(void *keyId)
{
	SceOpenPsId localId;
	SceUID pid;
	SceUID currentPid;
	unsigned int syscall = sceSyscallEnter();
	int ret;

	if (!g_krmKeysInitialized) {
		ret = (int)0x80a0000a;
		goto leave;
	}
	ret = KrmDriverLock();
	if (ret < 0)
		goto leave;
	if (!KrmDriverGetState()) {
		ret = (int)0x80a0000a;
		KrmDriverUnlock();
		goto leave;
	}
	pid = KrmDriverGetPID();
	currentPid = ksceKernelGetProcessId();
	if (pid == -1) {
		if (!KrmDriverGetAuthorization(currentPid))
			goto denied;
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
		sceKrmDriverSetPID(currentPid);
	} else if (pid != currentPid) {
		goto denied;
	}
	ret = (int)0x80a00000;
	if (__builtin_expect(keyId &&
			     KrmDriverIsAccessible(keyId, sizeof(localId), 2), 1)) {
		ret = KrmGetDefaultKey(&g_krmSignedState.state, &localId);
		if (__builtin_expect(ret == 0, 0))
			ret = KrmDriverCopyToUser(keyId, &localId, sizeof(localId));
		__builtin_memset(&localId, 0, sizeof(localId));
	}
	if (__builtin_expect(!KrmDriverGetState(), 1)) {
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
	}
	KrmDriverUnlock();
leave:
	sceSyscallLeave(syscall);
	return ret;
denied:
	ret = (int)0x80a00006;
	KrmDriverUnlock();
	goto leave;
}

int SceKrm_350C8251(const void *wrappingKeyId, const KrmKey *key)
{
	KrmKey localKey;
	SceOpenPsId localId;
	unsigned int syscall = sceSyscallEnter();
	int ret;

	ret = KrmDriverLockProcess();
	if (ret < 0)
		goto leave;
	ret = (int)0x80a00000;
	if (__builtin_expect(!key, 0))
		goto checkState;
	if (__builtin_expect(wrappingKeyId != NULL, 1)) {
		if (__builtin_expect(!KrmDriverIsAccessible(wrappingKeyId, 16, 1) ||
		    !KrmDriverIsAccessible(key, sizeof(*key), 1), 0))
			goto checkState;
		ret = KrmDriverCopyFromUser(&localId, wrappingKeyId, sizeof(localId));
		if (ret < 0)
			goto clearId;
		wrappingKeyId = &localId;
	} else if (!KrmDriverIsAccessible(key, sizeof(*key), 1)) {
		goto checkState;
	}
	ret = KrmDriverCopyFromUser(&localKey, key, sizeof(localKey));
	if (__builtin_expect(ret == 0, 0))
		ret = KrmImportKey(&g_krmSignedState.state, wrappingKeyId, &localKey);
	memset(&localKey, 0, sizeof(localKey));
clearId:
	__builtin_memset(&localId, 0, sizeof(localId));
checkState:
	if (__builtin_expect(!KrmDriverGetState(), 1)) {
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
	}
unlock:
	KrmDriverUnlock();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int SceKrm_2528D613(const void *wrappingKeyId, const void *keyId, KrmKey *key)
{
	KrmKey localKey;
	SceOpenPsId localWrappingId;
	SceOpenPsId localId;
	unsigned int syscall = sceSyscallEnter();
	int ret;

	ret = KrmDriverLockProcess();
	if (ret < 0)
		goto leave;
	ret = (int)0x80a00000;
	if (__builtin_expect(!key || !KrmDriverIsAccessible(key, sizeof(*key), 2), 0))
		goto checkState;
	if (wrappingKeyId) {
		if (__builtin_expect(!KrmDriverIsAccessible(wrappingKeyId, 16, 1), 0))
			goto checkState;
		ret = KrmDriverCopyFromUser(&localWrappingId, wrappingKeyId, 16);
		if (ret < 0)
			goto clearWrappingId;
		wrappingKeyId = &localWrappingId;
	}
	if (keyId) {
		if (__builtin_expect(!KrmDriverIsAccessible(keyId, 16, 1), 0)) {
			ret = (int)0x80a00000;
			goto checkState;
		}
		ret = KrmDriverCopyFromUser(&localId, keyId, 16);
		if (ret < 0)
			goto clearId;
		keyId = &localId;
	}
	ret = KrmExportKey(&g_krmSignedState.state, wrappingKeyId, keyId, &localKey);
	if (__builtin_expect(ret == 0, 0))
		ret = KrmDriverCopyToUser(key, &localKey, sizeof(localKey));
	memset(&localKey, 0, sizeof(localKey));
clearId:
	__builtin_memset(&localId, 0, sizeof(localId));
clearWrappingId:
	__builtin_memset(&localWrappingId, 0, sizeof(localWrappingId));
checkState:
	if (__builtin_expect(!KrmDriverGetState(), 1)) {
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
	}
unlock:
	KrmDriverUnlock();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int SceKrm_949506DC(const KrmKey *key)
{
	KrmKey local;
	SceUID pid;
	SceUID currentPid;
	unsigned int syscall = sceSyscallEnter();
	int ret;

	if (__builtin_expect(!g_krmKeysInitialized, 0)) {
		ret = (int)0x80a0000a;
		goto leave;
	}
	ret = KrmDriverLock();
	if (ret < 0)
		goto leave;
	if (__builtin_expect(!KrmDriverGetState(), 0)) {
		ret = (int)0x80a0000a;
		KrmDriverUnlock();
		goto leave;
	}
	pid = KrmDriverGetPID();
	currentPid = ksceKernelGetProcessId();
	if (pid == -1) {
		if (!KrmDriverGetAuthorization(currentPid))
			goto denied;
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
		sceKrmDriverSetPID(currentPid);
	} else if (pid != currentPid) {
		goto denied;
	}
	ret = (int)0x80a00000;
	if (__builtin_expect(key && KrmDriverIsAccessible(key, sizeof(*key), 1), 1)) {
		ret = KrmDriverCopyFromUser(&local, key, sizeof(local));
		if (__builtin_expect(ret == 0, 0))
			ret = sceKrmInternalGenerateKey(&g_krmSignedState.state, &local);
		memset(&local, 0, sizeof(local));
	}
	if (__builtin_expect(!KrmDriverGetState(), 1)) {
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
	}
	KrmDriverUnlock();
leave:
	sceSyscallLeave(syscall);
	return ret;
denied:
	ret = (int)0x80a00006;
	KrmDriverUnlock();
	goto leave;
}

int SceKrm_18DE25C6(const void *keyId)
{
	SceOpenPsId local;
	SceUID pid;
	SceUID currentPid;
	unsigned int syscall = sceSyscallEnter();
	int ret;

	if (!g_krmKeysInitialized) {
		ret = (int)0x80a0000a;
		goto leave;
	}
	ret = KrmDriverLock();
	if (ret < 0)
		goto leave;
	if (!KrmDriverGetState()) {
		ret = (int)0x80a0000a;
		KrmDriverUnlock();
		goto leave;
	}
	pid = KrmDriverGetPID();
	currentPid = ksceKernelGetProcessId();
	if (pid == -1) {
		if (!KrmDriverGetAuthorization(currentPid))
			goto denied;
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
		sceKrmDriverSetPID(currentPid);
	} else if (pid != currentPid) {
		goto denied;
	}
	ret = (int)0x80a00000;
	if (__builtin_expect(keyId &&
			     KrmDriverIsAccessible(keyId, sizeof(local), 1), 1)) {
		ret = KrmDriverCopyFromUser(&local, keyId, sizeof(local));
		if (__builtin_expect(ret == 0, 0))
			ret = KrmDeleteKey(&g_krmSignedState.state, &local);
		__builtin_memset(&local, 0, sizeof(local));
	}
	if (__builtin_expect(!KrmDriverGetState(), 1)) {
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
	}
	KrmDriverUnlock();
leave:
	sceSyscallLeave(syscall);
	return ret;
denied:
	ret = (int)0x80a00006;
	KrmDriverUnlock();
	goto leave;
}

int SceKrm_5296A954(const KrmCryptRequest *request)
{
	SceUID pid;
	SceUID currentPid;
	unsigned int syscall = sceSyscallEnter();
	int ret;

	if (__builtin_expect(!g_krmKeysInitialized, 0)) {
		ret = (int)0x80a0000a;
		goto leave;
	}
	ret = KrmDriverLock();
	if (__builtin_expect(ret < 0, 0))
		goto leave;
	if (!KrmDriverGetState()) {
		KrmDriverUnlock();
		sceSyscallLeave(syscall);
		return (int)0x80a0000a;
	}
	pid = KrmDriverGetPID();
	currentPid = ksceKernelGetProcessId();
	if (pid == -1) {
		if (!KrmDriverGetAuthorization(currentPid))
			goto denied;
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
		sceKrmDriverSetPID(currentPid);
	} else if (pid != currentPid) {
		goto denied;
	}
	ret = KrmDriverCryptUserData(request, 1);
	if (__builtin_expect(!KrmDriverGetState(), 1)) {
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
	}
	KrmDriverUnlock();
leave:
	sceSyscallLeave(syscall);
	return ret;
denied:
	ret = (int)0x80a00006;
	KrmDriverUnlock();
	goto leave;
}

int SceKrm_7251237D(const KrmCryptRequest *request)
{
	SceUID pid;
	SceUID currentPid;
	unsigned int syscall = sceSyscallEnter();
	int ret;

	if (__builtin_expect(!g_krmKeysInitialized, 0)) {
		ret = (int)0x80a0000a;
		goto leave;
	}
	ret = KrmDriverLock();
	if (__builtin_expect(ret < 0, 0))
		goto leave;
	if (!KrmDriverGetState()) {
		KrmDriverUnlock();
		sceSyscallLeave(syscall);
		return (int)0x80a0000a;
	}
	pid = KrmDriverGetPID();
	currentPid = ksceKernelGetProcessId();
	if (pid == -1) {
		if (!KrmDriverGetAuthorization(currentPid))
			goto denied;
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
		sceKrmDriverSetPID(currentPid);
	} else if (pid != currentPid) {
		goto denied;
	}
	ret = KrmDriverCryptUserData(request, 0);
	if (__builtin_expect(!KrmDriverGetState(), 1)) {
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
	}
	KrmDriverUnlock();
leave:
	sceSyscallLeave(syscall);
	return ret;
denied:
	ret = (int)0x80a00006;
	KrmDriverUnlock();
	goto leave;
}

int SceKrm_A9201DA3(const KrmSignatureRequest *request)
{
	KrmSignatureRequest local;
	SceOpenPsId keyId;
	KrmEccSignature signature;
	SceUID pid;
	SceUID currentPid;
	unsigned int syscall = sceSyscallEnter();
	int ret;

	if (!g_krmKeysInitialized) {
		ret = (int)0x80a0000a;
		goto leave;
	}
	ret = KrmDriverLock();
	if (ret < 0)
		goto leave;
	if (!KrmDriverGetState()) {
		ret = (int)0x80a0000a;
		KrmDriverUnlock();
		goto leave;
	}
	pid = KrmDriverGetPID();
	currentPid = ksceKernelGetProcessId();
	if (pid == -1) {
		if (!KrmDriverGetAuthorization(currentPid))
			goto denied;
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
		sceKrmDriverSetPID(currentPid);
	} else if (pid != currentPid) {
		goto denied;
	}
	ret = (int)0x80a00000;
	if (!request || !KrmDriverIsAccessible(request, sizeof(*request), 1))
		goto checkState;
	if (KrmDriverCopyFromUser(&local, request, sizeof(local)) < 0)
		goto invalid;
	if (!local.src || !local.signature)
		goto invalid;
	if (local.srcSize > 4096 || local.signatureSize > sizeof(signature))
		goto invalid;
	if (local.keyId && !KrmDriverIsAccessible(local.keyId, 16, 1))
		goto invalid;
	if (!KrmDriverIsAccessible(local.src, local.srcSize, 1) ||
	    !KrmDriverIsAccessible(local.signature, local.signatureSize, 2))
		goto invalid;
	if (local.keyId) {
		ret = KrmDriverCopyFromUser(&keyId, local.keyId, 16);
		if (ret < 0)
			goto clearKeyId;
		local.keyId = &keyId;
	}
	ret = KrmDriverCopyFromUser(g_krmCryptBuffer, local.src, local.srcSize);
	if (ret >= 0) {
		ret = KrmSignData(&g_krmSignedState.state, local.keyId, g_krmCryptBuffer,
				  local.srcSize, &signature, local.signatureSize, local.digest);
		if (ret == 0)
			ret = KrmDriverCopyToUser(local.signature, &signature, local.signatureSize);
		memset(&signature, 0, sizeof(signature));
	}
	memset(g_krmCryptBuffer, 0, sizeof(g_krmCryptBuffer));
clearKeyId:
	__builtin_memset(&keyId, 0, sizeof(keyId));
	goto clearLocal;
invalid:
	ret = (int)0x80a00000;
clearLocal:
	__builtin_memset(&local, 0, sizeof(local));
checkState:
	if (__builtin_expect(!KrmDriverGetState(), 1)) {
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
	}
	KrmDriverUnlock();
leave:
	sceSyscallLeave(syscall);
	return ret;
denied:
	ret = (int)0x80a00006;
	KrmDriverUnlock();
	goto leave;
}

int SceKrm_F0B626E7(const KrmSignatureRequest *request)
{
	KrmSignatureRequest local;
	SceOpenPsId keyId;
	KrmEccSignature signature;
	SceUID pid;
	SceUID currentPid;
	unsigned int syscall = sceSyscallEnter();
	int ret;

	if (!g_krmKeysInitialized) {
		ret = (int)0x80a0000a;
		goto leave;
	}
	ret = KrmDriverLock();
	if (ret < 0)
		goto leave;
	if (!KrmDriverGetState()) {
		ret = (int)0x80a0000a;
		KrmDriverUnlock();
		goto leave;
	}
	pid = KrmDriverGetPID();
	currentPid = ksceKernelGetProcessId();
	if (pid == -1) {
		if (!KrmDriverGetAuthorization(currentPid))
			goto denied;
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
		sceKrmDriverSetPID(currentPid);
	} else if (pid != currentPid) {
		goto denied;
	}
	ret = (int)0x80a00000;
	if (!request || !KrmDriverIsAccessible(request, sizeof(*request), 1))
		goto checkState;
	if (KrmDriverCopyFromUser(&local, request, sizeof(local)) < 0)
		goto invalid;
	if (local.srcSize > 4096 || local.signatureSize > sizeof(signature))
		goto invalid;
	if (!KrmDriverIsAccessible(local.src, local.srcSize, 1) ||
	    !KrmDriverIsAccessible(local.signature, local.signatureSize, 1))
		goto invalid;
	if (local.keyId) {
		ret = KrmDriverCopyFromUser(&keyId, local.keyId, 16);
		if (ret < 0)
			goto clearKeyId;
		local.keyId = &keyId;
	}
	ret = KrmDriverCopyFromUser(g_krmCryptBuffer, local.src, local.srcSize);
	if (ret >= 0) {
		ret = KrmDriverCopyFromUser(&signature, local.signature, local.signatureSize);
		if (ret >= 0)
			ret = KrmVerifyData(&g_krmSignedState.state, local.keyId, g_krmCryptBuffer,
					    local.srcSize, &signature, local.signatureSize, local.digest);
		memset(&signature, 0, sizeof(signature));
	}
	memset(g_krmCryptBuffer, 0, sizeof(g_krmCryptBuffer));
clearKeyId:
	__builtin_memset(&keyId, 0, sizeof(keyId));
	goto clearLocal;
invalid:
	ret = (int)0x80a00000;
clearLocal:
	__builtin_memset(&local, 0, sizeof(local));
checkState:
	if (__builtin_expect(!KrmDriverGetState(), 1)) {
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
	}
	KrmDriverUnlock();
leave:
	sceSyscallLeave(syscall);
	return ret;
denied:
	ret = (int)0x80a00006;
	KrmDriverUnlock();
	goto leave;
}

int SceKrm_41547BE1(const void *keyId, KrmKey *key)
{
	KrmKey local;
	SceOpenPsId localId;
	unsigned int syscall = sceSyscallEnter();
	int ret;

	ret = KrmDriverLockProcess();
	if (ret < 0)
		goto leave;
	ret = (int)0x80a00000;
	if (__builtin_expect(!key, 0))
		goto checkState;
	if (__builtin_expect(keyId != NULL, 1)) {
		if (__builtin_expect(!KrmDriverIsAccessible(keyId, 16, 1) ||
		    !KrmDriverIsAccessible(key, sizeof(*key), 2), 0))
			goto checkState;
		ret = KrmDriverCopyFromUser(&localId, keyId, 16);
		if (ret < 0)
			goto clearId;
		keyId = &localId;
	} else if (!KrmDriverIsAccessible(key, sizeof(*key), 2)) {
		goto checkState;
	}
	ret = KrmGetKeyInfo(&g_krmSignedState.state, keyId, &local);
	if (__builtin_expect(ret == 0, 0))
		ret = KrmDriverCopyToUser(key, &local, sizeof(local));
	memset(&local, 0, sizeof(local));
clearId:
	__builtin_memset(&localId, 0, sizeof(localId));
checkState:
	if (__builtin_expect(!KrmDriverGetState(), 1)) {
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
	}
unlock:
	KrmDriverUnlock();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int SceKrm_7401B932(int index, KrmKey *key)
{
	KrmKey local;
	SceUID pid;
	SceUID currentPid;
	unsigned int syscall = sceSyscallEnter();
	int ret;

	if (!g_krmKeysInitialized) {
		ret = (int)0x80a0000a;
		goto leave;
	}
	ret = KrmDriverLock();
	if (ret < 0)
		goto leave;
	if (!KrmDriverGetState()) {
		ret = (int)0x80a0000a;
		KrmDriverUnlock();
		goto leave;
	}
	pid = KrmDriverGetPID();
	currentPid = ksceKernelGetProcessId();
	if (pid == -1) {
		if (!KrmDriverGetAuthorization(currentPid))
			goto denied;
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
		sceKrmDriverSetPID(currentPid);
	} else if (pid != currentPid) {
		goto denied;
	}
	ret = (int)0x80a00000;
	if (__builtin_expect(key && KrmDriverIsAccessible(key, sizeof(*key), 2), 1)) {
		ret = KrmGetKeyInfoByIndex(&g_krmSignedState.state, index, &local);
		if (__builtin_expect(ret == 0, 0))
			ret = KrmDriverCopyToUser(key, &local, sizeof(local));
		memset(&local, 0, sizeof(local));
	}
	if (__builtin_expect(!KrmDriverGetState(), 1)) {
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
	}
	KrmDriverUnlock();
leave:
	sceSyscallLeave(syscall);
	return ret;
denied:
	ret = (int)0x80a00006;
	KrmDriverUnlock();
	goto leave;
}

int SceKrm_D7B35BE6(KrmKeyCounts *counts)
{
	KrmKeyCounts local;
	SceUID pid;
	SceUID currentPid;
	unsigned int syscall = sceSyscallEnter();
	int ret;

	if (!g_krmKeysInitialized) {
		ret = (int)0x80a0000a;
		goto leave;
	}
	ret = KrmDriverLock();
	if (ret < 0)
		goto leave;
	if (!KrmDriverGetState()) {
		ret = (int)0x80a0000a;
		KrmDriverUnlock();
		goto leave;
	}
	pid = KrmDriverGetPID();
	currentPid = ksceKernelGetProcessId();
	if (pid == -1) {
		if (!KrmDriverGetAuthorization(currentPid))
			goto denied;
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
		sceKrmDriverSetPID(currentPid);
	} else if (pid != currentPid) {
		goto denied;
	}
	ret = (int)0x80a00000;
	if (__builtin_expect(counts &&
			     KrmDriverIsAccessible(counts, sizeof(*counts), 2), 1)) {
		ret = KrmGetKeyCounts(&g_krmSignedState.state, &local);
		if (__builtin_expect(ret == 0, 0))
			ret = KrmDriverCopyToUser(counts, &local, sizeof(local));
		__builtin_memset(&local, 0, sizeof(local));
	}
	if (__builtin_expect(!KrmDriverGetState(), 1)) {
		memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
		__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
		__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
		sceKrmDriverSetPID(-1);
	}
	KrmDriverUnlock();
leave:
	sceSyscallLeave(syscall);
	return ret;
denied:
	ret = (int)0x80a00006;
	KrmDriverUnlock();
	goto leave;
}

int SceKrm_3257BD35(KrmSignedNonce *nonce)
{
	KrmSignedNonce local;
	unsigned int syscall = sceSyscallEnter();
	int ret;

	ret = KrmLockChallenge();
	if (ret >= 0) {
		if (__builtin_expect(nonce && KrmDriverIsAccessible(nonce, sizeof(*nonce), 2), 1)) {
			ret = KrmCreateSignedNonce(&local);
			if (__builtin_expect(ret == 0, 0))
				ret = KrmDriverCopyToUser(nonce, &local, sizeof(local));
			memset(&local, 0, sizeof(local));
		} else {
			ret = (int)0x80a00000;
		}
		KrmUnlockChallenge();
	}
	sceSyscallLeave(syscall);
	return ret;
}

int SceKrm_ECF53237(const KrmChallenge *challenge, KrmChallengeResponse *response)
{
	unsigned int syscall = sceSyscallEnter();
	int ret;

	ret = KrmLockChallenge();
	if (ret >= 0) {
		ret = KrmDriverRespondToChallenge(challenge, response);
		KrmUnlockChallenge();
	}
	sceSyscallLeave(syscall);
	return ret;
}
