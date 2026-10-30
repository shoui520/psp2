/*
 * The profile provider's probe firing (Sony's dtrace/sce/sce_profile.c): the
 * interrupted PC, kernel or user, as the probe's arguments.
 */
#pragma once

#include <sys/dtrace.h>

void profile_fire_probe(dtrace_id_t id, hrtime_t late, int frames);
