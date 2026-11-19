/*
 * DOL-1001's temperature sensor: its registers over the I2C bus, the local temperature read
 * by task 5 (thermalPoll), which also gives the charge alarms their temperature, and the
 * statistics the host reads with serial command 0x01e0 and clears with 0x01e1: the highest
 * temperature ever (log record 0x1c), the highest since a clear, and for each of two modes
 * the power jobs set the minimum, maximum and average of samples taken each period.
 */
#pragma sfr

#include <stddef.h>

#include "ernie.h"
#include "analog.h"
#include "charge.h"
#include "config.h"
#include "iica.h"
#include "kernel.h"
#include "log.h"
#include "powerseq.h"
#include "powerwait.h"
#include "state.h"
#include "thermal.h"

#if ANALOG_THERMISTOR
struct Thermal thermal;

/* task 5: the thermistor's temperature (analog.c), counted and given to the charge alarms in
   0.1 K */
void thermalPoll(void)
{
	unsigned char status;
	signed char temperature;

	status = thermalReadTemp(&temperature);
	if (status == 0) {
		thermalStatsUpdate(temperature);
		chargeSetTemp((int)temperature * 10 + 2732);
	}
}

void thermalInit(void)
{
	thermalStatsInit();
	return;
}

int thermalReadTemp(signed char *temperature)
{
	unsigned char status;

	analogThermistorRead(temperature);
	status = 0;
	return status;
}

/* in 0.1 K */
int thermalReadKelvin(unsigned int *temperature)
{
	unsigned char status;
	signed char celsius;

	status = thermalReadTemp(&celsius);
	if (status == 0)
		*temperature = (int)celsius * 10 + 2732;
	return status;
}
#else
static struct IicaRequest thermalIica;
struct Thermal thermal;

/* task 5: the temperature read, counted and given to the charge alarms in 0.1 K */
void thermalPoll(void)
{
	signed char temperature;

	if (deviceVersions.thermal[0] == THERMAL_ID) {
		thermalReadTemp(&temperature);
		thermalStatsUpdate(temperature);
		chargeSetTemp((int)temperature * 10 + 2732);
	}
}

/* at the power-on: the settings the sensor is given (thermalStart) */
void thermalInit(void)
{
	thermal.temperature = 0;
	thermal.config.byte = 0x84;
	thermal.rate = 4;
	thermalStatsInit();
	return;
}

void thermalStart(void)
{
	signed char temperature;

	thermalSetConfig(thermal.config.byte);
	powerSleep2(0x40);
	thermalSetRate(thermal.rate);
	thermalReadTemp(&temperature);
}

/* data[0] is the register */
unsigned char thermalWrite(unsigned char *data, unsigned char length)
{
	unsigned char status;
	ER ercd;
	FLGPTN ptn;

	if (deviceVersions.thermal[0] == 0xff)
		return 0xff;
	thermalIica.rxData = NULL;
	thermalIica.rxCount = 0;
	thermalIica.txData = data;
	thermalIica.txCount = length;
	thermalIica.address = THERMAL_ADDRESS;
	thermalIica.status = &status;
	clr_flg(4, ~0x10);
	iicaStart(&thermalIica);
	ercd = twai_flg(4, 0x10, TWF_ORW, &ptn, 1);
	if (ercd == E_OK) {
		if (ptn & 0x10)
			clr_flg(4, ~0x10);
	} else if (ercd == E_TMOUT) {
		status = 0x10;
	}
	return status;
}

/* a register's length bytes; with no sensor 0 */
unsigned char thermalRead(unsigned char *reg, unsigned char *data, unsigned char length)
{
	unsigned char status;
	ER ercd;
	FLGPTN ptn;

	if (deviceVersions.thermal[0] == 0xff) {
		*data = 0;
		return 0xff;
	}
	thermalIica.rxData = data;
	thermalIica.rxCount = length;
	thermalIica.txData = reg;
	thermalIica.txCount = 1;
	thermalIica.address = THERMAL_ADDRESS;
	thermalIica.status = &status;
	clr_flg(4, ~0x10);
	iicaStart(&thermalIica);
	ercd = twai_flg(4, 0x10, TWF_ORW, &ptn, 1);
	if (ercd == E_OK) {
		if (ptn & 0x10)
			clr_flg(4, ~0x10);
	} else if (ercd == E_TMOUT) {
		status = 0x10;
	}
	return status;
}

int thermalReadTemp(signed char *temperature)
{
	unsigned char status;

	status = thermalReadLimit(temperature, THERMAL_REG_LOCAL);
	if (status == 0)
		thermal.temperature = *temperature;
	return status;
}

/* in 0.1 K */
int thermalReadKelvin(unsigned int *temperature)
{
	unsigned char status;
	signed char celsius;

	status = thermalReadTemp(&celsius);
	if (status == 0)
		*temperature = (int)celsius * 10 + 2732;
	return status;
}

int thermalReadStatus(unsigned char *status)
{
	unsigned char reg = THERMAL_REG_STATUS;

	return thermalRead(&reg, status, 1);
}

int thermalReadConfig(unsigned char *config)
{
	unsigned char status;
	unsigned char reg = THERMAL_REG_CONFIG;

	status = thermalRead(&reg, config, 1);
	if (status == 0)
		thermal.config.byte = *config;
	return status;
}

int thermalSetConfig(unsigned char config)
{
	unsigned char status;

	elmoCommand[0] = 0x09;
	elmoCommand[1] = config;
	status = thermalWrite(elmoCommand, 2);
	if (status == 0)
		thermal.config.byte = config;
	return status;
}

int thermalReadRate(unsigned char *rate)
{
	unsigned char reg = THERMAL_REG_RATE;

	return thermalRead(&reg, rate, 1);
}

int thermalSetRate(unsigned char rate)
{
	if (rate >= 0x0b)
		return 0xfe;
	elmoCommand[0] = 0x0a;
	elmoCommand[1] = rate;
	return thermalWrite(elmoCommand, 2);
}

int thermalReadHigh(signed char *limit)
{
	return thermalReadLimit(limit, THERMAL_REG_HIGH);
}

int thermalSetHigh(signed char limit)
{
	return thermalWriteLimit(limit, 0x0b);
}

int thermalReadLow(signed char *limit)
{
	return thermalReadLimit(limit, THERMAL_REG_LOW);
}

int thermalSetLow(signed char limit)
{
	return thermalWriteLimit(limit, 0x0c);
}

int thermalReadTherm(signed char *limit)
{
	return thermalReadLimit(limit, THERMAL_REG_THERM);
}

/* Sony's writes the low limit's register, not the THERM limit's */
int thermalSetTherm(signed char limit)
{
	return thermalWriteLimit(limit, 0x0c);
}

int thermalReadHysteresis(unsigned char *hysteresis)
{
	unsigned char reg = THERMAL_REG_HYSTERESIS;

	return thermalRead(&reg, hysteresis, 1);
}

int thermalSetHysteresis(unsigned char hysteresis)
{
	elmoCommand[0] = THERMAL_REG_HYSTERESIS;
	elmoCommand[1] = hysteresis;
	return thermalWrite(elmoCommand, 2);
}

int thermalReadConsecutive(unsigned char *consecutive)
{
	unsigned char reg = THERMAL_REG_CONSECUTIVE;

	return thermalRead(&reg, consecutive, 1);
}

/* the consecutive alerts: 1, 2, 3 or 4 (bits 1-3 0, 1, 3 or 7) */
int thermalSetConsecutive(unsigned char consecutive)
{
	unsigned char bits;

	bits = consecutive & 0x0e;
	if (bits != 0x00 && bits != 0x02 && bits != 0x06 && bits != 0x0e)
		return 0xfe;
	elmoCommand[0] = THERMAL_REG_CONSECUTIVE;
	elmoCommand[1] = consecutive;
	return thermalWrite(elmoCommand, 2);
}

/* with no sensor 0xff */
int thermalReadManufacturer(unsigned char *id)
{
	unsigned char reg = THERMAL_REG_MANUFACTURER;

	if (deviceVersions.thermal[0] == 0xff) {
		*id = 0xff;
		return 0;
	}
	return thermalRead(&reg, id, 1);
}

int thermalReadDevice(unsigned char *id)
{
	unsigned char reg = THERMAL_REG_DEVICE;

	if (deviceVersions.thermal[0] == 0xff) {
		*id = 0xff;
		return 0;
	}
	return thermalRead(&reg, id, 1);
}

/* a temperature register, in degrees C */
unsigned char thermalReadLimit(signed char *limit, unsigned char reg)
{
	unsigned char status;
	unsigned char number;

	number = reg;
	status = thermalRead(&number, (unsigned char *)limit, 1);
	if (status == 0) {
		if (thermal.config.bits.range)
			*limit -= 0x40;
	}
	return status;
}

/* from -40 (0 in the standard range) to 125 degrees C */
unsigned char thermalWriteLimit(signed char limit, unsigned char reg)
{
	if (thermal.config.bits.range) {
		if (limit < -40 || limit >= 126)
			return 0xfe;
	} else if (limit < 0 || limit >= 126) {
		return 0xfe;
	}
	elmoCommand[0] = reg;
	if (thermal.config.bits.range)
		elmoCommand[1] = limit + 0x40;
	else
		elmoCommand[1] = limit;
	return thermalWrite(elmoCommand, 2);
}

#endif

/* a temperature read: the peaks, then the mode's statistics in its system states */
void thermalStatsUpdate(signed char temperature)
{
	unsigned int state;
	int result;
	unsigned char size;

	state = stateGet();
	if (temperature > thermal.sessionPeak) {
		thermal.sessionPeak = temperature;
		if (thermal.peak < -40 || thermal.peak >= 126) {
			result = (unsigned char)logReadData(0x1c, &thermal.peak, &size);
			if (result != 0) {
			}
		}
		if (thermal.peak >= -40 && thermal.peak < 126 && temperature > thermal.peak) {
			thermal.peak = temperature;
			logEventValue(0x1c, temperature);
		}
	}
	switch (thermal.mode) {
	case THERMAL_MODE_OFF:
		break;
	case THERMAL_MODE_ON:
		if (state == STATE_OFF || state == STATE_SUSPEND)
			thermalStatsCount(temperature, &thermal.stats[0]);
		break;
	case THERMAL_MODE_STANDBY:
		if (state == STATE_RESUME)
			thermalStatsCount(temperature, &thermal.stats[1]);
		break;
	}
}

void thermalStatsCount(signed char temperature, struct ThermalStats *stats)
{
	switch (stats->state) {
	case 0:
		stats->state = 1;
		stats->samples = 0;
		/* fall through */
	case 1:
		stats->ticks++;
		if (stats->ticks >= stats->limit || stats->samples >= thermal.maxSamples) {
			stats->state = 2;
			break;
		}
		if (stats->ticks >= stats->start && thermal.countdown == 0) {
			stats->samples++;
			stats->sum += temperature;
			stats->average = (long)stats->sum / stats->samples;
			if (stats->min > temperature || stats->min == THERMAL_NONE)
				stats->min = temperature;
			if (temperature > stats->max || stats->max == THERMAL_NONE)
				stats->max = temperature;
		}
		break;
	case 2:
		break;
	}
	if (thermal.countdown == 0)
		thermal.countdown = thermal.period;
	thermal.countdown--;
}

void thermalStatsInit(void)
{
	thermalStatsClear(7);
	thermal.mode = THERMAL_MODE_OFF;
	thermal.period = configGet()->thermalPeriod;
	thermal.maxSamples = configGet()->thermalMaxSamples;
	thermal.stats[0].start = configGet()->thermalStart0;
	thermal.stats[0].limit = configGet()->thermalLimit0;
	thermal.stats[0].threshold = configGet()->thermalThreshold0;
	thermal.stats[1].start = configGet()->thermalStart1;
	thermal.stats[1].limit = configGet()->thermalLimit1;
	thermal.stats[1].threshold = configGet()->thermalThreshold1;
	thermal.peak = THERMAL_NONE;
}

/* mask bit 0: the session's peak, bit 1: stats[0], bit 2: stats[1] */
void thermalStatsClear(unsigned char mask)
{
	if (mask & 1)
		thermal.sessionPeak = THERMAL_NONE;
	if (mask & 2) {
		thermal.stats[0].ticks = 0;
		thermal.stats[0].state = 0;
		thermal.stats[0].min = THERMAL_NONE;
		thermal.stats[0].max = THERMAL_NONE;
		thermal.stats[0].average = THERMAL_NONE;
		thermal.stats[0].sum = 0;
	}
	if (mask & 4) {
		thermal.stats[1].ticks = 0;
		thermal.stats[1].state = 0;
		thermal.stats[1].min = THERMAL_NONE;
		thermal.stats[1].max = THERMAL_NONE;
		thermal.stats[1].average = THERMAL_NONE;
		thermal.stats[1].sum = 0;
	}
	thermal.countdown = 0;
}

/* serial command 0x01e0's 16 bytes: four groups of a flag (1: no value) and three values */
void thermalStatsGet(struct ThermalReport *report)
{
	if (thermal.sessionPeak != THERMAL_NONE) {
		report->invalid = 0;
		report->peak = thermal.sessionPeak;
	} else {
		report->invalid = 1;
		report->peak = THERMAL_NONE;
	}
	report->reserved2 = 0;
	report->reserved3 = 0;
	if (thermal.stats[0].state != 0
	    && thermal.stats[0].samples >= thermal.stats[0].threshold) {
		report->on.invalid = 0;
		report->on.min = thermal.stats[0].min;
		report->on.max = thermal.stats[0].max;
		report->on.average = thermal.stats[0].average;
	} else {
		report->on.invalid = 1;
		report->on.min = THERMAL_NONE;
		report->on.max = THERMAL_NONE;
		report->on.average = THERMAL_NONE;
	}
	if (thermal.stats[1].state != 0
	    && thermal.stats[1].samples >= thermal.stats[1].threshold) {
		report->standby.invalid = 0;
		report->standby.min = thermal.stats[1].min;
		report->standby.max = thermal.stats[1].max;
		report->standby.average = thermal.stats[1].average;
	} else {
		report->standby.invalid = 1;
		report->standby.min = THERMAL_NONE;
		report->standby.max = THERMAL_NONE;
		report->standby.average = THERMAL_NONE;
	}
	report->reserved12 = 0;
	report->reserved13 = 0;
	report->reserved14 = 0;
	report->reserved15 = 0;
}

/* from the power jobs: a new mode restarts both statistics' ticks */
void thermalSetMode(unsigned char mode)
{
	if (thermal.mode != mode) {
		thermal.stats[0].ticks = 0;
		thermal.stats[1].ticks = 0;
		thermal.countdown = 0;
		thermal.mode = mode;
	}
}
