/*
 * control.c: the stack's settings and state, through sceNetSyscallControl. A command's
 * argument words are laid out as net_ps reads them (net_ps_socket.c).
 */
#include <libkernel/libkernel.h>
#include "libnet_internal.h"

int libnet_get_connection_param(int ifIndex, SceNetPsConnectionParam *param, int flags)
{
	int kernelIfIndex;

	if (param == NULL)
		return SCE_NET_ERROR_EINVAL;
	if (flags != 0)
		return SCE_NET_ERROR_EINVAL;
	kernelIfIndex = libnet_if_kernel_index(ifIndex);
	if (kernelIfIndex < 0)
		return SCE_NET_ERROR_ENODEV;
	return libnet_control(kernelIfIndex, SCE_NETPS_CONTROL_GET_CONNECTION_PARAM, param,
			      sizeof(*param));
}

int libnet_set_connection_param(int ifIndex, const SceNetPsConnectionParam *param, int flags)
{
	int kernelIfIndex;

	if (param == NULL)
		return SCE_NET_ERROR_EINVAL;
	if (flags != 0)
		return SCE_NET_ERROR_EINVAL;
	kernelIfIndex = libnet_if_kernel_index(ifIndex);
	if (kernelIfIndex < 0)
		return SCE_NET_ERROR_ENODEV;
	return libnet_control(kernelIfIndex, SCE_NETPS_CONTROL_SET_CONNECTION_PARAM,
			      (void *)param, sizeof(*param));
}

int libnet_update_connections(int ifIndex, int argument, int flags)
{
	int kernelIfIndex;
	int args[2];

	if (flags & ~1)
		return SCE_NET_ERROR_EINVAL;
	if (ifIndex == -1) {
		kernelIfIndex = -1;
	} else {
		kernelIfIndex = libnet_if_kernel_index(ifIndex);
		if (kernelIfIndex < 0)
			return SCE_NET_ERROR_ENODEV;
	}
	sceClibMemset(args, 0, sizeof(args));
	args[0] = argument;
	args[1] = flags;
	return libnet_control(kernelIfIndex, SCE_NETPS_CONTROL_UPDATE_CONNECTIONS, args,
			      sizeof(args));
}

int libnet_reset_idle_time(int ifIndex, int argument, int flags)
{
	int kernelIfIndex;
	int value;

	if (flags != 0)
		return SCE_NET_ERROR_EINVAL;
	if (argument != 0)
		return SCE_NET_ERROR_EINVAL;
	if (ifIndex == -1) {
		kernelIfIndex = -1;
	} else {
		kernelIfIndex = libnet_if_kernel_index(ifIndex);
		if (kernelIfIndex < 0)
			return SCE_NET_ERROR_ENODEV;
	}
	sceClibMemset(&value, 0, sizeof(value));
	value = 0;
	return libnet_control(kernelIfIndex, SCE_NETPS_CONTROL_RESET_IDLE_TIME, &value,
			      sizeof(value));
}

int libnet_get_process_policy(int processId, int *policy, int flags)
{
	int args[2];
	int ret;

	if (flags != 0)
		return SCE_NET_ERROR_EINVAL;
	if (policy == NULL)
		return SCE_NET_ERROR_EINVAL;
	sceClibMemset(args, 0, sizeof(args));
	args[0] = processId;
	ret = libnet_control(-1, SCE_NETPS_CONTROL_GET_PROCESS_POLICY, args, sizeof(args));
	if (ret >= 0)
		*policy = args[1];
	return ret;
}

int libnet_set_process_policy(int processId, int policy, int flags)
{
	int args[2];

	if (flags != 0 || (policy & ~3))
		return SCE_NET_ERROR_EINVAL;
	sceClibMemset(args, 0, sizeof(args));
	args[0] = processId;
	args[1] = policy;
	return libnet_control(-1, SCE_NETPS_CONTROL_SET_PROCESS_POLICY, args, sizeof(args));
}

int libnet_get_thread_mode(int threadId, int *mode, int flags)
{
	int args[3];
	int ret;

	if ((flags & ~1) || mode == NULL)
		return SCE_NET_ERROR_EINVAL;
	sceClibMemset(args, 0, sizeof(args));
	args[0] = threadId;
	args[1] = flags;
	ret = libnet_control(-1, SCE_NETPS_CONTROL_GET_THREAD_MODE, args, sizeof(args));
	if (ret >= 0)
		*mode = args[2];
	return ret;
}

int libnet_set_thread_mode(int threadId, int mode, int flags)
{
	int args[3];

	if (flags != 0 || (mode & ~1))
		return SCE_NET_ERROR_EINVAL;
	sceClibMemset(args, 0, sizeof(args));
	args[0] = threadId;
	args[2] = mode;
	return libnet_control(-1, SCE_NETPS_CONTROL_SET_THREAD_MODE, args, sizeof(args));
}

int libnet_get_sleep_counts(SceNetPsSleepCount *counts, int count, int flags)
{
	if (flags != 0)
		return SCE_NET_ERROR_EINVAL;
	return libnet_control(-1, SCE_NETPS_CONTROL_GET_SLEEP_COUNTS, counts,
			      count * sizeof(*counts));
}

int libnet_get_thread_list(SceNetPsThreadInfo *list, int count, int flags)
{
	if (flags != 0)
		return SCE_NET_ERROR_EINVAL;
	return libnet_control(-1, SCE_NETPS_CONTROL_GET_THREAD_LIST, list, count * sizeof(*list));
}

int libnet_get_promiscuous(int ifIndex, SceNetPsPromiscuousParam *param, int flags)
{
	int kernelIfIndex;

	if (param == NULL)
		return SCE_NET_ERROR_EINVAL;
	if (flags != 0)
		return SCE_NET_ERROR_EINVAL;
	kernelIfIndex = libnet_if_kernel_index(ifIndex);
	if (kernelIfIndex < 0)
		return SCE_NET_ERROR_ENODEV;
	return libnet_control(kernelIfIndex, SCE_NETPS_CONTROL_GET_PROMISCUOUS, param,
			      sizeof(*param));
}

int libnet_set_promiscuous(int ifIndex, const SceNetPsPromiscuousParam *param, int flags)
{
	int kernelIfIndex;

	if (param == NULL)
		return SCE_NET_ERROR_EINVAL;
	if (flags != 0)
		return SCE_NET_ERROR_EINVAL;
	kernelIfIndex = libnet_if_kernel_index(ifIndex);
	if (kernelIfIndex < 0)
		return SCE_NET_ERROR_ENODEV;
	return libnet_control(kernelIfIndex, SCE_NETPS_CONTROL_SET_PROMISCUOUS, (void *)param,
			      sizeof(*param));
}

/* the calling process's network error, or with FLAGS bit 0 the calling thread's */
int libnet_get_error(int *error, int flags)
{
	int args[2];
	int ret;

	if (error == NULL || (flags & ~1))
		return SCE_NET_ERROR_EINVAL;
	sceClibMemset(args, 0, sizeof(args));
	if (flags != 0)
		args[1] |= 1;
	ret = libnet_control(-1, SCE_NETPS_CONTROL_GET_ERROR, args, sizeof(args));
	if (ret >= 0)
		*error = args[0];
	return ret;
}

int libnet_set_error(int error, int flags)
{
	int args[2];

	if (flags & ~1)
		return SCE_NET_ERROR_EINVAL;
	sceClibMemset(args, 0, sizeof(args));
	if (flags != 0)
		args[1] |= 1;
	args[0] = error;
	return libnet_control(-1, SCE_NETPS_CONTROL_SET_ERROR, args, sizeof(args));
}
