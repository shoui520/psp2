/*
 * Build selection: the first_loader build, named by the BCD build date it carries in
 * its last word (FL_BUILD_DATE), given by the Makefile from fw/<date>.mk.
 */
#ifndef FIRST_LOADER_H
#define FIRST_LOADER_H

#ifndef FL_BUILD_DATE
#error "FL_BUILD_DATE (build, YYYYMMDD in hex) not set"
#endif

#define FL_SINCE(date)	(FL_BUILD_DATE >= (date))

#endif
