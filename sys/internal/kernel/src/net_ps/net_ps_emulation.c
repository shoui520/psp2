/* SCE packet emulation; original source filename is unavailable. */

#include "net_ps_internal.h"
#include <string.h>

extern SceNetPsPacketService gNetPsPacketService;
int sceNetPsStartBandwidthTimer(struct ifnet *interface);
extern int gNetPsSoftTick;
void callout_reset(struct callout *timer, int ticks,
		   void (*callback)(void *), void *argument);
void sceNetPsDelayTimer(void *argument);
extern SceNetPsMutex gNetPsPktMutex;
extern SceNetPsMutex gNetPsDevSendMutex;
int sceNetPsIsApplicationProgram(int *error);
int sceNetPsIsCEX(void);
int sceNetPsIsDevelopmentMode(void);
int sceNetPsIsSdkVersion2OrLater(void);
int sceNetPsSeedEmulationRandom(unsigned int seed);
unsigned int sceNetPsGetEmulationSeed(void);
unsigned int sceNetPsEmulationRandom(void);
void callout_init(struct callout *timer);
void callout_stop(struct callout *timer);

/* SCE: convert each named emulation field and its policy entries in place. */
void sceNetPsSwapEmulationData(SceNetEmulationData *data)
{
	unsigned short dropRate = __builtin_bswap16(data->drop_rate);
	unsigned short dropDuration = __builtin_bswap16(data->drop_duration);
	unsigned short passDuration = __builtin_bswap16(data->pass_duration);
	unsigned short delayTime = __builtin_bswap16(data->delay_time);
	unsigned short delayJitter = __builtin_bswap16(data->delay_jitter);
	unsigned short orderRate = __builtin_bswap16(data->order_rate);
	unsigned short orderDelay = __builtin_bswap16(data->order_delay_time);
	unsigned short duplicationRate = __builtin_bswap16(data->duplication_rate);
	unsigned int bandwidth = __builtin_bswap32(data->bps_limit);
	unsigned short lowerSize = __builtin_bswap16(data->lower_size_limit);
	unsigned short upperSize = __builtin_bswap16(data->upper_size_limit);
	unsigned int systemPolicy = __builtin_bswap32(data->system_policy_pattern);
	unsigned int gamePolicy = __builtin_bswap32(data->game_policy_pattern);
	unsigned short *policy = data->policy_flags;
	unsigned int count = 64;

	data->delay_jitter = delayJitter;
	data->order_rate = orderRate;
	data->drop_rate = dropRate;
	data->drop_duration = dropDuration;
	data->pass_duration = passDuration;
	data->delay_time = delayTime;
	data->order_delay_time = orderDelay;
	data->duplication_rate = duplicationRate;
	data->bps_limit = bandwidth;
	data->lower_size_limit = lowerSize;
	data->upper_size_limit = upperSize;
	data->system_policy_pattern = systemPolicy;
	data->game_policy_pattern = gamePolicy;
	do {
		*policy = __builtin_bswap16(*policy);
		++policy;
	} while (--count);
}

/* SCE: convert the parameter header and both directional configurations. */
void sceNetPsSwapEmulationParam(SceNetEmulationParam *parameters)
{
	unsigned short version = __builtin_bswap16(parameters->version);
	unsigned short option = __builtin_bswap16(parameters->option_number);
	unsigned short currentVersion = __builtin_bswap16(parameters->current_version);
	unsigned short result = __builtin_bswap16(parameters->result);
	unsigned int flags = __builtin_bswap32(parameters->flags);
	unsigned int seed = __builtin_bswap32(parameters->seed);

	parameters->version = version;
	parameters->option_number = option;
	parameters->current_version = currentVersion;
	parameters->result = result;
	parameters->flags = flags;
	parameters->seed = seed;
	sceNetPsSwapEmulationData(&parameters->send);
	sceNetPsSwapEmulationData(&parameters->recv);
}

/* SCE: replace the parameters with one of the four built-in presets. */
int sceNetPsSetEmulationPreset(SceNetEmulationParam *parameters)
{
	unsigned int option = parameters->option_number;

	memset(parameters, 0, sizeof(*parameters));
	switch (option) {
	case 4:
		parameters->send.delay_time = 230;
		parameters->send.delay_jitter = 110;
		parameters->send.drop_rate = 1;
		parameters->send.order_rate = 1;
		parameters->send.duplication_rate = 1;
		parameters->recv.drop_rate = 1;
		parameters->send.order_delay_time = 200;
		parameters->send.bps_limit = 0x10000;
		parameters->send.system_policy_pattern = -1u;
		parameters->send.game_policy_pattern = -1u;
		parameters->recv.system_policy_pattern = -1u;
		parameters->recv.game_policy_pattern = -1u;
		parameters->recv.bps_limit = 0x20000;
		break;
	case 3:
		parameters->send.drop_rate = 5;
		parameters->send.system_policy_pattern = -1u;
		parameters->send.game_policy_pattern = -1u;
		break;
	case 2:
		parameters->send.bps_limit = 0x20000;
		parameters->send.system_policy_pattern = -1u;
		parameters->send.game_policy_pattern = -1u;
		break;
	case 1:
		parameters->send.delay_time = 100;
		parameters->send.delay_jitter = 0;
		parameters->send.system_policy_pattern = -1u;
		parameters->send.game_policy_pattern = -1u;
		break;
	default:
		return -1;
	}
	parameters->option_number = option;
	parameters->current_version = 0x104;
	return 0;
}

/* SCE: replenish the driver's emulated bandwidth and resume queued traffic. */
void sceNetPsBandwidthTimer(void *argument)
{
	struct ifnet *interface = argument;
	struct SceNetPsDriverContext *context = interface->if_softc;
	struct NetDriver *driver = context->driver;
	unsigned int flags = context->emulation.flags & 1;
	unsigned int limit, bytes, portion, remaining, limited;

	gNetPsPacketService.flags &= ~0xf00000u;
	limit = context->emulation.send.bps_limit;
	bytes = context->sendBpsBytes;
	if (flags && context->sendBpsPackets == 0)
		limited = 0;
	else
		limited = limit && limit < 8 * bytes;
	portion = limit / 80 + 1;
	if (portion > bytes) {
		context->sendBpsBytes = 0;
	} else {
		remaining = bytes - portion;
		portion *= 50;
		if (remaining < portion)
			portion = remaining;
		context->sendBpsBytes = portion;
	}
	context->sendBpsPackets = 0;
	if (limited && (flags || !limit || limit >= 8 * context->sendBpsBytes))
		gNetPsPacketService.kickTx(driver, 1);
	bytes = context->receiveBpsBytes;
	portion = context->emulation.recv.bps_limit / 80 + 1;
	if (portion > bytes) {
		context->receiveBpsBytes = 0;
	} else {
		remaining = bytes - portion;
		portion *= 50;
		if (remaining < portion)
			portion = remaining;
		context->receiveBpsBytes = portion;
	}
	context->receiveBpsPackets = 0;
	sceNetPsStartBandwidthTimer(interface);
}

/* SCE: advance the first queued packet and arm its remaining delay. */
int sceNetPsCheckPacketDelay(unsigned int *releaseTime, unsigned int *flags,
			    unsigned char *firstPacket, struct ifnet *interface)
{
	struct SceNetPsDriverContext *context = interface->if_softc;
	struct timeval now;
	unsigned int deadline;

	if (!*firstPacket) {
		if (*flags & 1) {
			*flags &= ~1u;
			*releaseTime -= context->emulation.send.order_delay_time;
		} else {
			sceNetPsGetTime(&now);
			*releaseTime = now.tv_usec / 1000 + 1000 * now.tv_sec;
		}
		*firstPacket = 1;
	}
	deadline = *releaseTime;
	sceNetPsGetTime(&now);
	if (deadline <= now.tv_usec / 1000 + 1000 * now.tv_sec)
		return 0;
	deadline = *releaseTime;
	sceNetPsGetTime(&now);
	callout_reset(&gNetPsPacketService.delayCallout,
		      1000 * (deadline - (now.tv_usec / 1000 + 1000 * now.tv_sec)) /
		      gNetPsSoftTick, sceNetPsDelayTimer, interface);
	gNetPsPacketService.flags |= 0x10000;
	if (*flags & 1)
		gNetPsPacketService.flags |= 0x20000;
	return 1;
}

/* SCE: test the ready and pending transmit queues against their deadlines. */
int sceNetPsProcessDelayedPackets(struct ifnet *interface)
{
	struct SceNetPsDriverContext *context = interface->if_softc;
	struct NetDriverPacket *packet;
	struct timeval now;
	unsigned char firstPacket = 1;
	int delayed = 1;

	if ((gNetPsPacketService.flags & 0xf0000) == 0x10000)
		return 1;
	sce_psnet_bnet_mutex_lock(&gNetPsPktMutex, NULL);
	packet = gNetPsPacketService.prepareQueue(&context->sendQueue, 1, 0);
	if (!packet)
		goto unlockReady;
	if (gNetPsPacketService.flags & 0x20000) {
		gNetPsPacketService.flags &= ~0xf0000u;
		sceNetPsGetTime(&now);
		delayed = 0;
		firstPacket = 0;
		packet->releaseTimeMs = now.tv_usec / 1000 + 1000 * now.tv_sec;
		packet = packet->next;
	}
	for (; packet; packet = packet->next) {
		if (sceNetPsCheckPacketDelay(&packet->releaseTimeMs,
					    &packet->emulationFlags, &firstPacket,
					    interface))
			goto unlockReady;
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsPktMutex);
	sce_psnet_bnet_mutex_lock(&gNetPsDevSendMutex, NULL);
	for (packet = context->sendQueue.pendingHead; packet; packet = packet->next) {
		if (sceNetPsCheckPacketDelay(&packet->releaseTimeMs,
					    &packet->emulationFlags, &firstPacket,
					    interface)) {
			sce_psnet_bnet_mutex_unlock(&gNetPsDevSendMutex);
			return delayed;
		}
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsDevSendMutex);
	return 0;

unlockReady:
	sce_psnet_bnet_mutex_unlock(&gNetPsPktMutex);
	return delayed;
}

/* SCE: restart transmission after the pending packet delay expires. */
void sceNetPsDelayTimer(void *argument)
{
	struct ifnet *interface = argument;
	struct SceNetPsDriverContext *context = interface->if_softc;
	struct NetDriver *driver = context->driver;

	gNetPsPacketService.flags &= ~0xf0000u;
	sceNetPsProcessDelayedPackets(interface);
	gNetPsPacketService.kickTx(driver, 1);
}

/* SCE: validate the SDK parameters before changing driver emulation. */
int sceNetPsSetDriverEmulation(struct SceNetPsDriverContext *context,
			       SceNetEmulationParam *parameters,
			       int networkOrder, int privileged)
{
	struct NetDriver *driver;
	struct ifnet *interface;
	unsigned int ownerFlag, presetFlag, index;
	unsigned short errorField;
	int result;

	if (!context)
		return -19;
	driver = context->driver;
	interface = context->interface;
	if (networkOrder)
		sceNetPsSwapEmulationParam(parameters);
	if (parameters->version &&
	    ((parameters->version & 0xff) > 4 ||
	     (parameters->version & 0xff00) != 0x100))
		return -2;
	if (parameters->option_number && sceNetPsSetEmulationPreset(parameters) < 0) {
		errorField = offsetof(SceNetEmulationParam, option_number);
		goto invalid;
	}
	if (parameters->current_version)
		parameters->current_version = 0;
	if (parameters->result)
		parameters->result = 0;
	parameters->flags &= ~0x700u;
	if (parameters->flags & ~1u) {
		errorField = offsetof(SceNetEmulationParam, flags);
		goto invalid;
	}
	if (parameters->reserved1) {
		errorField = offsetof(SceNetEmulationParam, reserved1);
		goto invalid;
	}
	if (!parameters->seed)
		parameters->seed = sceNetPsGetEmulationSeed();
	sceNetPsSeedEmulationRandom(parameters->seed);
	for (index = 0; index < sizeof(parameters->reserved); ++index) {
		if (parameters->reserved[index]) {
			errorField = offsetof(SceNetEmulationParam, reserved);
			goto invalid;
		}
	}
#define CHECK_EMULATION(member, condition) \
	do { \
		if (condition) { \
			errorField = offsetof(SceNetEmulationParam, member); \
			goto invalid; \
		} \
	} while (0)
	CHECK_EMULATION(send.drop_rate, parameters->send.drop_rate > 100);
	CHECK_EMULATION(send.drop_duration, parameters->send.drop_duration > 8000);
	CHECK_EMULATION(send.pass_duration, parameters->send.pass_duration > 8000);
	CHECK_EMULATION(send.delay_time, parameters->send.delay_time > 8000);
	CHECK_EMULATION(send.delay_jitter,
			parameters->send.delay_jitter > parameters->send.delay_time);
	CHECK_EMULATION(send.order_rate, parameters->send.order_rate > 100);
	CHECK_EMULATION(send.order_delay_time, parameters->send.order_delay_time > 8000);
	CHECK_EMULATION(send.duplication_rate, parameters->send.duplication_rate > 100);
	CHECK_EMULATION(send.bps_limit, parameters->send.bps_limit > 0x40000000 ||
			parameters->send.bps_limit - 1 <= 0x3fe);
	CHECK_EMULATION(send.lower_size_limit, parameters->send.upper_size_limit &&
			parameters->send.lower_size_limit > parameters->send.upper_size_limit);
	for (index = 0; index < 64; ++index)
		CHECK_EMULATION(send.policy_flags, parameters->send.policy_flags[index]);
	for (index = 0; index < sizeof(parameters->send.reserved); ++index)
		CHECK_EMULATION(send.reserved, parameters->send.reserved[index]);
	CHECK_EMULATION(recv.drop_rate, parameters->recv.drop_rate > 100);
	CHECK_EMULATION(recv.drop_duration, parameters->recv.drop_duration);
	CHECK_EMULATION(recv.pass_duration, parameters->recv.pass_duration);
	CHECK_EMULATION(recv.delay_time, parameters->recv.delay_time);
	CHECK_EMULATION(recv.delay_jitter, parameters->recv.delay_jitter);
	CHECK_EMULATION(recv.order_rate, parameters->recv.order_rate);
	CHECK_EMULATION(recv.order_delay_time, parameters->recv.order_delay_time);
	CHECK_EMULATION(recv.duplication_rate, parameters->recv.duplication_rate);
	CHECK_EMULATION(recv.bps_limit, parameters->recv.bps_limit > 0x40000000 ||
			parameters->recv.bps_limit - 1 <= 0x3fe);
	CHECK_EMULATION(recv.lower_size_limit, parameters->recv.upper_size_limit &&
			parameters->recv.lower_size_limit > parameters->recv.upper_size_limit);
	for (index = 0; index < 64; ++index)
		CHECK_EMULATION(recv.policy_flags, parameters->recv.policy_flags[index]);
	for (index = 0; index < sizeof(parameters->recv.reserved); ++index)
		CHECK_EMULATION(recv.reserved, parameters->recv.reserved[index]);
#undef CHECK_EMULATION
	if (!parameters->send.drop_rate && !parameters->send.drop_duration &&
	    !parameters->send.pass_duration && !parameters->send.delay_time &&
	    !parameters->send.delay_jitter && !parameters->send.order_rate &&
	    !parameters->send.order_delay_time && !parameters->send.duplication_rate &&
	    !parameters->send.bps_limit && !parameters->send.lower_size_limit &&
	    !parameters->send.upper_size_limit && !parameters->recv.drop_rate &&
	    !parameters->recv.bps_limit)
		parameters->seed = 0;
	presetFlag = 0x400;
	ownerFlag = 0;
	if (!privileged) {
		presetFlag = 0x200;
		if (sceNetPsIsApplicationProgram(NULL)) {
			if (context->emulationOwnerFlags & 0x600)
				return -86;
			ownerFlag = 0x100;
			presetFlag = 0x100;
		}
	}
	if (parameters->option_number)
		ownerFlag = presetFlag;
	parameters->flags |= ownerFlag;
	context->emulationOwnerFlags = ownerFlag;
	if (sceNetPsIsCEX())
		return 0;
	if (!sceNetPsIsDevelopmentMode() && sceNetPsIsApplicationProgram(NULL)) {
		result = sceNetPsIsSdkVersion2OrLater();
		if (!result)
			return result;
		return -86;
	}
	if (parameters->send.delay_time ||
	    (parameters->send.order_rate && parameters->send.order_delay_time)) {
		if (!(gNetPsPacketService.flags & 1)) {
			callout_init(&gNetPsPacketService.delayCallout);
			gNetPsPacketService.flags |= 1;
		}
	} else if (gNetPsPacketService.flags & 1) {
		callout_stop(&gNetPsPacketService.delayCallout);
		gNetPsPacketService.flags &= ~1u;
	}
	if (parameters->send.bps_limit || parameters->recv.bps_limit) {
		if (!(gNetPsPacketService.flags & 2)) {
			callout_init(&gNetPsPacketService.bandwidthCallout);
			gNetPsPacketService.activeDriver = driver;
			gNetPsPacketService.flags |= 2;
			sceNetPsStartBandwidthTimer(interface);
		}
	} else if (gNetPsPacketService.flags & 2) {
		callout_stop(&gNetPsPacketService.bandwidthCallout);
		gNetPsPacketService.activeDriver = NULL;
		gNetPsPacketService.flags &= ~2u;
	}
	memcpy(&context->emulation, parameters, sizeof(*parameters));
	gNetPsPacketService.kickTx(driver, 1);
	return 0;

invalid:
	context->emulation.result = errorField;
	return -28;
}

/* SCE: apply frame-size, drop-interval and duplication controls. */
int sceNetPsFilterTxMbuf(struct SceNetPsDriverContext *context,
			struct mbuf *mbuf, unsigned int *duplicates)
{
	struct timeval now;
	int length = mbuf->m_pkthdr.len;
	unsigned int flags, deadline;

	if (context->emulation.send.lower_size_limit > length)
		mbuf->m_flags &= ~0x100000u;
	if (context->emulation.send.upper_size_limit &&
	    length > context->emulation.send.upper_size_limit)
		mbuf->m_flags &= ~0x100000u;
	if (!(mbuf->m_flags & 0x100000))
		return 0;
	flags = gNetPsPacketService.filterFlags;
	if (flags & 1) {
		deadline = gNetPsPacketService.releaseTimeMs;
		sceNetPsGetTime(&now);
		if (deadline > now.tv_usec / 1000 + 1000 * now.tv_sec)
			goto drop;
		flags = gNetPsPacketService.filterFlags & ~1u;
		gNetPsPacketService.filterFlags = flags;
		if (context->emulation.send.pass_duration) {
			sceNetPsGetTime(&now);
			gNetPsPacketService.filterFlags |= 2;
			gNetPsPacketService.releaseTimeMs = now.tv_usec / 1000 +
				1000 * now.tv_sec + context->emulation.send.pass_duration;
			goto duplicate;
		}
	}
	if (flags & 2) {
		deadline = gNetPsPacketService.releaseTimeMs;
		sceNetPsGetTime(&now);
		if (deadline > now.tv_usec / 1000 + 1000 * now.tv_sec)
			goto duplicate;
		gNetPsPacketService.filterFlags &= ~2u;
	}
	if (context->emulation.send.drop_rate) {
		unsigned int random = sceNetPsEmulationRandom();

		if (context->emulation.send.drop_rate > (int)(random % 101)) {
			sceNetPsGetTime(&now);
			gNetPsPacketService.filterFlags |= 1;
			gNetPsPacketService.releaseTimeMs = now.tv_usec / 1000 +
				1000 * now.tv_sec + context->emulation.send.drop_duration;
			goto drop;
		}
	}
duplicate:
	if (context->emulation.send.duplication_rate) {
		unsigned int random = sceNetPsEmulationRandom();

		if (context->emulation.send.duplication_rate > (int)(random % 101))
			*duplicates = 1;
	}
	return 0;

drop:
	m_freem(mbuf);
	++context->txDrops;
	return 1;
}

/* SCE: assign transmit timing, bandwidth and packet-reordering metadata. */
int sceNetPsSetTxEmulation(struct SceNetPsDriverContext *context,
			  struct mbuf *mbuf, struct NetDriverPacket *packet)
{
	struct timeval now;
	unsigned int releaseTime;
	int jitter;

	if (mbuf->m_flags & 0x200000)
		packet->emulationFlags |= 2;
	if (!(mbuf->m_flags & 0x100000))
		return 0;
	sceNetPsGetTime(&now);
	releaseTime = now.tv_usec / 1000 + 1000 * now.tv_sec +
		context->emulation.send.delay_time;
	jitter = context->emulation.send.delay_jitter;
	if (jitter)
		jitter = sceNetPsEmulationRandom() % (2 * jitter + 1) - jitter;
	packet->releaseTimeMs = releaseTime + jitter;
	packet->sequenceNumber = gNetPsPacketService.sequenceNumber++;
	if (context->emulation.send.order_rate) {
		unsigned int random = sceNetPsEmulationRandom();

		if (context->emulation.send.order_rate > (int)(random % 101)) {
			packet->emulationFlags |= 1;
			packet->releaseTimeMs += context->emulation.send.order_delay_time;
		}
	}
	return 0;
}

/* SCE: account for receive bandwidth and mark frames selected for dropping. */
int sceNetPsFilterRxMbuf(struct SceNetPsDriverContext *context,
			struct mbuf *mbuf, unsigned int length)
{
	unsigned int limit = context->emulation.recv.bps_limit;

	if (limit) {
		unsigned int flags = context->emulation.flags;
		unsigned int packets = context->receiveBpsPackets;

		if ((flags & 1) && !packets) {
			context->receiveBpsBytes += length;
			context->receiveBpsPackets = packets + 1;
		} else {
			unsigned int bytes = context->receiveBpsBytes;

			if (limit < 8 * bytes) {
				context->receiveBpsBytes = bytes + length;
				context->receiveBpsPackets = packets + 1;
				goto drop;
			}
			context->receiveBpsBytes = bytes + length;
			context->receiveBpsPackets = packets + 1;
		}
	}
	if (context->emulation.recv.drop_rate) {
		unsigned int random = sceNetPsEmulationRandom();

		if (context->emulation.recv.drop_rate > (int)(random % 101))
			goto drop;
	}
	return 0;

drop:
	mbuf->m_flags |= 0x20000;
	return 1;
}
