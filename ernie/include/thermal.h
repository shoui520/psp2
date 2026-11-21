/*
 * DOL-1001's temperature sensor (app/thermal.c): a TMP451-like sensor on the I2C bus, its
 * local temperature read by task 5 and kept as statistics for the host (serial commands
 * 0x01e0 and 0x01e1).
 */
#ifndef ERNIE_THERMAL_H
#define ERNIE_THERMAL_H

#include "ernie.h"
#include "analog.h"

#define THERMAL_ADDRESS		0x98	/* the sensor's, shifted */
#define THERMAL_ID		0x1a	/* deviceVersions.thermal[0] when it answered; 0xff none */

/* its registers: read at one number, written at another */
#define THERMAL_REG_LOCAL	0x00	/* the local temperature */
#define THERMAL_REG_STATUS	0x02
#define THERMAL_REG_CONFIG	0x03	/* written at 0x09 */
#define THERMAL_REG_RATE	0x04	/* the conversion rate, written at 0x0a */
#define THERMAL_REG_HIGH	0x05	/* the local high limit, written at 0x0b */
#define THERMAL_REG_LOW		0x06	/* the local low limit, written at 0x0c */
#define THERMAL_REG_THERM	0x20	/* the local THERM limit */
#define THERMAL_REG_HYSTERESIS	0x21
#define THERMAL_REG_CONSECUTIVE	0x22
#define THERMAL_REG_MANUFACTURER 0xfe
#define THERMAL_REG_DEVICE	0xff

/* the configuration register */
union ThermalConfig {
	unsigned char	byte;
	struct {
		unsigned char	reserved0:2;
		unsigned char	range:1;	/* the extended range: a temperature is stored +0x40 */
		unsigned char	reserved3:3;
		unsigned char	stop:1;		/* conversions stopped */
		unsigned char	alertMask:1;
	} bits;
};
#define THERMAL_NONE		(-128)	/* a statistic with no sample yet */

/* the modes thermalSetMode takes from the power jobs: which statistics run */
#define THERMAL_MODE_OFF	0
#define THERMAL_MODE_ON		1	/* stats[0], in system states 4 and 8 */
#define THERMAL_MODE_STANDBY	2	/* stats[1], in system state 0x20 */

/* a mode's statistics: from start ticks after the mode began, a sample each period ticks,
   until limit ticks or the samples' maximum */
struct ThermalStats {
	unsigned char	state;		/* 0 not started, 1 sampling, 2 done */
	signed char	min;
	signed char	max;
	signed char	average;
	unsigned long	sum;
	unsigned int	ticks;
	unsigned int	start;
	unsigned int	limit;
	unsigned int	samples;
	unsigned int	threshold;	/* the samples before the host is answered them */
};

/* thermalStatsGet's answer to serial command 0x01e0: each part's invalid is 1 (and its
   values THERMAL_NONE) while it has no samples */
struct ThermalReportStats {
	unsigned char	invalid;
	signed char	min;
	signed char	max;
	signed char	average;
};

struct ThermalReport {
	unsigned char	invalid;
	signed char	peak;		/* thermal.sessionPeak */
	unsigned char	reserved2;
	unsigned char	reserved3;
	struct ThermalReportStats on;	/* thermal.stats[0] */
	struct ThermalReportStats standby; /* thermal.stats[1] */
	unsigned char	reserved12;
	unsigned char	reserved13;
	unsigned char	reserved14;
	unsigned char	reserved15;
};

struct Thermal {
#ifndef ERNIE_UPDATER	/* the PS TV's updater keeps only the sensor's */
	signed char	peak;		/* the highest ever, kept in log record 0x1c */
	signed char	sessionPeak;	/* the highest since the statistics were cleared */
	unsigned char	mode;		/* THERMAL_MODE_* */
	unsigned char	countdown;	/* ticks to the next sample */
	unsigned char	period;
	unsigned int	maxSamples;
	struct ThermalStats stats[2];
#endif
#if !ANALOG_THERMISTOR
	signed char	temperature;	/* the last read */
	union ThermalConfig config;	/* as written to the sensor */
	unsigned char	rate;
#endif
};

extern struct Thermal thermal;

extern void thermalPoll(void);
extern void thermalInit(void);
extern void thermalStart(void);
extern unsigned char thermalWrite(unsigned char *data, unsigned char length);
extern unsigned char thermalRead(unsigned char *reg, unsigned char *data, unsigned char length);
extern int thermalReadTemp(signed char *temperature);
extern int thermalReadKelvin(unsigned int *temperature);
extern int thermalReadStatus(unsigned char *status);
extern int thermalReadConfig(unsigned char *config);
extern int thermalSetConfig(unsigned char config);
extern int thermalReadRate(unsigned char *rate);
extern int thermalSetRate(unsigned char rate);
extern int thermalReadHigh(signed char *limit);
extern int thermalSetHigh(signed char limit);
extern int thermalReadLow(signed char *limit);
extern int thermalSetLow(signed char limit);
extern int thermalReadTherm(signed char *limit);
extern int thermalSetTherm(signed char limit);
extern int thermalReadHysteresis(unsigned char *hysteresis);
extern int thermalSetHysteresis(unsigned char hysteresis);
extern int thermalReadConsecutive(unsigned char *consecutive);
extern int thermalSetConsecutive(unsigned char consecutive);
extern int thermalReadManufacturer(unsigned char *id);
extern int thermalReadDevice(unsigned char *id);
extern unsigned char thermalReadLimit(signed char *limit, unsigned char reg);
extern unsigned char thermalWriteLimit(signed char limit, unsigned char reg);
extern void thermalStatsUpdate(signed char temperature);
extern void thermalStatsCount(signed char temperature, struct ThermalStats *stats);
extern void thermalStatsInit(void);
extern void thermalStatsClear(unsigned char mask);
extern void thermalStatsGet(struct ThermalReport *report);
extern void thermalSetMode(unsigned char mode);

#endif
