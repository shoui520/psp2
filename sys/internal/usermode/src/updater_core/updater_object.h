/*
 * The root of updater_core's classes: a virtual destructor and nothing else (its out-of-line
 * copy is at 0x81015970 in cui_setupper). The name is ours.
 */
#ifndef UPDATER_CORE_UPDATER_OBJECT_H
#define UPDATER_CORE_UPDATER_OBJECT_H

class UpdaterObject {
public:
	virtual ~UpdaterObject() {}
};

#endif
