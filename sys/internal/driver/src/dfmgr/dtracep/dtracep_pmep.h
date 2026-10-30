/*
 * The PMEP channel (Sony's dtracep_pmep.c): the host's communication
 * processor values DTrace's cpch0..cpch7 variables read.
 */
#pragma once

#include <scetypes.h>

int sceDtracepPmepStartPolling(void *values);
int sceDtracepPmepStopPolling(void);
